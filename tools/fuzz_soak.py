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
    rounds = 0
    while time.monotonic() < deadline:
        for executable, arguments in targets:
            if not executable.exists():
                raise SystemExit(f"missing fuzzer executable: {executable}")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            result = subprocess.run([str(executable), *arguments], cwd=root, timeout=remaining)
            if result.returncode:
                return result.returncode
        rounds += 1
    print(f"fuzz soak passed: {rounds} complete rotations in {args.seconds}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
