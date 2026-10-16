# Project status and roadmap

This is the canonical readiness plan. The detailed chronology and rationale for individual changes live in [`tracking.md`](tracking.md). User-facing behavior is documented in [`../doc/README.md`](../doc/README.md).

## Current progress

### What works

- The public `xui::Xui` facade loads fonts, `.xui`, and `.xss`, attaches caller-owned `Renderer` and `Platform` implementations, and runs the standard event/layout/paint lifecycle.
- `.xui` parsing/schema validation and `.xss` parsing/compilation are separate from runtime rendering. Tests exercise selectors, tokens, state resolution, diagnostics, layout constraints, draw-list rendering, text, and runtime event handling.
- The retained runtime materializes the controls and containers listed as supported in the [widget reference](../doc/widgets.md), including text input, tabs, dropdown/color picker, split panels, ScrollView, and value retrieval. The GLFW showcase exercises that supported set.
- Layout supports retained flow, Split, ScrollView, absolute child insets, percentage sizing, explicit min/max constraints on leaves and containers, measured text wrapping, viewport clipping, and nested scroll routing. Runtime behavior is covered by automated tests.
- The GLFW showcase now has seven sections. Its new Docking page authors nested left/right/bottom regions and a center tab group in `.xui`, with all dock chrome in `.xss`; a callback reads dock state into a live value label.
- The bundled GLFW/OpenGL example is the interactive backend in this checkout. Windows builds also produce `xui_hello` from the same GLFW/OpenGL example source.
- Optional GLFW and OpenGL backend implementations now live under root `backends/` as separate CMake targets; test-only sample assets are under `tests/fixtures/` while the demo's shared showcase pair remains in `examples/`.
- The root and `doc/` documentation entry points describe the current API and supported runtime scope. Agent context is indexed in this `plan/` folder.

### What does not work

- Parser/schema recognition is broader than runtime support. Bindings, dynamic expressions, conditional/repeated nodes, component expansion, transitions/keyframe playback, and the unsupported objects listed in the [widget reference](../doc/widgets.md) do not render as runtime features.
- There is no separate native Win32 backend target in this checkout. `xui_hello` is also GLFW/OpenGL; it does not verify Win32-specific behavior.
- There is no installed/exported CMake package target.
- The image decoder only accepts the project's own uncompressed PNG writer output; general PNG and other image formats are not supported.

### What partially works or remains unverified

- Windows Release build of `xui_tests`, `xui_glfw_demo`, and `xui_hello` succeeded on 2026-10-16; **161 tests passed** after the docking runtime changes.
- Latest clean Release hidden GLFW demo profile measured **671.382 FPS over 5.00014 seconds**; latest hidden `xui_hello` profile measured **432.721 FPS over 7.99591 seconds**. These are loop throughput readings, not visible interaction/resize FPS; see [`tracking.md`](tracking.md).
- An optional live-resize hook now invokes the full render-frame path from GLFW size callbacks, including during a native modal resize loop. Source inspection and tests cover the path, but sustained manual border-drag verification is still pending.
- Visible checks for resize smoothness, normal close, popup editing, caret/tab behavior, cursor states, theme appearance, clipping, and widget interactivity across all showcase tabs have not been completed on the current source. The demo ran in hidden profile mode only.
- DockHost/DockPanel build and pass headless tests for authored recursive splits, tab activation, min-size constraints, splitter movement, release-outside floating (including a stale-preview pointer-up case), and value-API redocking. A four-panel Release runtime microbenchmark most recently measured 0.67 μs per drag-preview pointer update and 1.88 μs per splitter resize update. Latest hidden GLFW/hello profiles measured 422.333/427.048 FPS. Docking was not manually exercised in a visible desktop session, so chrome appearance, pointer feel, and docking-specific visible FPS remain unverified.
- Theme palette changes update paint/text styles, but theme-dependent layout metrics are not fully re-applied after initial `.xui` precedence resolution. Some compound widget details (for example virtual popup rows and semantic color ramps) have documented subpart styling limits.
- Linux/macOS builds and behavior have not been verified in this checkout session. The GLFW source is portable, but source portability is not a substitute for platform verification.

### Verification boundary

Claims above distinguish automated/headless tests, compiled demo targets, hidden loop profiles, and visible interactive checks. Only the first three have recent recorded evidence; do not describe visible resize/input or cross-platform parity as verified until an interactive run is recorded for each target platform.

### Documentation structure

User-facing guides are indexed in [`../doc/README.md`](../doc/README.md). Agent rules and this roadmap/history live under `plan/`; the root README is a short navigation page. Root `plan.md` and `tracking.md` remain compatibility pointers to these canonical files.

## Known issues and risks

- **Performance:** hidden loop throughput exceeds 60 FPS in the recorded Windows Release run, but visible FPS during resize, popup editing, docking, and interaction has not been measured. The CPU `SoftRenderer` is a reference renderer and can be too slow for full-window interactive use.
- **Rendering and text:** complex-script shaping, automatic fallback fonts, and broad font-format support are absent. Text clipping depends on renderer scissor/texture behavior matching the draw-list contract.
- **Stability:** the project history contains freeze, glyph-atlas, resize, and input regressions. Headless tests reduce risk but cannot establish native event-loop stability or normal close behavior.
- **Layout:** percent/min/max floors, nested minima, Split minima, absolute insets, and retained layout paths have automated tests, but no comprehensive visible matrix across window sizes/DPI is recorded. Grid/Stack are solver-only and unavailable in ordinary `.xui` runtime documents. Theme geometry reapplication remains incomplete.
- **API and packaging:** the facade simplifies startup, but there is no install/export package or stable compatibility/version policy. Lower-level ownership and frame-loop paths still require careful use.
- **Cross-platform:** only the GLFW/OpenGL path is implemented in the current examples. A native Win32 backend and current Linux/macOS verification are absent.
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
| P2 | Add runtime support for prioritized missing objects (Grid/Stack first, then menus/dialogs and data views); keep each implementation incremental. | Closes gaps between schema vocabulary and usable widgets. | M–L per object | Layout/overlay architecture and acceptance priorities. |
| P2 | Add general image decoding and broader resource lifetime/packaging support. | Makes image widgets practical for applications. | M | Dependency/license decision and resource API. |
| P2 | Add install/export CMake package, versioning/compatibility policy, and a small external consumer example. | Enables conventional downstream integration. | M | Public API and release policy. |
| P3 | Consider design tooling, live preview, widget inspector, and asset validation CLI. | Improves authoring productivity after runtime stability. | L | Stable formats, parser/runtime diagnostics. |

Effort estimates are relative, not calendar commitments. Do not start P2/P3 feature work while a P0 stability/performance issue is reproduced and unresolved.

## Roadmap and milestones

### M0 — Visible stability acceptance

- Exercise the seven-section `xui_glfw_demo` and `xui_hello` visibly: every widget category, text entry/deletion, popup open/edit/close, hover/cursor, scroll, horizontal/vertical Split drag, DockHost tab/edge/float/close/cancel actions, continuous resize, and normal close.
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
- Should the project add a general image dependency or keep image decoding intentionally narrow?
- What compatibility/versioning policy and package-distribution form should the first stable release use?
