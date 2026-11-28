#!/usr/bin/env python3
"""Count registered C++ tests, verify or refresh the roadmap summary.

The roadmap records one canonical test total. CI runs ``--check`` so the
documented count cannot drift from the registered suite; contributors run
``--update`` after adding or removing tests.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "plan" / "plan.md"
TEST_PATTERN = re.compile(r"^\s*TEST\s*\(", re.MULTILINE)
SUMMARY_PATTERN = re.compile(r"^Automated test cases: \*\*(\d+)\*\*$", re.MULTILINE)


def count_tests() -> int:
    return sum(
        len(TEST_PATTERN.findall(path.read_text(encoding="utf-8")))
        for path in (ROOT / "tests").glob("*.cpp")
    )


def documented_count() -> int:
    text = PLAN.read_text(encoding="utf-8")
    matches = SUMMARY_PATTERN.findall(text)
    if len(matches) != 1:
        raise SystemExit(
            "plan/plan.md must contain exactly one 'Automated test cases: **N**' summary line"
        )
    return int(matches[0])


def write_count(count: int) -> None:
    text = PLAN.read_text(encoding="utf-8")
    updated, changes = SUMMARY_PATTERN.subn(f"Automated test cases: **{count}**", text, count=1)
    if changes != 1:
        raise SystemExit("plan/plan.md summary line could not be replaced")
    PLAN.write_text(updated, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--update", action="store_true", help="refresh the count in plan/plan.md")
    group.add_argument("--check", action="store_true", help="fail when plan/plan.md is stale")
    args = parser.parse_args()

    count = count_tests()
    documented = documented_count()
    if args.update:
        write_count(count)
        print(f"{count} registered tests; updated plan/plan.md")
        return 0
    if args.check and documented != count:
        print(
            f"plan/plan.md documents {documented} tests but {count} are registered; "
            "run `python tools/count_tests.py --update`"
        )
        return 1
    print(f"{count} registered tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
