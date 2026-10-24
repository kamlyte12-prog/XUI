# Project status and roadmap

This is the canonical readiness plan. The detailed chronology and rationale for individual changes live in [`tracking.md`](tracking.md). User-facing behavior is documented in [`../doc/README.md`](../doc/README.md).

## Current progress

### What works

- The public `xui::Xui` facade loads fonts, `.xui`, and `.xss`, attaches caller-owned `Renderer` and `Platform` implementations, and runs the standard event/layout/paint lifecycle.
- Initial native window title and size resolve field-by-field from explicit `XuiOptions`, then `.xui Window(title, size)`, then root `.xss` `window-title`/`size`, then facade defaults. Windows Release tests cover each layer, single-field overrides, and explicit default-valued overrides.
- `compileXss()` supplies a Fluent 2-inspired light baseline across the supported runtime widget catalog; project-authored rules continue through the ordinary cascade and override it. The compiler and runtime also support paint-only `outline` / `outline-offset` focus rings. Checklist containers expose and accept a typed list of selected child IDs.
- `.xui` parsing/schema validation and `.xss` parsing/compilation are separate from runtime rendering. Tests exercise selectors, tokens, state resolution, diagnostics, layout constraints, draw-list rendering, text, and runtime event handling.
- The retained runtime materializes the controls and containers listed as supported in the [widget reference](../doc/widgets.md), including text input, tabs, dropdown/color picker, split panels, ScrollView, and value retrieval. The GLFW showcase exercises that supported set.
- Layout supports retained flow, Split, ScrollView, absolute child insets, percentage sizing, explicit min/max constraints on leaves and containers, measured text wrapping, viewport clipping, and nested scroll routing. Runtime behavior is covered by automated tests.
- Text layout now uses bundled HarfBuzz 15.0.0 for left-to-right OpenType shaping and GPOS positioning; variable `wght` is applied to both HarfBuzz and FreeType instances. The layout cache retains shaped advances and offsets. Ligature substitutions remain off for editable-text caret stability.
- Text telemetry now reports per-frame layout-cache requests/hits/misses/rehashes, HarfBuzz shaping time/runs/glyph count, and glyph-atlas rasterizations/uploads/rebuilds. `bench_text` exercises static labels, changing textbox text, wrapped paragraphs, and changing wrap widths. It exposed and fixed a cache-index growth bug; after the fix, a 600-operation width sweep produced exactly 300 cold misses and 300 warm hits. Windows Release microbenchmark numbers and limits are recorded in [`tracking.md`](tracking.md). These are CPU pipeline timings, not visible FPS.
- Variable and composite glyph contours are rasterized with TrueType non-zero winding fill; a regression test covers overlapping contours and counters. Windows Release rebuilt and the test binary reports **198 passed, 0 failed**. The layout demo stylesheet now uses a readable 32px title with explicit foreground contrast; visible retesting of the user's screenshot is still pending.
- Content-sized text children include rasterized glyph bounds and use the active DPI before layout; Row text coverage includes `!`, multiword text, two adjacent identical long labels, wrapping, and a button sibling. Headless draw-list checks pass at 1.0×, 1.25×, and 1.5× where applicable. Visible interactive verification is still pending.
- The GLFW showcase has eight sections. Its Docking page authors nested left/right/bottom regions and a center tab group in `.xui`, with all dock chrome in `.xss`; a callback reads dock state into a live value label.
- `examples/game/` now contains a Tetris implementation with seven-bag pieces, rotation/wall kicks, collision, row clears, scoring, levels, and a timed gravity update. Its board, controls, menus, HUD, pause, settings, and game-over page are authored in `game.xui`/`game.xss`; C++ updates only runtime values from game state and UI events. Windows Release builds the example and 204 headless tests pass, including all 200 authored cell values and the facade tick scheduler. Visible playthrough, resize behavior, and FPS are not verified; the board uses one block color and only button controls are authored.
- The eighth Motion & Style section demonstrates alpha, linear/radial/conic gradients, separate glow/shadow, font size/weight, nested auto-sized button content, state transitions, and keyframes. Headless tests cover the new paint/compiler/runtime paths; visible appearance and animation cadence remain unverified in this environment.
- Optional image loading is implemented in `xui_image`: bundled stb_image decodes PNG/JPEG/BMP/TGA, and bundled LunaSVG parses static SVG documents and rasterizes by display-size/DPI bucket. Relative image paths resolve from the loaded entry `.xui` directory. Windows Release tests/builds pass with support ON and OFF; the image-focused GLFW/OpenGL profile ran at 410 FPS. Manual visual quality and Linux/macOS remain unverified. SVG text, filters, masks, clip paths, and animation are outside the current subset.
- Visible keyframes can be started/cancelled through `xui::Xui`, with one-shot and loop modes; paused/completed animations return to event-driven waiting. Widget transforms paint the subtree and use inverse transforms for ordinary node hit testing. `.xss cursor` values reach the GLFW cursor adapter. The current Windows suite covers these code paths, but does not replace visual interaction checks.
- Docked panes support floating z-order raise and frontmost hit testing, with a headless overlap test. Client chrome is framework-owned on Windows: `Window[platformTopMenu: false]` hides native decorations and a `TitleBar` widget provides the draggable top menu with optional framework min/max/close buttons, styleable through `.xss`; the showcase uses it. Borderless windows resize from every edge/corner through the native sizing loop, with a configurable hit-test inset. The 2026-10-21 maximize fix keeps native non-client calculation/activation from repainting chrome and sizes the maximized client area to the monitor work area. Windows build, headless suite, and hidden GLFW profile pass; visible maximize/restore remains unverified. macOS/Linux retain native chrome as the documented fallback and also render the authored TitleBar as an in-client bar; see the platform limitation in `doc/widgets.md` and the 2026-10-21 tracking entry.
- Root `Window` `.xss radius` is passed to the GLFW platform for custom chrome. The Windows backend requests DWM rounded corners and excludes outside-corner hit pixels only when DWM accepts the hint, and only while restored; maximized windows retain their work-area shape. The Release backend compiles, but visual inspection of the corners and maximize/restore is still pending. macOS/Linux retain native rectangular corners.
- State transitions and keyframes use an event-loop timer path; GLFW cursor exit now clears stale hover so hover-out can run. The timer wait targets the remaining frame deadline instead of adding a fresh 16 ms after render. A second fix avoids passing an already-due timer as `waitEvents(0)`, whose platform contract is an indefinite wait. GLFW-visible windows continue animations while unfocused and pause clocks while hidden/minimized. `.xss` supports widget `animation-cancel: stop | finish` and per-animation `cancel(stop|finish)`; legacy `animation-keep: until-done` retains its retargeting behavior. Automated timer and cancellation tests pass. The latest visible timer profile measured 61.09 FPS over five seconds on Windows Release; live resize and input cadence remain unverified.
- F4 in the GLFW showcase toggles the previous frame's backend-neutral Painter draw-batch count beside the FPS text. It does not force redraws; F3 still opts into continuous FPS sampling.
- Popup command menus (`Menu`/`MenuItem`: items, separators, disabled/checked rows, shortcuts, one submenu level, keyboard navigation, right-click context anchoring) and the `NumberField` upgrades (invalid `error` state with blur revert, styleable `NumberButton` steppers, documented live per-keystroke commits) are implemented with headless tests; visible verification is pending.
- The 2026-10-21 MSVC error pass fixed the GLFW resize-inset type cascade, runtime warnings-as-errors and benchmark/test compile failures. The full Windows Ninja build succeeds; the latest test binary reports **188 passed, 0 failed**. It also fixed anchored-menu placement/input by resolving geometry from the anchor widget; visible appearance and FPS were not measured by that compile pass.
- The 2026-10-21 “Recent files” freeze was an infinite submenu paint traversal: an already-painted open submenu was selected repeatedly. The traversal now skips painted nodes, and the runtime menu regression test paints while a submenu is open. Windows full build succeeds and all 183 tests pass; visible GLFW interaction and FPS still need desktop verification.
- The bundled GLFW/OpenGL example is the only runnable graphics backend in this checkout. Current CMake defines `xui_glfw_demo`; there is no `xui_hello` target or distinct native Win32 backend.
- Optional GLFW and OpenGL backend implementations now live under root `backends/` as separate CMake targets; test-only sample assets are under `tests/fixtures/` while the demo's shared showcase pair remains in `examples/`.
- The root and `doc/` documentation entry points describe the current API and supported runtime scope. Agent context is indexed in this `plan/` folder.
- Phase D documentation now has a grouped runtime-widget catalog, property-by-property `.xss` applicability notes, a delimiter-accurate `.xui` grammar section, and public API comments. The stale Window title/size claim was corrected; parser-only `Tooltip` is no longer presented as a runtime widget. This pass changed no runtime behavior; details are in [`tracking.md`](tracking.md).

### What does not work

- Parser/schema recognition is broader than runtime support. Bindings, dynamic expressions, conditional/repeated nodes, component expansion, and the unsupported objects listed in the [widget reference](../doc/widgets.md) do not render as runtime features.
- There is no separate native Win32 backend target in this checkout. `xui_hello` is also GLFW/OpenGL; it does not verify Win32-specific behavior.
- There is no installed/exported CMake package target.

### What partially works or remains unverified

- Variable-font `wght` selection, matching HarfBuzz GPOS variation positioning, advances, and outlines use bundled FreeType 2.15.0 and HarfBuzz 15.0.0. Windows Release tests verify Roboto's `H/i/m/.` advances at weights 100/400/700/900, `To` pair adjustments that vary with weight, atlas advances, simultaneous face instances, and `.xss` weight on Text/Button/TextBox including parent inheritance. The same suite passes with Roboto Flex and static Segoe UI; Inter and Source Sans 3 assets are unavailable locally. Visible manual GLFW text inspection and macOS/Linux builds remain unverified; see [`tracking.md`](tracking.md).
- Latest Windows MSVC Release build compiles all targets, including `xui_tests` and `xui_glfw_demo`; the direct test executable passes with **203 tests, 0 failures**. The latest idle GLFW/OpenGL profile measures **465.384 FPS over 5.00017 seconds**. This is frame-loop throughput only; visible frame pacing, manual input, live resize, and DWM corner appearance remain unverified. Variable-font/GPOS evidence and font availability limits are in [`tracking.md`](tracking.md).
- Windows Release build of `xui_tests`, `xui_glfw_demo`, and `xui_hello` succeeded on 2026-10-16; **161 tests passed** after the docking runtime changes.
- Latest clean Release hidden GLFW demo profile measured **671.382 FPS over 5.00014 seconds**; latest hidden `xui_hello` profile measured **432.721 FPS over 7.99591 seconds**. These are loop throughput readings, not visible interaction/resize FPS; see [`tracking.md`](tracking.md).
- An optional live-resize hook now invokes the full render-frame path from GLFW size callbacks, including during a native modal resize loop. Source inspection and tests cover the path, but sustained manual border-drag verification is still pending.
- Visible checks for resize smoothness, normal close, popup editing, caret/tab behavior, cursor states, theme appearance, clipping, and widget interactivity across all showcase tabs have not been completed on the current source. The five-second visible timer profile clears 60 FPS on this Windows machine, but does not establish cadence during resize or interaction, so the full visible-performance acceptance criterion remains open.
- DockHost/DockPanel build and pass headless tests for authored recursive splits, tab activation, min-size constraints, splitter movement, release-outside floating (including a stale-preview pointer-up case), and value-API redocking. The current Windows Release microbenchmark measures 0.77 μs per drag-preview update and 2.07 μs per splitter update; six wide/deep scale scenarios from 8 to 32 panels now complete. Docking was not manually exercised in a visible desktop session, so chrome appearance, pointer feel, and docking-specific visible FPS remain unverified.
- Theme palette changes update paint/text styles, but theme-dependent layout metrics are not fully re-applied after initial `.xui` precedence resolution. Some compound widget details (for example virtual popup rows and semantic color ramps) have documented subpart styling limits.
- Linux/macOS builds and behavior have not been verified in this checkout session. The GLFW source is portable, but source portability is not a substitute for platform verification.
- The Windows release test target reports 171 tests passing after adding gradient, animation, transparent-hit-test, nested-button, and nested-tab coverage. The GLFW demo builds and its hidden profiles now initialize successfully after fixing a GLSL compilation error: 403.986 FPS with the glow-heavy page and 637.98 FPS with the animation page, each over five seconds. These are hidden-loop measurements, not visible presentation or input-latency results; the desktop automation channel remains unavailable.
- Latest Windows MSVC Release/Ninja build compiled `xui_tests` and `xui_glfw_demo`; CTest reports **1/1 passed**, with **178 tests, 0 failures** inside the test binary. Sequential hidden profiles measured animation **471.212 FPS**, transform **481.324 FPS**, glow/shadow **649.543 FPS**, docking **484.688 FPS**, and client MenuBar **489.502 FPS**. These are uncapped hidden-loop throughput results and do not verify visible >60 FPS, resize, or interaction criteria.
- The current transform implementation is partial for non-node overlays and virtual composite targets, and rotated scissor clips use bounding boxes. A profile can complete even when those corner cases remain; test transformed popup/scroll/tab scenes visibly before depending on them.

### Verification boundary

Claims above distinguish automated/headless tests, compiled demo targets, hidden loop profiles, and visible interactive checks. Only the first three have recent recorded evidence; do not describe visible resize/input or cross-platform parity as verified until an interactive run is recorded for each target platform.

The 2026-10-24 Windows Release verification built `xui_game_demo` and `xui_tests`; the test binary reports **199 passed, 0 failed** and CTest reports **1/1 passed**. The added game test validates the documents and runtime tree under the null backend. The game window has not been visibly played or FPS-measured, so this does not close M0/M1.

The Phase E Tetris update built `xui_game_demo` and `xui_tests` with Windows MSVC Release using the Ninja build directory. The direct test binary reports **204 passed, 0 failed**, including all 200 cell value IDs and the new scheduled-application-tick test. The visual GLFW game, interactive controls, resize path, paused idle CPU, and visible FPS remain unverified.

The latest text-diagnostics slice rebuilt `xui_tests`, `bench_text`, and `xui_glfw_demo` with MSVC Release. The test binary reports **201 passed, 0 failed**. Three text benchmark runs measured the 600-operation width sweep at a median of **99.16 µs/op**, with 300 misses followed by 300 hits after the cache rehash fix. The rebuilt demo completed its hidden animation profile at **425.29 FPS over 5.04 seconds**. Both measurements are headless/hidden throughput checks; visible resize/input FPS is still unverified.

### Documentation structure

User-facing guides are indexed in [`../doc/README.md`](../doc/README.md). Agent rules and this roadmap/history live under `plan/`; the root README is a short navigation page. Root `plan.md` and `tracking.md` remain compatibility pointers to these canonical files.

## Known issues and risks

- **Performance:** hidden loop throughput exceeds 60 FPS in the recorded Windows Release run, but visible FPS during resize, popup editing, docking, and interaction has not been measured. The CPU `SoftRenderer` is a reference renderer and can be too slow for full-window interactive use.
- **Text performance:** cached text is inexpensive in the local microbenchmark, but cold typing and width changes still require HarfBuzz shaping. The fixed index growth issue no longer causes duplicate cache misses; profile real visible text fields and live resize before pursuing further text optimizations.
- **Rendering and text:** shaping currently forces left-to-right runs and disables ligature substitutions. RTL/bidi paragraph layout, complex-script editing/caret mapping, automatic fallback fonts, and broad font-format support are absent. Inter and Source Sans 3 variable fonts were not available for verification. Text clipping depends on renderer scissor/texture behavior matching the draw-list contract.
- **Build dependency:** FreeType 2.15.0 and HarfBuzz 15.0.0 are built from bundled source under `lib/` via CMake. FreeType's optional zlib, bzip2, PNG, HarfBuzz, Brotli, and HVF integrations are disabled; its internal zlib inflater remains. Xui links the separate bundled HarfBuzz target for shaping/GPOS. Cross-platform builds still require each platform's compiler and GLFW system SDKs.
- **Stability:** the project history contains freeze, glyph-atlas, resize, and input regressions. Headless tests reduce risk but cannot establish native event-loop stability or normal close behavior.
- **Layout:** percent/min/max floors, nested minima, Split minima, absolute insets, and retained layout paths have automated tests, but no comprehensive visible matrix across window sizes/DPI is recorded. Grid/Stack are solver-only and unavailable in ordinary `.xui` runtime documents. Theme geometry reapplication remains incomplete.
- **API and packaging:** the facade simplifies startup, but there is no install/export package or stable compatibility/version policy. Lower-level ownership and frame-loop paths still require careful use.
- **Cross-platform:** only the GLFW/OpenGL path is implemented in the current examples. A native Win32 backend and current Linux/macOS verification are absent. GitHub Actions is configured for Ubuntu GCC/Clang, macOS, and Windows builds, but hosted results have not been checked in this update; only the Windows build/test path was run locally.
- **Transforms and menus:** normal widget paint and node hit testing share a transform matrix, but popup overlays, scrollbars, tab headers, and exact rotated clipping are incomplete. `MenuBar` remains a client-area Row container; menu popup semantics and menu keyboard navigation are unimplemented. Custom window chrome (`platformTopMenu` + `TitleBar`) compiles and has headless behavior coverage, but visible edge resizing, maximize/restore, and rounded-corner appearance remain unverified.
- **Accessibility and input:** accessibility semantics, IME/composition, advanced keyboard navigation, touch/gesture behavior, and broad platform text-input testing remain limited or absent.
- **Documentation:** docs now have separate user and agent indexes, but accuracy must track runtime support; parser/schema features must never be described as working controls.

## Goals for professional use

Priority order:

1. **Stability and correctness:** no hangs or crashes during startup, interaction, resize, or shutdown; glyphs, clipping, hit testing, and layout remain correct.
2. **Interactive performance:** exceed 60 FPS in normal visible use, respond to input without perceptible delay, and continuously re-layout/redraw during resize without freezes. Measurements must include visible runs, not only hidden profiles.
3. **Backend neutrality and parity:** the same `.xui`/`.xss` assets behave consistently on each declared supported backend; shared code does not depend on platform APIs.
4. **Predictable public API:** keep `xui::Xui` startup small while preserving documented custom `Renderer`/`Platform` and lower-level use.
5. **Accurate formats/docs:** supported syntax, defaults, style behavior, object support, setup and limitations match implementation and have examples.
6. **Extensibility and maintenance:** test each feature, maintain useful benchmarks, define compatibility/versioning, and make adding backends/widgets follow clear interfaces.

## Future work

| Priority | Work | Why | Effort | Dependencies |
|---|---|---|---|---|
| P0 | Run and record visible GLFW checks for resize, interaction, typing, popup editing, caret, cursor, and close; fix failures at their source. | Resolves the largest remaining correctness and acceptance gap. | M | Interactive desktop access; existing demo/tests. |
| P0 | Measure visible frame pacing during resize and widget interaction; profile layout, paint, submit, and presentation separately if below target. | Proves or falsifies the >60 FPS and responsiveness targets. | M | P0 smoke pass; benchmark build. |
| P1 | Build/run on Windows, Linux, and macOS; decide whether a native Win32 backend is a supported target or GLFW-on-Windows is sufficient. | Clarifies actual platform support and prevents false parity claims. | M–L | CI/toolchains and product decision. |
| P1 | Reapply theme layout metrics while preserving explicit `.xui` precedence; test two themes against geometry and visual states. | Makes themes complete rather than paint-only. | M | Cascade/merge model and focused tests. |
| P1 | Improve text shaping/fallback and platform text-input/IME handling; specify accessibility and keyboard coverage. | Needed for broader language and assistive-technology use. | L | Text/platform API design. |
| P2 | Add runtime support for prioritized missing objects (Grid/Stack first, then dedicated menus/dialogs and data views); keep each implementation incremental. | Closes gaps between schema vocabulary and usable widgets. | M–L per object | Layout/overlay architecture and acceptance priorities. |
| P2 | Verify image/SVG rendering visibly and on Linux/macOS; extend resource packaging/lifetime only as concrete app needs emerge. | Confirms image widgets across supported targets. | M | Cross-platform builds and visual test harness. |
| P2 | Add install/export CMake package, versioning/compatibility policy, and a small external consumer example. | Enables conventional downstream integration. | M | Public API and release policy. |
| P3 | Consider design tooling, live preview, widget inspector, and asset validation CLI. | Improves authoring productivity after runtime stability. | L | Stable formats, parser/runtime diagnostics. |

Effort estimates are relative, not calendar commitments. Do not start P2/P3 feature work while a P0 stability/performance issue is reproduced and unresolved.

## Roadmap and milestones

### M0 — Visible stability acceptance

- Exercise the eight-section `xui_glfw_demo` visibly: every widget category, text entry/deletion, popup open/edit/close, hover/cursor, scroll, horizontal/vertical Split drag, DockHost tab/edge/float/close/cancel actions, client-area File/Edit/View dropdown menu, continuous resize, and normal close. `xui_hello` is not a current CMake target.
- Exit when no regression from [`agents.md`](agents.md) reproduces; resize is visibly live and freeze-free; text/clipping/hit-test are correct; and results, platform, window/DPI, build mode, and FPS are recorded.
- **Blocker:** the previous desktop helper did not complete a sustained resize/interaction session.

### M1 — Performance and platform claims

- Measure visible interactive FPS and latency in normal use and during resize; the target is >60 FPS with no perceptible input delay.
- Build and test supported OS targets. Decide/record whether a native Win32 backend is required in addition to GLFW.
- Exit when each claimed target has a build, automated test result, visible smoke test, and recorded limitation list.

### M2 — Theme, text, and input completeness

- Reapply theme geometry without violating authored `.xui` overrides; verify text shaping, IME/input, focus/cursor, and accessibility decisions.
- Exit when two-theme layout/paint tests pass and documented platform text/input requirements are met.

### M3 — Runtime feature completeness

- Implement only prioritized runtime widgets, with parser, layout, `.xss`, values/events, tests, example, and documentation as a single slice.
- Exit each widget on parser diagnostics, runtime behavior, style states, value/event behavior, regression tests, showcase usage, and visible backend verification.

### M4 — Distribution and release readiness

- Add install/export/package validation, semantic version policy, compatibility notes, and downstream integration sample.
- Exit when a clean consumer project builds from the package without source-tree paths and CI covers the declared platform/backend matrix.

## Open questions and decisions needed

- Is GLFW/OpenGL on Windows the intended Windows production backend, or is a distinct native Win32 backend required? The current checkout has no separate Win32 target.
- Are Windows, Linux, and macOS the complete initial support matrix, or are additional platforms required?
- Which missing runtime widgets should follow Grid/Stack, and are menus/dialogs or virtualized data views the higher priority?
- What accessibility baseline, IME behavior, and text-shaping languages are required before professional release?
- Image decoder/rendering platform scope remains partly open: PNG has a decode test and SVG has a raster test; JPEG/BMP/TGA lack dedicated fixtures, and Linux/macOS builds have not run in this checkout.
- What compatibility/versioning policy and package-distribution form should the first stable release use?
- Should dedicated Menu/MenuItem controls use an in-client popup system, and what keyboard/accessibility behavior is required? The current `MenuBar` is a regular client-area container; the custom draggable top menu (`TitleBar` + `platformTopMenu`) now covers window-level chrome, but a command-menu model remains open.
