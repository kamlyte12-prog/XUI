#!/usr/bin/env python3
"""Run the deterministic fuzz harnesses continuously for a timed CI soak."""

from __future__ import annotations

import argparse
import subprocess
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=180)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    args = parser.parse_args()
    if args.seconds < 1:
        parser.error("--seconds must be positive")

    root = Path(__file__).resolve().parents[1]
    build = (root / args.build_dir).resolve()
    targets = [
        (build / "fuzz" / "fuzz_parsers", ["5000", "12345"]),
        (build / "fuzz" / "fuzz_layout", ["1", "1500", "10"]),
        (build / "fuzz" / "fuzz_paint", ["1", "800", "50"]),
    ]
    deadline = time.monotonic() + args.seconds
    # A single rotation takes tens of seconds under the sanitizers. Cap each run
    # well above that so a genuine hang still fails the job, while a timeout that
    # lands on the soak deadline is just the timed soak stopping mid-rotation.
    run_timeout = 120.0
    rounds = 0
    while time.monotonic() < deadline:
        complete_rotation = True
        for executable, arguments in targets:
            if not executable.exists():
                raise SystemExit(f"missing fuzzer executable: {executable}")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                complete_rotation = False
                break
            try:
                result = subprocess.run(
                    [str(executable), *arguments], cwd=root, timeout=min(remaining, run_timeout)
                )
            except subprocess.TimeoutExpired:
                if time.monotonic() >= deadline:
                    complete_rotation = False
                    break
                raise SystemExit(f"{executable.name} exceeded {run_timeout:.0f}s without finishing")
            if result.returncode:
                return result.returncode
        if not complete_rotation:
            break
        rounds += 1
    print(f"fuzz soak passed: {rounds} complete rotations in {args.seconds}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
