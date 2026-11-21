# Fuzz harnesses

These deterministic harnesses complement the unit suite. They are ordinary
executables, so they run on Windows, Linux, and macOS without a fuzzing runtime;
ASan/UBSan builds are recommended when exercising them.

Build with `-DXUI_BUILD_FUZZERS=ON` (the default), then run:

```sh
fuzz/fuzz_parsers
fuzz/fuzz_layout [firstSeed] [count] [stepsPerCase]
fuzz/fuzz_paint [firstSeed] [count] [primsPerScene]
```

- `fuzz_parsers` mutates valid `.xui` and `.xss` inputs and checks parser
  recovery and sanitizer cleanliness.
- `fuzz_layout` checks that incremental layout matches a full layout for every
  generated seed.
- `fuzz_paint` checks that draw-command reordering preserves rendered pixels.

A failure prints the seed needed to reproduce it. `fuzz_parsers` also writes the
last input to `fuzz_last.txt`, which is ignored by Git.
