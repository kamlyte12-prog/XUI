# Xui widget implementation plan

Local working plan. Durable history lives in `tracking.md`; this file tracks phase status only.

## Live resize — 2026-10-15 (this session)

- **Root cause:** Windows interactive resize is a modal OS loop inside `glfwPollEvents`/`glfwWaitEvents`; the facade's frame loop never runs mid-drag, so all facade-side resize handling was unreachable. Fixed with an optional `Platform::setLiveResizeHook` — GLFW calls it from `onSize`, and `Xui::renderFrame()` (extracted render path: resize backbuffer → layout → paint → present) renders every step. Animated widgets stay live because the hook runs the full paint path.
- **Pending owner verification:** continuous border-drag reflow on both demos, animated widgets during drag, 158/158 suite.

## Cursor fix + blue theme — 2026-10-15 (this session)

- **Cursor mapping fixed** in `UiRuntime::cursorAt`: `Hand` for every clickable control (buttons, checks, switch/toggle, radios, dropdown/color/date pickers, segmented, rating, breadcrumb, stepper, keybind, tabs) and the SearchField clear glyph; `ResizeEW` on sliders and horizontal split gutters; `ResizeNS` on vertical gutters; `IBeam` only in editable, non-readonly fields; `Arrow` for disabled controls, panels, and text. GLFW backend already mapped every shape — no backend change.
- **Blue theme:** `showcase.xss` rewritten (deep-navy surfaces, accent `#4d9fff`, consistent 4/6/8 px radii, accented tooltips/selected tabs). Selector set and `.xui` structure/ids deliberately unchanged — the suite hard-codes them and a structural rewrite could not be verified here.
- **Pending owner verification:** build + 158/158 suite, then a visible cursor pass per widget class and a palette/contrast check on screen.

## Demo polish delta — 2026-10-15 (this session)

- **Coverage:** the only runtime-supported gap left in the showcase was the password TextBox mode — now demoed as `passphrase` in the Input tab (masked display, length-only title readout).
- **Overlap:** static vertical budget of all five tabs holds at the test's 1080×760 layout (Controls ≈ 535 / 584 px is the tightest; Layout's Split flexes to ≈ 322 > the suite's 300 floor). Sequential rows cannot overlap; the input row's three fields motivated the 900×640 minimum window size in `glfw_demo.cpp`.
- **Theme:** radius consistency pass only (SearchField/Stepper → 4 px, Image → 6 px); the committed blue-grey palette is left intact.
- **Pending the owner's visible pass:** password masking + length readout, row fit at 900×640 minimum, and the pre-existing open visual items (theme coherence on screen, resize feel, close behavior).

## Latest showcase and sizing audit — 2026-10-15

- **Demo packaging cleanup:** `glfw_demo.cpp` loads only `showcase.xui` / `showcase.xss`. CMake now copies only this runtime pair beside `xui_glfw_demo` and `xui_hello`; `cross_platform.*` and `controls.*` stay as source fixtures because tests and docs actively reference them. Rebuilt both demos and `xui_tests`; 158/158 tests pass. The generated output folder now contains only `showcase.*` among XUI/XSS assets.
- **Current implementation:** the showcase theme is now palette-token based outside its token block, with normalized control radii/padding and shared state colors. The Layout tab now includes the runtime-supported `Divider`, `Toolbar`, and `StatusBar` aliases. The five demo tabs collectively list the complete runtime-supported set; parser-only names in `doc/widgets.md` remain intentionally excluded.
- **Root sizing gap fixed:** auto-sized empty controls previously measured as `0×0`, which could collapse themselves and shrink child-measured containers. Runtime preferred content-box sizes now cover blank TextBox/TextArea, Slider, ProgressBar, ColorPicker, and Dropdown. Text fields use current text or placeholder and preserve a useful minimum; layout still adds padding/borders and respects authored constraints. The `Column`/`Row`/`Panel` engine was already content-driven, so no solver behavior was changed.
- **Asset inventory:** `showcase.*` is the only pair copied for runtime demos. `controls.*` and `cross_platform.*` remain in the source tree because tests and docs actively reference them; they are not demo deployment assets.
- **Verified:** Release build succeeded for `xui_tests`, `xui_glfw_demo`, and `xui_hello`; test suite **158/158**, including content-mutation growth and composite-control intrinsic size checks. Latest hidden `--profile` average: `xui_hello` **572.588 FPS**, `xui_glfw_demo` **583.659 FPS**. The initial Debug build failed with MSVC C1902; Release with embedded debug info built successfully.
- **Not visually verified:** the Windows desktop bridge did not provide working app launch/window inspection methods, and an approved shell launch did not leave a discoverable window. Thus visual theme consistency, demo clipping/overlap, live resize, interactions, and normal close remain pending; hidden profile rates do not prove these criteria.

### Immediate next steps

1. Complete a visible GLFW demo pass: inspect all tabs, test controls and closing, and perform a sustained resize with FPS on.
2. If any ordinary-flow overlap/clipping remains, add a minimal authored fixture and trace intrinsic/available sizes before modifying the solver.
3. Continue implementing only runtime-backed widgets; keep schema-only widgets clearly identified as unsupported.

## Current audit delta — 2026-10-15

- **Verified:** Windows MSVC Debug builds of `xui_tests`, `xui_glfw_demo`, and `xui_hello` succeeded with warnings treated as errors. The automated suite passes **157/157**. It covers ordinary row/column placement, percentage and min/max constraints, nested scroll clipping, runtime resize layout, text wrapping, theme palette resolution, and headless widget input.
- **Visible evidence:** the GLFW/OpenGL showcase was captured in the Scrolling tab. The display reported **97 FPS / 5.1 ms**, and text, tabs, horizontal/vertical/nested scroll containers, and viewport clipping were visible without white-box glyphs or ordinary flow siblings overlapping. This is one visible state, not an exhaustive interaction test.
- **FPS diagnosis/fix:** the former 4 FPS / 251.6 ms readout counted event-driven idle waits because the overlay refreshed itself every 250 ms. Enabling the FPS overlay now opts into continuous redraw, making the displayed figure a frame-cadence measurement. This consumes more CPU/GPU while enabled; F3 returns to event-driven idle behavior.
- **Resize status:** source path and unit coverage show GLFW size events reach `Xui::run()`, which resizes the renderer and calls layout using logical client size before paint. The prior once-per-drain event pump is present. A live border-drag test could not complete because the desktop helper lost the window process handle; freeze-free interactive resizing remains **unverified**.
- **Overlap status:** no normal Row/Column overlap was reproduced. Stack/Tabs, explicit grid positions, absolute children, and scroll clipping are intentional. No solver rewrite was made without a failing minimal document; a user-specific overlap fixture remains an open need if the captured scroll state does not reproduce it.
- **Theme status:** `Xui::setTheme(name)` and runtime palette re-resolution are now implemented for authored widgets and generated overlays. Theme-dependent paint/text values switch; layout metrics (`pad`, `gap`, sizes, min/max, margins) are not re-applied after the initial `.xui` precedence merge and must stay theme-invariant for now.
- **Style audit:** composite checkbox/switch/radio/segmented/rating/editable selection/image-placeholder colors now derive from `.xss` values. Fixed contrast marks, semantic picker ramps, virtual dropdown/date rows, tab header geometry, spinner cadence, and clear-glyph geometry remain documented subpart limits; no whole runtime widget type is unstyleable.
- **Performance:** hidden continuous runs before the last small composite tint edits measured 805.2 FPS (`xui_hello`) and 806.9 FPS (`xui_glfw_demo`). Post-edit profile processes exited successfully but yielded no stats text, so the earlier figures are not the final-source result. The visible 97 FPS reading is the only current-source live throughput observation.

### Immediate acceptance work

1. Reopen the rebuilt GLFW demo and perform a sustained resize drag while watching layout and FPS; then test hover, click, typing, scroll routing, and normal close.
2. Repeat the visible smoke test in `xui_hello.exe`.
3. If overlap remains, reduce it to a fixture and log each node's parent content box, resolved width/height, margin/padding, clip, and z-order before changing the solver.
4. Reapply theme-dependent `.xss` layout values without losing explicit `.xui` precedence; add a two-theme geometry test.

## Phase status

| Phase | Scope | Status |
| --- | --- | --- |
| 1 — Inputs & display | RadioGroup, Switch, ToggleButton, NumberField, SearchField, Label, Badge, Separator/Divider, Spinner, Image, readonly state | Implemented; compile-verified (MSVC /W4 /WX) before demo edits; interactions not yet run |
| 1b — Showcase demo | Runtime-supported widgets and value readouts | Full supported widget list now included, including aliases; parser/runtime-test verified; visible interaction pass still open |
| 1c — Input polish | Blinking caret, whitespace visualization API + demo toggle, Tab behavior documented | Compiled and headless-test verified; physical input/visual pass still open |
| 1d — Second widget wave | SegmentedControl, Stepper, Rating, Breadcrumb, KeybindField, DatePicker, indeterminate progress, Toolbar/StatusBar + Blender grey demo theme | Compiled and headless-test verified; visible interaction pass still open |
| 1e — Resize, sizing, picker and text polish | Fair GLFW event drains, percentage min/max constraints, ColorPicker layout/hit testing, TextArea tabs/caret layout, showcase theme | Implemented; Release build and 156 headless tests pass; hidden loop profiles exceed 60 FPS; visible resize/caret/picker visual pass remains open |
| 2 — Scrolling & containers | ScrollView (wheel + clip in paint and hit-test), Grid, GroupBox/Panel title+collapsible → Accordion | ScrollView implemented and headless-tested; Grid and titled/collapsible containers pending |
| 3 — Overlays & navigation | Shared overlay layer refactor, then Popover, Modal/Dialog, MenuBar/Menu, ContextMenu, Toast | Not started |
| 4 — Data display | ListView, TreeView, Table (virtualized rows on ScrollView) | Not started; depends on Phase 2 |

## Known gaps carried forward

- General PNG decoder for `Image` (current reader accepts only this project's writer output).
- Per-row styling for RadioGroup/Dropdown virtual rows.
- No general animation/transition system; Spinner, tooltip/FPS refresh, and ScrollView smooth-scroll timers are the current time-driven paths.
- Visible GLFW smoke pass remains open. Latest bounded hidden Release loop profiles report 574.7 FPS (`xui_hello`) and 585.5 FPS (`xui_glfw_demo`); these do not establish visible resize, open-picker throughput, or Win32 behavior.
- Grid is not materialized by the runtime. ScrollView support is implemented, but touch/momentum scrolling, accessibility, and generic `overflow` clipping remain open.

## Verification checklist for the current work (run by owner)

1. `cmake --build build --config Release --target xui_tests` then run `build/tests/Release/xui_tests.exe` — includes the nested scroll, wheel, clipping, keyboard, programmatic offset, and showcase checks.
2. `cmake --build build --config Release --target xui_glfw_demo` then run `build/examples/Release/xui_glfw_demo.exe` — scrolling tab includes vertical/horizontal/nested and hidden-scrollbar cases.
3. `xui_glfw_demo.exe --profile` records a hidden uncapped frame-loop ceiling; verify a visible scroll/resize/input session separately before claiming interactive >60 FPS.

## Latest measured layout and scrolling results

Windows Release `bench_layout` (938 nodes; 2026-10-15 rerun): cold build 0.32 ms, first layout 0.124 ms, full relayout 111.9 μs, resize 102.5 μs, one leaf content change 2.3 μs, splitter drag frame 97.2 μs, and clean idle layout effectively 0 μs. The 1,000-child scroll microbenchmark measured offset update plus descendant absolute-rect lookup at 0.008 μs per iteration with zero nodes arranged. These numbers do not indicate a solver-speedup claim; the incremental constraints add limited work to the existing solver.

Windows Release hidden continuous-loop profiles (2026-10-15, final rebuild): `xui_hello.exe --profile` 4,184 frames / 7.15079 s / 585.11 FPS; `xui_glfw_demo.exe --profile` 4,197 frames / 7.14915 s / 587.063 FPS. These exceed 60 in the hidden uncapped loop, but do not verify visible frame pacing or FPS specifically during wheel/drag/resize. Both example targets use the GLFW/OpenGL host in this checkout.

The retained solver's dirty-flag invalidation and constraint-keyed measurement cache were already present and are preserved. The latest Release build covers `xui_tests`, `xui_glfw_demo`, `xui_hello`, and `bench_layout`; all automated checks pass (**156 tests, 0 failures**). A visible interactive scroll/resize/close/caret/picker smoke pass and non-Windows build remain unverified.
