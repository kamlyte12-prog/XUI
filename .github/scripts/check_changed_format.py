#!/usr/bin/env python3
"""Check (or apply) clang-format for the C++ lines a change introduces.

The repository predates `.clang-format`: existing code keeps its own spacing, so
a whole-tree format check is not usable. `clang-format-diff` has the opposite
problem - it reformats the whole block around a changed line, so it reports
fixes for untouched legacy lines and, once those are applied, keeps pulling in
neighbours.

This script keeps the intended gate - clang-format 22.1.3 over the lines a
patch adds - and bounds it to code the change owns:

* a rewrite group made only of changed lines must be formatted as clang-format
  produces it;
* a group that also covers untouched legacy lines is not rewritten wholesale.
  Its changed lines are still checked: each must keep its text (a split or join
  means the line is not formatted) and its indentation.

Usage:
    check_changed_format.py <range> [--apply]

`<range>` is any revision range understood by `git diff`, for example
`origin/master...HEAD`. `--apply` rewrites the groups the change owns and
prints anything left for manual attention. Without it, the script reports the
offending lines and exits 1.
"""

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path

# C++ sources owned by this project. Vendored dependencies keep their own style.
PATHSPEC = ["*.cpp", "*.hpp", "*.h", ":(exclude)libs/**"]
HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


def git(*args: str) -> str:
    result = subprocess.run(["git", *args], capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"git {' '.join(args)} failed")
    return result.stdout


def changed_ranges(path: str, revision_range: str) -> list[tuple[int, int]]:
    """New-file line ranges touched by the diff, deletions included."""
    ranges: list[tuple[int, int]] = []
    for line in git("diff", "-U0", "-M", revision_range, "--", path).splitlines():
        match = HUNK.match(line)
        if not match:
            continue
        start = int(match.group(1))
        count = int(match.group(2)) if match.group(2) is not None else 1
        ranges.append((start, start + max(count, 1) - 1))
    return ranges


def clang_format(path: str, text: str, ranges: list[tuple[int, int]]) -> str:
    args = [f"--lines={start}:{end}" for start, end in ranges]
    result = subprocess.run(
        ["clang-format", "--style=file", f"--assume-filename={path}", *args],
        input=text,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"clang-format failed for {path}")
    return result.stdout


def owns_group(opcode, changed: set[int]) -> bool:
    """True when every original line of this rewrite belongs to the patch."""
    tag, i1, i2, _, _ = opcode
    if tag == "equal":
        return False
    if i1 == i2:
        # Pure insertion: it extends the line before it, or the one at the point.
        return i1 in changed or i1 + 1 in changed
    return all(number in changed for number in range(i1 + 1, i2 + 1))


def indent_of(line: str) -> str:
    return line[: len(line) - len(line.lstrip())]


def mixed_violations(source: list[str], i1: int, i2: int, target: list[str], j1: int, j2: int,
                     changed: set[int]) -> list[str]:
    """Changed lines inside a mixed group that clang-format would still rewrite."""
    candidates = [(target[n].strip(), indent_of(target[n])) for n in range(j1, j2)]
    problems = []
    for number in range(i1 + 1, i2 + 1):
        if number not in changed:
            continue
        text = source[number - 1].strip()
        if not text:
            continue
        for index, (candidate, indent) in enumerate(candidates):
            if candidate == text:
                if indent != indent_of(source[number - 1]):
                    problems.append(f"line {number}: indentation is not clang-format style")
                del candidates[index]
                break
        else:
            problems.append(f"line {number}: clang-format splits or rejoins this line")
    return problems


def process(path: str, original: str, ranges: list[tuple[int, int]]) -> tuple[str, list[str]]:
    """Return the file with owned formatting applied, plus problems to fix by hand."""
    changed = {number for start, end in ranges for number in range(start, end + 1)}
    formatted = clang_format(path, original, ranges)
    source = original.splitlines()
    target = formatted.splitlines()
    merged: list[str] = []
    problems: list[str] = []
    for opcode in difflib.SequenceMatcher(None, source, target, autojunk=False).get_opcodes():
        tag, i1, i2, j1, j2 = opcode
        if tag == "equal":
            merged.extend(source[i1:i2])
        elif owns_group(opcode, changed):
            merged.extend(target[j1:j2])
        else:
            merged.extend(source[i1:i2])
            problems.extend(mixed_violations(source, i1, i2, target, j1, j2, changed))
    text = "\n".join(merged) + ("\n" if original.endswith("\n") else "") if merged else original
    return text, problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("range", help="git revision range, e.g. origin/master...HEAD")
    parser.add_argument("--apply", action="store_true", help="write the formatting instead of failing")
    args = parser.parse_args()

    files = [name for name in git("diff", "--name-only", "-M", args.range, "--", *PATHSPEC).splitlines() if name]
    failures = 0
    for name in files:
        path = Path(name)
        ranges = changed_ranges(name, args.range)
        if not ranges or not path.is_file():
            continue
        raw = path.read_bytes()
        crlf = b"\r\n" in raw
        original = raw.decode("utf-8").replace("\r\n", "\n")
        expected, problems = process(name, original, ranges)
        changed_by_format = expected != original
        if args.apply and changed_by_format:
            text = expected.replace("\n", "\r\n") if crlf else expected
            path.write_bytes(text.encode("utf-8"))
            print(f"formatted {name}")
        if changed_by_format and not args.apply:
            failures += 1
            sys.stdout.writelines(
                difflib.unified_diff(
                    original.splitlines(keepends=True),
                    expected.splitlines(keepends=True),
                    fromfile=f"{name} (changed lines)",
                    tofile=f"{name} (clang-format 22.1.3)",
                )
            )
        if problems:
            failures += 1
            print(f"{name}:")
            for problem in problems:
                print(f"  {problem}")

    if failures:
        print(f"{failures} file(s) still need formatting on changed lines.", file=sys.stderr)
        return 1
    print("Formatting applied." if args.apply else "Changed C++ lines are clang-format clean.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
