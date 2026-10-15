# Xui widget implementation plan

Local working plan. Durable history lives in `tracking.md`; this file tracks phase status only.

## Phase status

| Phase | Scope | Status |
| --- | --- | --- |
| 1 — Inputs & display | RadioGroup, Switch, ToggleButton, NumberField, SearchField, Label, Badge, Separator/Divider, Spinner, Image, readonly state | Implemented; compile-verified (MSVC /W4 /WX) before demo edits; interactions not yet run |
| 1b — Showcase demo | Every supported widget visible/interactive with live value readouts | Written; not yet compiled/run |
| 1c — Input polish | Blinking caret, whitespace visualization API + demo toggle, Tab behavior documented | Written; not yet compiled/run |
| 1d — Second widget wave | SegmentedControl, Stepper, Rating, Breadcrumb, KeybindField, DatePicker, indeterminate progress, Toolbar/StatusBar + Blender grey demo theme | Written; not yet compiled/run |
| 2 — Scrolling & containers | ScrollView (wheel + clip in paint and hit-test), Grid, GroupBox/Panel title+collapsible → Accordion | Not started; highest regression risk (clipping/z-order) |
| 3 — Overlays & navigation | Shared overlay layer refactor, then Popover, Modal/Dialog, MenuBar/Menu, ContextMenu, Toast | Not started |
| 4 — Data display | ListView, TreeView, Table (virtualized rows on ScrollView) | Not started; depends on Phase 2 |

## Known gaps carried forward

- General PNG decoder for `Image` (current reader accepts only this project's writer output).
- Per-row styling for RadioGroup/Dropdown virtual rows.
- No animation/transition system; Spinner + tooltip/FPS timers are the only time-driven paint.
- Visible GLFW smoke pass and stable `--profile` benchmark remain open (see tracking.md).
- `Grid`/`Scroll` layout kinds exist in the layout engine but are not materialized by the runtime yet.

## Verification checklist for the current work (run by owner)

1. `cmake --build build --config Release --target xui_tests` then run `build/tests/Release/xui_tests.exe` — expect all tests passing, including showcase parse/build over the new content.
2. `cmake --build build --config Release --target xui_glfw_demo` then run `build/examples/Release/xui_glfw_demo.exe` — three tabs, every control, readouts updating.
3. `xui_glfw_demo.exe --profile` plus a visible run with F3 — FPS > 60, spinner animates, resize is smooth.
