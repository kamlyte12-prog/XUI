# Xui widget implementation plan

Local working plan. Durable history lives in `tracking.md`; this file tracks phase status only.

## Phase status

| Phase | Scope | Status |
| --- | --- | --- |
| 1 — Inputs & display | RadioGroup, Switch, ToggleButton, NumberField, SearchField, Label, Badge, Separator/Divider, Spinner, Image, readonly state | Implemented; compile-verified (MSVC /W4 /WX) before demo edits; interactions not yet run |
| 1b — Showcase demo | Runtime-supported widgets and value readouts | Built and parser/runtime-test verified; visible interaction pass still open |
| 1c — Input polish | Blinking caret, whitespace visualization API + demo toggle, Tab behavior documented | Compiled and headless-test verified; physical input/visual pass still open |
| 1d — Second widget wave | SegmentedControl, Stepper, Rating, Breadcrumb, KeybindField, DatePicker, indeterminate progress, Toolbar/StatusBar + Blender grey demo theme | Compiled and headless-test verified; visible interaction pass still open |
| 2 — Scrolling & containers | ScrollView (wheel + clip in paint and hit-test), Grid, GroupBox/Panel title+collapsible → Accordion | ScrollView implemented and headless-tested; Grid and titled/collapsible containers pending |
| 3 — Overlays & navigation | Shared overlay layer refactor, then Popover, Modal/Dialog, MenuBar/Menu, ContextMenu, Toast | Not started |
| 4 — Data display | ListView, TreeView, Table (virtualized rows on ScrollView) | Not started; depends on Phase 2 |

## Known gaps carried forward

- General PNG decoder for `Image` (current reader accepts only this project's writer output).
- Per-row styling for RadioGroup/Dropdown virtual rows.
- No general animation/transition system; Spinner, tooltip/FPS refresh, and ScrollView smooth-scroll timers are the current time-driven paths.
- Visible GLFW smoke pass and repeatable `--profile` benchmark remain open (see tracking.md); one bounded hidden run currently reports 805 FPS.
- Grid is not materialized by the runtime. ScrollView support is implemented, but touch/momentum scrolling, accessibility, and generic `overflow` clipping remain open.

## Verification checklist for the current work (run by owner)

1. `cmake --build build --config Release --target xui_tests` then run `build/tests/Release/xui_tests.exe` — includes the nested scroll, wheel, clipping, keyboard, programmatic offset, and showcase checks.
2. `cmake --build build --config Release --target xui_glfw_demo` then run `build/examples/Release/xui_glfw_demo.exe` — scrolling tab includes vertical/horizontal/nested and hidden-scrollbar cases.
3. `xui_glfw_demo.exe --profile` records a hidden uncapped frame-loop ceiling; verify a visible scroll/resize/input session separately before claiming interactive >60 FPS.

## Latest measured layout and scrolling results

Windows Release `bench_layout` (938 nodes): cold build 0.30 ms, first layout 0.120 ms, full relayout 103.8 μs, resize 99.8 μs, one leaf content change 2.4 μs, and clean idle layout effectively 0 μs. The pre-scroll run was 107.9 μs, 102.3 μs, 2.2 μs, and effectively 0 μs respectively; this small variance does not support a solver-speedup claim. The 1,000-child scroll microbenchmark measured offset update plus descendant absolute-rect lookup at 0.008 μs per iteration with zero nodes arranged.

Windows Release hidden `xui_glfw_demo.exe --profile`: 4,025 frames / 5.00004 s / 804.994 FPS. This exceeds 60 in an uncapped hidden continuous loop, but does not verify visible frame pacing or FPS specifically during wheel/drag/resize.

The retained solver's dirty-flag invalidation and constraint-keyed measurement cache were already present and are preserved. All automated checks pass (**154 tests, 0 failures**). A visible interactive scroll/resize/close smoke pass and non-Windows build remain unverified.
