# Xui implementation tracking

This file records project changes, design decisions, trade-offs, current status, and next steps. It is updated as implementation work lands.

## Showcase demo extended to the full widget set — 2026-10-14

### What was added

- `examples/showcase.xui` now demonstrates every runtime-supported widget, organized by category:
  - **Input tab** (existing `TextBox`, read-only `TextBox`, `TextArea`, `CheckBox` pair) plus `SearchField` (clear glyph, Escape reset, live readout) and `NumberField` (value/min/max/step/unit, live `Delay: N ms` readout).
  - **Controls tab** (existing `Button` pair, `Slider` + readout, `ProgressBar`, `Dropdown`, `ColorPicker`) plus `Switch` (with label), `ToggleButton`, and `RadioGroup`, each with a live value readout in a readout column.
  - **Layout tab** (existing `Split` + `CheckList`/`CheckItem` + wrapping text + tooltip button) plus a display section: `Separator` (used as section dividers on all three tabs), `Image` (Contain fit, rounded), `Label`, `Badge`, and an enabled/disabled `Spinner` pair.
- Every new stateful control has an authored `id` and a `change` handler; `glfw_demo.cpp` reads each value through `Xui::runtime().value()` and displays it via `setValue()` on a readout `Text` (and, for search/notes/name, the window title). This demonstrates the value retriever API end to end for `bool` (switch/toggle), `f64` (number field), and `string` (search/radio) alternatives.
- `glfw_demo.cpp` generates `xui-demo-image.png` (96x64 RGBA gradient) into the process working directory at startup via `xui::writePng` when missing, because `Image` resolves sources against the CWD. The widget reads it back through the project's own PNG reader — deliberately exercising both sides of the image I/O pair.
- `showcase.xss` gained token-consistent styles for all ten Phase 1 widgets, including `hover`/`focus`/`checked`/`pressed`/`readonly`/`disabled` state blocks.
- Runtime fixes found while building the demo: `RadioGroup` had no intrinsic measure (auto-sized to zero without explicit `.xui` size; now measures widest option + 28 px per row), and `NumberField`'s initial text appended the unit without a space while stepping added one (`250ms` vs `250 ms`) — both paths now produce `N unit`.

### Coverage audit

Runtime-supported and demoed: `Window`, `Column`, `Row`, `Panel`, `Split`, `CheckList`, `CheckItem`, `CheckBox`, `Switch`, `ToggleButton`, `RadioGroup`, `TextBox`, `TextArea`, `NumberField`, `SearchField`, `Tabs`/`Tab`, `Text`, `Label`, `Badge`, `Separator`/`Divider`, `Image`, `Spinner`, `Button`, `Slider`, `ProgressBar`, `Dropdown`, `ColorPicker`, `FpsCounter`, tooltips (property, multiple triggers/delays), plus the read-only and disabled variants. No supported widget is absent from the demo. Schema-only names (`Grid`, `ScrollView`, `Menu*`, `Modal`, `ListView`, `Table`, `Canvas`, …) remain unsupported and are listed in `doc/widgets.md`.

Premise corrections worth recording: this checkout has a GLFW + OpenGL 2.1 backend only (no separate Win32 adapter; `xui_hello` is the same sources on Windows), and the backend contracts are `xui::Renderer` / `xui::Platform`. The task context's "modal, scroll view, menu, list view" are not implemented; "image, separator, spinner" are and are now demoed.

### Decisions and trade-offs

- Widgets were added to the three existing category tabs rather than a new tab, keeping one place per category; `Separator` doubles as the section divider so it is demonstrated structurally, not just as an exhibit.
- Readouts use identified `Text` nodes updated from `change` handlers (the existing slider pattern) rather than any new binding mechanism; the window title remains the interaction log for single-shot events (actions, tasks, tab changes).
- The demo writes one small file into the CWD (`xui-demo-image.png`) instead of changing process directory or copying assets; this keeps the app code platform-neutral and matches the documented CWD-relative source resolution.
- Two runtime fixes were small enough to fold into this change rather than defer: the `RadioGroup` intrinsic measure and the `NumberField` unit spacing.

### Verification and current status

- **Compile-verified only, partially.** The runtime and Phase 1 widget code compiled clean under MSVC `/W4 /WX` (targets `xui_core` through `xui_tests`) in a freshly reconfigured `build/` directory before the demo edits. The demo edits themselves (`showcase.xui/.xss`, `glfw_demo.cpp`, and the two runtime fixes above) have **not** been compiled or run in this session — binary execution and further builds were stopped by the owner, so the following remain unverified: showcase parse/validate/build, all new-widget interactions, spinner animation cadence, image generation/readback, FPS, and the no-regression checklist (text overlap, freezing on resize, tooltip behavior).
- This checkout's `build/` directory was reconfigured in place (the stale cache pointed at the older `Desktop/xui` checkout); that older tree keeps its own build directory.

### Next steps

1. Build and run the suite: `cmake --build build --config Release --target xui_tests && ./build/tests/Release/xui_tests.exe` (expect 152+ passing, including `showcase_example_assets_parse_validate_and_build` over the new content).
2. Build and run the demo: `cmake --build build --config Release --target xui_glfw_demo && ./build/examples/Release/xui_glfw_demo.exe` — walk the three tabs, exercise every control, and watch readouts update.
3. FPS check with the spinner visible: `./build/examples/Release/xui_glfw_demo.exe --profile` (bounded hidden run) plus a visible run with F3 enabled during interaction and resize.
4. Visible smoke pass items still open from earlier entries: hover/checked visuals, tooltip dismissal over popups, resize smoothness, close behavior.

## Phase 1 widgets: selection, numeric, and display controls — 2026-10-14

### Audit and diagnosis

- The agreed scope for this slice was the "inputs & display" set at full fidelity: `RadioGroup`, `Switch`, `ToggleButton`, `NumberField`, `SearchField`, `Label`, `Badge`, `Separator`/`Divider`, `Spinner` (activity indicator), and `Image`, plus read-only state styling. Scrolling/containers, overlays/menus, and lists/tables were deferred to later phases in dependency order.
- The core schema already validated all of these names (`xui_schema.cpp`), so no new language surface was required except small property additions; the gap was runtime materialization in `UiRuntime::buildNode`.
- The existing primitives covered the set without new infrastructure: `Painter::line` for separators/spinner ticks, `Painter::image` + `Renderer::createTexture` for images, `readPng`/`Image` for decode, the timer system (tooltips/FPS) for the spinner cadence, and the `TextBox` editing path for `NumberField`/`SearchField`.
- `readonly` already existed end to end (style state enum, `.xss` parser name, runtime activation for `textBox`/`textArea`); it was previously undocumented. `NumberField`/`SearchField` reuse it by sharing the `textBox` flag.

### Changes made

- `UiRuntime::Node` gained per-type flags and state for the ten widgets; `buildNode` maps them to `Leaf` layout nodes, parses their `.xui` properties (options/value/step/precision/unit/fit/source/checked), and registers the interactive ones for focus, tab, and event routing.
- `RadioGroup`: pipe-separated `options` with `value` selection, click-to-select rows, Up/Down/Left/Right keyboard movement, row-local hover tint, `change` on selection, and string `value()`/`setValue()`. Rows are painted by the control (ring, dot, labels) like dropdown rows; per-row selectors are not available.
- `Switch`: track/knob paint driven by `checked`; the on-state color is the control's `fg`. Optional label is offset clear of the track (this overlap was caught during documentation review and fixed before delivery). Toggles on click-release, Space/Enter; `bool` value API.
- `ToggleButton`: `Button` behavior plus persistent `checked`; toggles on release with `change`/`press` events; text centered like `Button`; `pressed`/`checked` states style it.
- `NumberField`: `TextBox` editing path with numeric input filtering, live commit-and-`change` on every valid edit, Up/Down stepping by `step`, `min`/`max` clamping, `precision` (0–6) and `unit` formatting, and `f64` value API. Invalid or partial entries never change the committed value.
- `SearchField`: `TextBox` semantics plus a painted `×` clear affordance with a reserved 18 px text inset, click-to-clear, and Escape-to-clear; both fire `change`.
- `Label` (alias of `Text` with its own name and value exposure), `Badge` (text leaf for pill styling), `Separator`/`Divider` (aspect-oriented 1 px line in `fg`), `Spinner` (eight-tick activity indicator, one step per 80 ms through the existing idle timer, disabled-aware), and `Image` (first-paint PNG load, RGBA8 texture via the renderer, `Fill`/`Contain`/`Cover` fitting with center-crop UVs, rounded-corner clip, placeholder box when the source is missing/unreadable or no renderer is attached).
- `Xui::attach()` now calls `UiRuntime::setRenderer()` so `Image` can create textures; the facade clears it on shutdown. Headless clients get the placeholder path.
- Value routing: `value()`/`setValue()` gained `bool` for `Switch`/`ToggleButton`, `f64` for `NumberField`, and string handling for `RadioGroup`/`Label`/`Badge`; `controlText()` covers the text-bearing display widgets. Intrinsic measure sizes keep auto-sized `Switch` (44x24), `Separator` (12x1), `Spinner` (18x18), and `Image` (96x64 placeholder / natural size) usable with no configuration.
- Hit testing treats `Label`/`Badge`/`Spinner`/`Image` as pointer blockers like text/panels; RadioGroup rows and the SearchField clear region resolve inside their control's bounds. `cursorAt` returns an arrow over the clear region.
- Schema additions: `readOnly` on `NumberField`, `maxLength`/`readOnly` on `SearchField`. Documentation-only otherwise; the `.xss` state table gained its missing `enabled`/`readonly` entries in the reference.

### Trade-offs and limitations

- `Image` uses the project's own `readPng`, which only reads the uncompressed PNG variant `writePng` produces (8-bit RGBA, filters 0/2). General PNG/JPEG decoding is future work; the placeholder path makes the failure visible instead of silent.
- RadioGroup rows, Switch knob colors, spinner cadence, and the SearchField clear glyph are built-in chrome; `.xss` styles the control surface and text colors but not those sub-elements. Per-row radio styling needs the same virtual-target styling work as dropdown rows.
- `NumberField` keeps typed text as authored until a step or programmatic set reformats it; the committed `f64` is always available through `value(id)`. Escape does not revert a number edit (only SearchField clear and popup close use Escape today).
- The spinner reuses the fixed-interval idle timer (80 ms) rather than interpolating real elapsed time, matching the FPS counter's approach; animation smoothness is bounded by event-loop wakeups.
- No animation/transition system was introduced; `Spinner` and existing timers remain the only time-driven paint.

### Verification and current status

- **Not yet build-verified.** This checkout's `build/` directory is stale: its CMake cache points at an older checkout (`C:/Users/Franck/Desktop/xui`), so `cmake --build build` compiles that tree, not this one. The one suite run performed after the changes executed the stale tree's binaries and crashed before printing its summary — that result says nothing about this code. A fresh configure inside this checkout (for example `cmake -S . -B out/ci` or reconfiguring `build/`) is required before any compile, test, profile, or regression claim can be made.
- Code review during documentation caught and fixed four defects before delivery: Switch label/track overlap, ToggleButton text not centered like Button, `Label`/`Badge` ignoring the named `text:` property, and RadioGroup hover state tracked but never painted.
- `plan.md` is no longer present in the checkout (it was also gitignored); phase planning and scope decisions are recorded here instead.

### Next steps

- Configure a working build directory, compile `xui_tests`, add per-widget unit tests (build/parse, interaction, value API, readonly state, spinner timer, image placeholder), run the full suite plus `--profile`, and update the showcase `.xui`/`.xss` with a Phase 1 widget section.
- Then the visible GLFW smoke pass that remains open from earlier entries: hover/checked visuals, spinner motion, number stepping, image rendering, resize, and FPS during interaction.

## Dropdown hover, color input, and live slider values — 2026-10-14

### Audit and diagnosis

- Dropdown options are popup rows painted by the `Dropdown` control, not `.xui` child nodes. The hover row previously resolved the dropdown header's `.xss :hover` style and reused its background, coupling the option hover appearance to the header. When the pointer moves into the popup, the header correctly stops being hovered; a stable header outline must therefore come from the control's `open`/`focus` state rather than pretending the header is still hovered.
- Slider pointer movement changed the logical value and requested redraw, but did not invoke the `change` callback. The release path invoked the callback once, so observers and the demo's title-only readout appeared to update after drag completion.
- ColorPicker exposed only an RGBA `Color` getter and a fixed HSV popup. `.xui` accepted only literal color expressions at initialization; there was no text parser, format cycling, editable value row, or functional alpha behavior.

### Changes made

- Dropdown popup rows now use an independent foreground-tinted row hover highlight and no longer resolve the header selector's hover style. The header remains un-hovered while the pointer is in the popup; its resting outline remains and `.xss open`/`focus` styles can emphasize it. The showcase uses a stronger `open` border.
- Slider `change` now fires on pointer-down when the thumb/value changes and on every distinct logical value change while dragging. Left/Right keyboard updates continue to fire once per value step. Pointer-up no longer duplicates the last change event. The showcase includes an authored `Text[id: "volume-readout"]`; its callback updates the label with `UiRuntime::setValue` and also updates the window title.
- Added color string conversion in the shared UI runtime. Input and programmatic `setValue` accept `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`, comma-separated `rgb()` / `rgba()`, `hsl()` / `hsla()`, and `hsv()` / `hsva()` with percentage saturation/lightness/value. Parsing rejects invalid strings without changing the last valid color. Named CSS colors and modern space-separated CSS syntax remain unsupported.
- Preserved `Color` (RGBA float components) as the stable `value(id)` result. `controlText(id)` returns the active-format string. ColorPicker also accepts string values in `setValue` and string initial values in `.xui`.
- Expanded the default popup with a saturation/value surface, hue strip, selected-position markers, optional alpha strip, editable text row, and cycling format label. `.xui` supports `popupSize` (with safe minimum dimensions), `format`, ordered pipe-separated `formats`, `precision` (0–4 decimal places), `alpha`, `showAlpha`, `showHue`, `showSaturationValue`, `showInputs`, and `showFormatSwitcher`. The editor validates/applies each complete valid value as it is typed; clipboard copy/paste routes through the existing `Platform` interface.
- Updated `doc/widgets.md`, `doc/styling-reference.md`, and `doc/troubleshooting.md` to describe event cadence, popup hover separation, value API, supported color syntax, and current limitations. Updated `plan.md` from fresh build/test/profile results.
- Added/extended runtime tests for RGB(A)/HSL parsing, invalid string rejection, picker text editing, and slider callbacks during pointer-down/drag without duplicate release callbacks.

### Decisions and trade-offs

- Kept picker interaction settings in `.xui` because format order, alpha allowance, precision, and visible controls affect behavior. Kept surface and state appearance in `.xss` using existing `size`, `bg`, `fg`, `border`, `radius`, and state selectors; no platform-specific picker implementation was introduced.
- Retained RGBA `Color` as the only canonical value type. Returning a formatted string from `value()` would make the API depend on presentation format; `controlText()` serves as the format-specific display/readout path.
- Slider events are emitted for each distinct logical value. This can result in frequent callbacks during rapid pointer movement; rendering remains coalesced by the existing redraw loop, and the runtime does not throttle the value/event path.
- The popup remains popup-only in this slice. Inline mode, component numeric fields, custom labels/placeholders, named colors, individually themed hue/alpha/indicator controls, and selectors for individual dropdown options are explicitly deferred rather than implied by the current API.
- A visible desktop test was not run. The Windows examples both use GLFW/OpenGL in this checkout; there is no separate native Win32 implementation to validate.

### Verification and current status

- Release builds succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests` with the Visual Studio CMake/MSBuild installation.
- Full headless suite: **153 tests, 0 failures**. New coverage verifies typed color API parsing and validation, typing through the popup value field, and slider change callbacks on immediate value changes during drag.
- Hidden uncapped profile after the final code build: `xui_hello.exe --profile` = **4,810 frames / 7.53706 s / 638.18 FPS**; `xui_glfw_demo.exe --profile` = **4,901 frames / 5.00283 s / 979.645 FPS**. Both exceed 60 FPS in hidden continuous rendering; hello again overshoots the requested five-second profile duration. These figures do not establish visible FPS, display pacing, resize feel, or input-to-frame latency.
- No white-box, glyph appearance, visible hover/outline, close behavior, or resize test was possible in the hidden profile. Tests confirm shared runtime logic, not actual visible GLFW interaction. Linux/macOS and native Win32 remain unverified/not present respectively.
- Status: dropdown option hover no longer reuses the header hover style; live slider change notifications and a live demo label work in runtime tests; picker parsing, editing, switching, alpha, and option configuration are available. Full picker customization and visible acceptance remain open.

### Next steps

- Perform a visible GLFW smoke pass for dropdown hover/open outline, popup format switching and typed values, alpha adjustment, slider readout during drag, resize, normal close, and text appearance.
- If full customization is still required, implement and test inline mode, labels/placeholders, independent numeric fields, and explicit XSS sub-control color properties; currently these are documented limitations.
- Investigate why both five-second hidden profiles again ran for roughly 7.4 seconds and add repeatable visible frame-time/input-latency measurements before making a production FPS claim.

## Audit — 2026-10-13

### Current state

- `.xui` has a hand-written parser, import loader, diagnostics, and schema validation (`xui_assets`).
- `.xss` has a hand-written parser/compiler, token evaluation, selector matching, state resolution, transitions, and themes (`xui_assets`).
- Retained layout, draw-list painting, a CPU software renderer, and TTF/text/atlas code exist as separate libraries.
- No runtime turns an `.xui` AST into retained nodes, resolves `.xss` against those nodes, translates style values into layout/paint inputs, or routes widget events.
- The previous Hello sample bypasses both file formats. It hardcodes geometry/colors, draws all text with GDI, and uses Win32 hit testing. Win32 currently hosts and blits the output; it is not a framework renderer implementation.
- There is no GLFW/OpenGL backend, general widget library, `.xui`/`.xss` hot reload, full expression/binding evaluation, accessibility bridge, or application runner.

### Implementation plan

1. Add a framework-owned runtime bridge for the first useful vertical slice: `Window`, `Column`, `Row`, `Text`, and `Button` structure from `.xui`; selector/state/style resolution from `.xss`; conversion into `LayoutTree`; and drawing through `Painter` + `Renderer` including atlas-backed text.
2. Replace the hardcoded Hello sample with `hello.xui` and `hello.xss` assets. Keep Win32 limited to native window lifetime, message delivery, and presenting the framework-rendered pixels.
3. Add runtime tests proving the sample can be parsed, styled, laid out, rendered headlessly, and clicked without a native control.
4. Document the implemented language/runtime subset and its unsupported features instead of implying the complete project brief is already delivered.
5. Record each implementation change and any discovered issue here.

### Decisions and trade-offs

- Use the current software renderer as the first backend because it is dependency-free and already implements `Renderer`; this proves backend separation without introducing a package download. It is a correctness/reference path, not a high-performance GPU backend.
- Keep Win32 as a thin host/presentation adapter for this example. Native widgets, text drawing, hit testing, and visual styling belong to Xui. A reusable `Platform` Win32 backend remains later work.
- Start with five core node types and literal text/event handler names. The parser/compiler support much more syntax than this vertical slice evaluates; full bindings, loops, components, menus, and advanced controls remain explicitly unsupported for now.
- Load the sample assets at runtime from the example output folder, so changing `.xui`/`.xss` does not require recompiling the UI source. File watching/hot reload remains later work.

## Change log

### Earlier implementation work in this chat

- Added `XUI_BUILD_EXAMPLES` and a Windows `xui_hello` target; added a native Hello window rendered with `Painter` and `SoftRenderer`.
- Added a quick-start README describing the prototype status and build commands.
- Fixed Windows compilation in that sample by disabling Win32 `min`/`max` macros, using `Painter::finish()`, and using Unicode cursor/mouse-coordinate helpers.
- Fixed the Windows font test stack overflow by replacing a 1 MiB automatic buffer with 16 KiB read chunks.
- Fixed atlas whitespace glyph dimensions to reserve a 1×1 empty cell.
- Corrected tests to reflect lazy glyph rasterization and to drain the key-up event before checking resize events.
- Verified the Hello target and named test/benchmark/fuzz targets compile under MSVC `/W4 /WX`; `ctest` passed with 138 tests.

### Current refactor

- Added `xui_widgets` and public `xui/ui/runtime.hpp`: the first runtime bridge turns one parsed `Window` tree containing `Column`, `Row`, `Text`, and `Button` nodes into retained layout nodes.
- The runtime matches type/class/id selectors and resolves resting/hover/pressed rules from the compiled `.xss` table. It maps `size`, `min-size`, `max-size`, `pad`, `margin`, `gap`, `grow`, `shrink`, `align`, and `justify` to the layout engine; it paints styled backgrounds and atlas-backed text through `Painter`.
- Added mouse hit routing for hover/press/release and dispatch for authored `press: handlerName`. Added a post-build font attachment API because the glyph atlas is renderer-owned.
- Added `examples/hello.xui` and `examples/hello.xss`. Structure/content, class names, palette, spacing, radius, font size, and button states now live in those files.
- Replaced the previous hardcoded Win32 button with a file-loading host. It parses/validates `.xui`, compiles `.xss`, loads Segoe UI, creates the framework runtime, forwards pointer messages, and blits the `SoftRenderer` framebuffer. It contains no GDI widget text calls or hardcoded widget geometry/colors.
- Changed the Windows Hello target to link `xui_widgets` and copy both authoring files beside the executable after each build.
- Added an integration test for parse → style match → layout → software render → button callback.
- Updated README with the runtime boundary, sample workflow, and remaining gaps.
- Added mouse-leave routing so a hovered style clears when the pointer leaves the client area.
- Verified MSVC `/W4 /WX` builds for `xui_hello` and `xui_tests`; CTest passes after the runtime addition. Launched the final example; process remained live with title `Xui - File-driven Hello`.

### Refactor decisions and trade-offs

- The Win32 layer remains a native top-level window and input/presentation adapter, but the widgets are framework-owned retained nodes. Keeping Win32 at that edge makes this example useful without pretending a reusable `Platform` or native `Renderer` exists yet.
- `SoftRenderer` is the first working backend because the framework already defines `Renderer` and the software path is dependency-free. It is not the eventual high-performance GPU backend.
- The runtime intentionally supports a narrow vertical slice and returns clear errors for unsupported widget/control-flow nodes. The `.xui` and `.xss` parsers retain their broader capabilities; the runtime does not claim to evaluate all parsed syntax.
- Runtime rendering uses the existing glyph atlas and requires the host to load a font and initialize the atlas. The test exercises shapes without a font so it remains independent of OS font files; the visible Windows sample loads Segoe UI.
- `.xui`/`.xss` are loaded at process start. Runtime file watching and in-place tree/style replacement are deferred to avoid conflating the initial composition path with hot-reload diffing.
- Alternative considered: using Win32 `BUTTON` controls and `DrawText` would have been shorter, but it would leave structure, styling, measurement, and input behavior outside Xui; that approach was rejected. Native Win32 is kept at the window/presentation boundary.
- Alternative considered: adding GLFW/OpenGL before connecting the current parsers would introduce dependencies and backend work without proving the core authoring pipeline. The software renderer is used first, with GPU backend work remaining explicit follow-up.
- Alternative considered: claiming full language support in the runtime because the parsers recognize the syntax would be misleading. Unsupported dynamic constructs are documented and a small end-to-end slice is tested first.

## Current status and next steps

- Status: the `.xui` + `.xss` path is now real for the documented five-node subset. The current Windows Hello executable loads both assets and remains live. Automated coverage passes (139 tests).
- Known runtime gaps: no imports/components/slots, interpolation or expression evaluation, bindings, `if`/`for`, property-filter selector lookup, keyboard/focus/accessibility, general controls, style-driven transitions/animation, or hot reload. The runtime currently requires a single `Window` root and ignores non-literal dynamic values.
- Platform/backend gaps: no reusable Win32 `Platform`, no Win32 `Renderer`, and no GLFW/OpenGL reference backend. This sample presents the CPU renderer through a DIB blit.
- Next: (1) add runtime component expansion and literal/property validation with better diagnostics; (2) implement a reusable Win32 `Platform` backend; (3) add input focus/keyboard and common controls; (4) implement expression/binding and property-filter selector evaluation; (5) add hot reload; (6) add OpenGL/GLFW backends and broader examples/performance measurements.

## Follow-up: centered button and visible label - 2026-10-13

### Diagnosis

- `.xui` parsing and schema validation already accepted the generic `align` property, but `UiRuntime::buildNode` did not consume it. The sample placed `align: Center` in `.xss`; `applyLayoutStyle` assigned it to the button's `alignItems`, which controls children of a container. A button is a leaf, and its parent `Column` therefore used its default `Stretch` cross-axis behavior. The fixed button width stayed at the left inset instead of centering.
- The button label was parsed from `.xui` and kept on the runtime node. The Win32 host did not make a native `BUTTON`, call `DrawText`, or need `WM_DRAWITEM`: `WM_PAINT` presented the framework's `SoftRenderer` output using the client rectangle.
- The missing glyphs came from `GlyphAtlas::rasterise`: glyph outline points were converted into atlas coordinates including the shelf's `(px, py)` offset, then sent to a rasterizer whose target was only that glyph's local `w x h` bitmap. Glyphs at the first shelf origin could appear; later glyphs were rasterized off-target and uploaded as empty coverage. The atlas UVs, renderer sampler, and XUI label value were not the cause.
- The host uses UTF-8 for its file strings and the sample label is ASCII `Click Me`; Segoe UI loads and parses. Layout uses `GetClientRect` and the same 1:1 client coordinates for layout, painter, and software framebuffer at DPI scale 1. DPI awareness is not yet configured, so high-DPI crispness/scaling is a separate platform follow-up; it did not cause this label failure. There is no `WM_DRAWITEM` path because the framework owns the button.

### Changes made

- `UiRuntime` now consumes an authored `.xui` `align` value. On `Window`, `Column`, and `Row` it sets the default alignment for children; on leaf widgets such as `Button` it sets `alignSelf`, the layout engine's per-item override.
- Moved the sample button's `align: Center` to `examples/hello.xui` and removed it from the `.xss` rule. `.xss` continues to own its background/interaction colors, foreground, font size, dimensions, and radius.
- Fixed atlas rasterization to use glyph-local point coordinates. `updateTexture` alone applies the shelf location when copying the completed bitmap into the atlas.
- Extended the runtime integration test to assert the exact centered x coordinate and, when a system TTF is available, count visible foreground pixels inside the rendered button.
- Documented the `.xui` alignment rule in the quick start.

### Decisions and trade-offs

- `.xui` owns this placement declaration because it describes the element's relationship to its parent. `.xss` remains responsible for the button's appearance and visual states. This leaves a narrow overlap in the existing style property vocabulary, but the runtime gives the explicit `.xui` alignment control as requested.
- A button-level `align` maps to `alignSelf`; putting `align: Center` on the `Column` instead would center every child, including the descriptive text, and would change the intended page layout.
- The atlas fix rasterizes once in local bitmap space and retains atlas placement in the texture upload, avoiding a special case for the first glyph or a renderer-specific workaround.
- Win32/GDI text or native button drawing was rejected because it would move label rendering and styling outside the framework path.

### Current status and next steps

- Status: the button is horizontally centered by `.xui`; its `.xui` label now produces visible atlas-backed glyphs; `.xss` supplies the font and colors. The Win32 host remains a window/input/presentation adapter.
- Verification: MSVC built `xui_hello` and `xui_tests`; CTest passed all 139 tests. The font-backed runtime assertion ran with the available Windows font and confirmed visible text pixels.
- Follow-up: add explicit DPI-awareness and convert physical client/input pixels to logical layout units; consider moving runtime `.xui` property application into a centralized layout-property mapper; later framework gaps remain as listed above.

## Follow-up: rendering, styling, controls and cross-platform demo - 2026-10-13

### Audit and implementation plan

- Rechecked the current architecture against the `.xui` + `.xss` goal. The parsers/compiler, retained `LayoutTree`, backend-neutral `Painter`/`DrawList`/`Renderer`, and `Platform` interface provide the intended separation. The runtime was the main gap: it only materialized Window/Column/Row/Text/Button, applied a narrow style subset, did not connect the existing Split layout kind to pointer input, and had no checklist object.
- Kept the existing interfaces and extended the runtime. A broad rewrite was unnecessary; the existing split solver, state model, shader-reference SDF renderer, font atlas, and platform event abstraction were reusable.
- Added an optional GLFW example target rather than making GLFW a mandatory dependency of the core or default CI build. The project does not have GLFW installed in the current workspace, so the example target could not be compiled here.

### Changes made

- Extended the retained runtime to materialize `Panel`, `Split`, `CheckList`, and `CheckItem`; parse checklist labels/initial checked state/disabled state and `.xui` ratio/direction/gutter; apply the split ratio; and draw the styled splitter.
- Added pointer toggling for checklist items, authored `change` callbacks, focus tracking, Tab focus cycling, and Enter/Space activation. Splitter hit regions are translated from split-local to root coordinates; drag deltas go through `LayoutTree::splitDragUpdate`.
- Expanded runtime painting for borders, shadows, opacity across the full node, per-corner radius, text alignment, letter spacing, and disabled/focus/checked/hover/pressed states. Added `background`, `color`, `padding`, and `font-family` aliases plus named font-face registration by family and weight. Missing family registrations fall back to the host's default face.
- Improved high-DPI text by rasterizing atlas glyphs at physical pixel size and scaling glyph quads back into logical layout units. Added a reusable glyph-quad scratch buffer so repeated paints reuse capacity. Updated the Win32 host to use per-monitor DPI awareness, convert pointer/client physical pixels to logical layout units, and forward Tab/Enter/Space input.
- Added `examples/glfw_platform.*`, a GLFW implementation of the framework's `Platform` boundary, and an OpenGL texture presentation renderer in `glfw_demo.cpp`. Both the Win32 and GLFW hosts now load the same `cross_platform.xui` / `cross_platform.xss`, which demonstrate a checklist, interactive button, styled panels and a draggable split. The UI app code consumes Xui APIs; GLFW calls are contained in the backend/example renderer.
- Added `XUI_BUILD_GLFW_EXAMPLE` (off by default, requires installed GLFW 3.3+ and OpenGL) so dependency-free builds remain unchanged. Expanded core widget schema registration for `CheckList` and `CheckItem`; `active` aliases the existing `pressed` state.
- Added `loadUiAssets()` as a compact helper for parse → style compile → core schema validation; both sample hosts now use it. This removes repeated asset boilerplate while leaving file access and backend lifecycle choices with the application.
- Added runtime coverage for checklist toggling, keyboard activation, style parsing and splitter resizing. Rewrote the README to describe actual capabilities, setup, font registration, limitations and the cross-platform sample's rendering trade-off.

### Root findings, decisions and trade-offs

- The framework was not only a hardcoded Win32 GUI: the existing example already passed authored files through a runtime into layout and a `Renderer`. The cross-platform concern was real, though: there was no concrete `Platform` implementation besides Null, and the Win32 host remained OS-specific by design.
- Reused `LayoutTree::Split` and `SplitDrag` rather than inventing a second splitter implementation. A checklist is a semantic container with `CheckItem` children, keeping repeated items explicit and styleable in `.xui`/`.xss`.
- Kept GLFW/OpenGL optional so normal core builds and CI do not need network access or third-party setup. Alternatives were vendoring/fetching GLFW automatically (simpler first build, but adds network/build risk) or writing a fully GPU-resident primitive renderer immediately (better throughput, but a much larger backend and validation effort).
- The current `GlfwOpenGLRenderer` uploads the CPU reference-rendered frame as an OpenGL texture. It is a real OpenGL presentation path and lets identical `DrawList` output run behind a GLFW context, but it does not accelerate primitive rasterization. This keeps output deterministic and reduces initial backend risk; the CPU path remains the performance bottleneck. GPU SDF rendering is the next backend optimization.
- Text antialiasing continues to use the analytic glyph scan conversion and coverage atlas already in the project. DPI-aware atlas rasterization improves high-density displays; actual family/weight variation requires the host to load/register those font faces. Complex-script shaping, fallback chains, variable fonts and subpixel LCD rendering are still outside the custom text engine's scope.
- Simplification kept the core APIs intact: documentation now provides a small checklist/split example and a clear optional-dependency switch. The authored language was not collapsed into a reduced format. Runtime support remains intentionally narrower than parser syntax and rejects unsupported widget nodes rather than pretending to render them.
- Complexity assessment: an app still needs to choose a platform, renderer, font, atlas and frame loop, so the low-level setup remains moderate. `loadUiAssets()` removes the repeated parser/compiler/schema steps; further hiding renderer/platform lifecycle behind one global application object would make custom backends and headless rendering harder, so that trade-off was not taken.

### Current status, verification and next steps

- Status: the dependency-free Win32 sample and core runtime compile; the shared runtime now supports the new controls and expanded style path. The GLFW source and build target are present but remain unverified in this workspace because GLFW is unavailable. The cross-platform sample requires a discoverable system font or `XUI_DEMO_FONT`.
- Verification: MSVC strict-warning build completed for `xui_tests` and `xui_hello`; direct test execution passed 141 tests, including checks that load the real cross-platform sample assets and drag its splitter. The optional GLFW target was not built, so cross-OS compilation and visual behavior still need confirmation on hosts with GLFW/OpenGL development packages.
- Next: (1) compile/run the GLFW demo on Windows, Linux and macOS; (2) implement a GPU SDF `Renderer` consuming `DrawList` directly to remove full-frame CPU rendering/upload; (3) add scrollbar/clipping/keyboard accessibility for lists and panels; (4) add reusable platform integrations and high-DPI test coverage on non-Windows hosts; (5) expand supported expressions, components and data bindings.

### Open questions

- Should the optional GLFW backend automatically fetch a pinned GLFW release when no system package is found, or stay package-manager supplied?
- Which bundled font family and license should the project adopt for identical out-of-box typography across operating systems?
- Is the checklist's current independent-toggle behavior sufficient, or should the runtime expose item values/selection bindings before the widget is considered complete?

## Follow-up: responsive runtime, text bounds, FPS, and backend guide - 2026-10-13

### Audit and baseline

- Confirmed the core philosophy is present: `.xui` parsing, `.xss` compilation, retained `LayoutTree`, `UiRuntime`, backend-neutral `Painter`/`DrawList`, and separate `Renderer` / `Platform` contracts. The Win32 executable is one host; it does not create native child controls.
- Existing benchmark run on the Windows Debug build: 938-node synthetic full resize layout median about 1.07 ms; a one-leaf content change about 18 μs; idle same-size layout about 0.1 μs; 641-widget paint emission/batching about 3.1 ms. These are synthetic local timings, not a release-build promise.
- The baseline paint benchmark did not include the software rasterizer. Both interactive samples use the deterministic CPU `SoftRenderer`; the GLFW/OpenGL presenter also rasterizes on CPU then uploads a full framebuffer. Thus a full software frame remains the largest expected resize/redraw cost and the OpenGL sample is not GPU-accelerated UI rendering.
- The Win32 host requested redraw only when state changed (Windows coalesces invalidations), but each paint still walked/layouted/painted the full UI, rasterized the whole frame, allocated an intermediate `Image`, then made a second full-frame BGRA conversion. Runtime painting re-resolved every node's style and pointer handlers linearly scanned the complete widget list.
- Text measurement could receive a width from the layout solver, but painting shaped text without a wrap constraint and issued glyphs under only the outer viewport clip. Long strings therefore escaped their own widget. Parent text properties also did not cascade.

### Changes made

- Cached each widget's resolved `.xss` table until its interaction-state mask changes. Added compact button/checklist/slider and splitter index lists; hover, press, focus cycling, and hit testing no longer scan noninteractive layout nodes or allocate a temporary focus vector for Tab.
- Implemented inherited text defaults for `font`, `font-size`, `fg`/`color`, `align`, and `letter-spacing`. Values are recomputed from the current parent state; an explicit child declaration wins. This follows the user's selected CSS-like inheritance behavior.
- Text paint now shapes to the available content width at the active DPI, honors padding, vertically centers wrapped runs, and clips glyphs to the text widget's content box. This keeps clipping at widget scope and leaves raster backends unaware of layout rules.
- Added `Slider` and `ProgressBar` runtime leaves. Sliders update from pointer position, support left/right keyboard steps after focus, honor min/max, and emit their `.xui` change handler. `ProgressBar` renders a declarative range/value. Added `controlValue(NodeId)` for reading a slider value; no general binding system was introduced.
- Added a stylesheet-controlled `FpsCounter` widget toggled with F3. The runtime measures frames in a backend-neutral clock; both sample hosts schedule redraws while visible.
- Added reusable `SoftRenderer::snapshotRGBA()` / `snapshotBGRA()` exports. Win32 now writes directly to reusable BGRA storage, avoiding the temporary RGBA `Image` and separate swizzle pass; the GLFW presenter similarly reuses an RGBA output buffer instead of allocating an `Image` every frame.
- Added `FrameInfo` dirty-rectangle metadata and runtime damage tracking for layout changes and interactive state changes. `SoftRenderer` preserves the pixels outside a partial frame; Win32 clips `Painter` to the invalidated client region, rasterizes and exports only that region, and presents only those pixels. Full frame redraw is retained for initial paint and resize.
- To keep native drag-resizing responsive despite the measured full-frame CPU cost, the Win32 sample scales the last completed bitmap during `WM_ENTERSIZEMOVE` and performs one fresh layout/render on `WM_EXITSIZEMOVE`. This is a deliberate visual trade-off: layout changes catch up on release. Live reflow during resizing requires the planned GPU renderer.
- Extended `cross_platform.xui` / `.xss` with slider, progress bar, FPS counter, and corresponding visual rules. The same files remain used by the Win32 and optional GLFW example.
- Added `architecture.md` for parse/layout/style/render/event data flow, interface contracts, backend creation steps, inheritance, and performance limits. Expanded README with a custom Platform/Renderer startup and frame-loop example, widget behavior, FPS toggle, and honest software-rendering caveat.
- Expanded the cross-platform asset integration test to validate/build the new controls, operate the slider, and toggle FPS. Corrected its node IDs for the expanded sample.

### Decisions and trade-offs

- Kept `.xss` inheritance to the five common text properties named above rather than inheriting visual backgrounds, borders, padding, or layout dimensions. Those properties should remain opt-in; broad CSS inheritance would surprise nested panels and merge visual ownership.
- Used text wrapping plus widget clipping. Ellipsis needs a dedicated text layout/truncation contract and was unnecessary to guarantee bounds; clipping alone would hide useful content in multi-line `Text` nodes.
- Kept Slider/ProgressBar as backend-neutral vector primitives in `UiRuntime`/`Painter`, rather than native widgets, so the same assets and interaction semantics work across backend implementations. ProgressBar is display-only until data bindings exist.
- Kept the software renderer as the reference implementation and did not pretend that frame-buffer upload is a fast OpenGL renderer. A direct GPU `Renderer` has higher potential throughput but needs its own primitive/shader implementation and backend validation.
- Optimized the confirmed host copies and runtime hot loops without adding dirty-rectangle complexity. Partial redraw needs retained damage propagation and renderer support; it is not safe to claim without that architecture.

### Current status and verification

- Status: implementation and docs are complete for this pass. Text geometry wraps and clips; parent defaults cascade; slider, progress and F3 FPS are in the shared example; click-driven Win32 redraws use damage rectangles; pixel export buffers are reused. The software reference renderer remains an explicit resize-performance limitation.
- Verification: strict MSVC Debug build succeeded for `xui_tests`, `xui_hello`, `bench_paint`, and `bench_layout`. CTest passed all 143 tests, including partial-frame preservation, text clipping, and shared sample/slider/FPS behavior. Latest Debug benchmark: layout resize ~1.11 ms; paint generation ~2.84 ms; full 640x360 reference raster ~154.6 ms + BGRA export ~8.9 ms; a 100x50 partial update ~4.45 ms raster + 0.19 ms export. Latest Release benchmark: layout resize ~97 μs; paint generation ~257 μs; full 640x360 raster ~28.0 ms + export ~1.56 ms; 100x50 partial update ~1.16 ms + 0.06 ms export. These are machine/build-specific synthetic measurements.
- Key status caveat: button and hover interactions use partial CPU raster; the Win32 sizing loop stays responsive by scaling its last frame, with accurate content reflow on release. The final full CPU frame can still be slow at large sizes. Do not claim the current OpenGL example is accelerated: it still uses the CPU reference renderer followed by a full RGBA upload.
- GLFW target was not rebuilt in this environment because the optional GLFW package is not installed; the path remains unverified here. F3 event mapping and reusable pixel export were updated in that source.

### Next steps and open limitations

- Implement a direct GPU `Renderer` that consumes the `DrawList` SDF attributes and glyph/image textures; use it for the Win32 and GLFW hosts where available. This is the required next step to make large full-window resize reliably smooth.
- Measure dirty-frame interaction on the actual Win32 sample and compare Release builds on target Windows hardware. Current interactive timings were measured with a synthetic scene, not a GUI automation trace.
- Build/run the GLFW example on Windows, Linux, and macOS with their package-provided GLFW/OpenGL development dependencies.
- Add value/data binding so the slider can drive the progress bar; add public widget change subscriptions/value validation; consider keyboard-focus visual parity and touch input.
- Add broader text tests for wrap/clipping at multiple DPI scales and stylesheet inheritance where nested parent states change. Complex-script shaping and automatic cross-platform font fallback remain out of scope.

## Follow-up: live rendering performance, wrapped lines, dropdown and color picker - 2026-10-13

### Audit and measured causes

- Rechecked both built executables and the shared source path. Layout was not responsible for the reported 3 FPS: the Release 938-node layout benchmark measured about 95 μs for a resize and 87 μs for a splitter drag; unchanged layout did no work.
- The actual bottleneck was the renderer. Release paint generation for a 641-widget synthetic scene took about 0.28 ms, while `SoftRenderer` took about 28.6 ms to rasterize 640×360 and another 1.7 ms to export BGRA. That is roughly 34 FPS before host work. `glfw_demo` then uploaded the finished CPU image to OpenGL instead of rasterizing UI primitives on the GPU.
- The old Win32 host ran that same CPU raster path and deferred layout while resizing: it stretched the last frame during the entire native sizing loop and reflowed only when the drag ended. Its partial repaint/DIB presentation also depended on preserved pixel regions and expanded SDF shadows, which could leave stale edges and artifacts.
- Text shaping correctly populated multiple `lineStarts` and a multi-line height, but never advanced `ShapedGlyph.y` after a wrap. Every rendered line therefore occupied the first baseline, causing the reported overlap.
- The project included parser schema entries for dropdown/color picker, but the runtime rejected both as unsupported. The `.xss` parser and state resolver already supported hover/open states; runtime hit testing only applied hover to the implemented controls.

### Changes made

- Added `GlDrawListRenderer`, an OpenGL 2.1-compatible shader renderer for the shared `DrawList`. It uploads reusable VBO/IBO storage and renders rounded rectangles, borders, shadows, glyph atlas quads, images, and capsule lines directly. Texture lifecycle, SDF flags, physical coordinates, scissor clips, and premultiplied-alpha blending follow the `Renderer`/`DrawList` contract.
- Routed both Windows `xui_hello` and `xui_glfw_demo` through the same GLFW `Platform` and direct OpenGL renderer. This removes the old Win32 DIB host from the build and deletes `hello_win32.cpp`; native OS/window/context calls remain within GLFW's platform adapter.
- Removed the cached stretched-frame resize shortcut from the active sample path. GLFW size events now trigger a live logical layout, paint, GPU submission, and swap. The event loop blocks while idle and redraws only for visual changes; the optional FPS counter schedules periodic frames at up to 120 Hz.
- Added five-second `--profile` mode to both executables, timing layout, UI paint/batching, GL submission, and swap/present. `xui_glfw_demo --profile --compare` also compares one rendered GL frame against `SoftRenderer` at tolerance 8.
- Fixed text wrapping by incrementing the shaped glyph baseline for every soft or explicit line break. Wrapped runs now paint at the line height already used by measurement and are still clipped to their widget content box.
- Added runtime support for single-select `Dropdown` and `ColorPicker` leaves. Dropdown options use `options: "One|Two|Three"`; popup selection, Enter/Space, arrows, Escape, hover, open and disabled states are supported. ColorPicker uses an in-app HSV saturation/value field and hue bar, supports click-and-drag, emits its `.xui` change handler, and exposes `controlColor(id)` and `controlText(id)` for host code.
- Added hover/open/focus/disabled rules and a long wrapped-text example to `cross_platform.xui` / `cross_platform.xss`. The demo callbacks show the selected dropdown value and color in the window title.
- Added runtime tests for popup selection/color changes and a text test that requires the next line's glyph y position to advance. Updated the shared example integration test for the new node count.
- Rewrote the performance section of `architecture.md`, updated README setup/backend/control/profile documentation, and added the new GPU path and profile measurements here.

### Profiling and verification

- Final local Release hidden-window sample profile, Windows OpenGL 2.1 context, 1000×680 logical sample, 5 seconds:
  - `xui_glfw_demo`: 4,794 frames; layout median 0.2 μs; UI paint + batch 59.7 μs; GPU submission 60.2 μs; swap/present 778.7 μs; measured effective rate 958.7 FPS.
  - `xui_hello`: 5,025 frames; layout median 0.3 μs; UI paint + batch 61.6 μs; GPU submission 61.2 μs; swap/present 777.2 μs; measured effective rate 1,004.9 FPS.
- The direct GL frame was compared against the CPU reference for one complete demo frame: 24 pixels differed by more than 8 channel values; maximum channel delta was 43. The small mismatch count is likely from edge coverage precision, but this check did not collect mismatch coordinates. It is a visual regression baseline, not proof across every driver.
- A fresh final Release build succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests`; CTest passed. An earlier Debug build tree could not compile because the installed MSVC reported `C1902: Program database manager mismatch`; the fresh Release build compiled successfully.
- Both final profile processes exited successfully. The GL profile results exceed 60 FPS by a wide margin on this Windows host. They are frame-loop measurements with vsync disabled, not a guarantee for all GPUs, operating systems, display resolutions, or drivers.

### Decisions and trade-offs

- Chose one OpenGL renderer for both examples rather than adding a second native-only Win32 renderer. This keeps one rendering implementation and tests the platform boundary consistently, at the cost of making the Windows example require GLFW and OpenGL 2.1.
- Kept the CPU renderer for golden tests/headless execution. It remains slower for full-frame rasterization; UI code and `.xui` / `.xss` were not specialized around its pixel loop.
- Kept full-frame redraw for the examples. Dirty-rectangle paths are risky across swapped OpenGL back buffers, and a full GPU draw is inexpensive in the measured target. The reference renderer still supports partial updates for tests and clients that can preserve the target surface.
- Kept the XUI dropdown `options` property as a compact pipe-separated string because the current runtime does not evaluate dynamic collections or option child nodes. An eventual data-binding API can replace that representation without changing layout/style separation.
- Used an HSV saturation/value surface plus hue bar rather than a native color dialog. This behaves the same across backends and keeps popup styling and input in the runtime.
- Added a one-frame CPU-vs-GPU image comparison in the opt-in profiler. The small edge delta reflects different rasterization precision; requiring byte-identical pixels would constrain GPU implementations unnecessarily.

### Current status, next steps, and open questions

- Status: both sample targets use the direct GPU path on Windows, exceed 60 FPS in the local Release profile, and build with shared assets. Wrapping now advances baselines; dropdown and color-picker popups are runtime controls with `.xss` hover/open rules.
- Next: run the Release targets through a visible resize/interaction pass on Windows and build the same target on Linux/macOS to validate driver/compiler portability; profile those hosts at their intended resolutions. Add visible screenshot/golden coverage for OpenGL if automated context creation is available in CI.
- Remaining limitations: no complex-script shaping or font fallback chain; dropdown option data is static; the popup picker exposes current color by ID rather than a general observable/binding system; CPU and GL antialiasing can differ slightly at primitive edges. The 60 FPS target is verified on the local Windows test machine only.
- Open questions: should dropdown options later become child `Option` nodes or bind to a runtime data source; should ColorPicker expose alpha editing and keyboard navigation; which non-OpenGL renderer should be added for environments without OpenGL 2.1?

## Follow-up: minimal public application API - 2026-10-13

### Audit

- The prior “simple” asset helper still left each application responsible for a `StringInterner`, `UiAssetBundle`, font bytes/registration, `TextSystem`, `UiRuntime`, `Painter`, platform/window initialization, renderer initialization, and the full event/layout/draw/present loop.
- That path was functional but made a new project learn most internal layers before it could display a `.xui` file. The backend contracts already provided the right extension seam and did not need redesign.

### Simplification made

- Added public `xui::Xui` in `include/xui/xui.hpp` with `loadFont`, `loadXui`, `loadXss`, `attach`, `run`, error reporting, window options, event callbacks, and opt-in access to the underlying runtime/text system.
- Implemented file reading, parsing/schema validation, style compilation, font loading, window/backend initialization, atlas setup, UI runtime build, event dispatch, resize handling, redraw, and cleanup inside `src/ui/xui.cpp`.
- Routed `Xui::loadXui()` through the existing import-aware loader so relative imports resolve from their declaring file and the full loaded program is schema-validated before runtime construction.
- Kept the concrete `Renderer` and `Platform` objects caller-owned and passed by reference. Their virtual contracts and the lower-level runtime APIs remain available without changes in spirit.
- Migrated both `xui_hello` and `xui_glfw_demo` to the facade. Both still use the same `.xui` / `.xss` assets and direct OpenGL draw-list renderer through the GLFW platform adapter.
- Added facade integration coverage with null backends: real example assets load, all 23 nodes build, timed run produces frames, a close event exits cleanly, and shutdown releases the window.
- Updated README with the small startup path, backend requirements, loading fonts/assets, configuration, low-level escape hatches, and an old/new API comparison. Updated architecture data flow to show the facade composition layer.
- Changed sample `--profile` behavior to print total frame count/effective FPS from a hidden bounded run. The earlier stage-timing and `--compare` arguments were removed during entry-point migration; they are not part of the simple API.

### Decisions and trade-offs

- Chose a single-window facade that owns framework-side state and loop behavior, while the caller owns the renderer/platform instances. This reduces setup code without hiding the backend interfaces or tying shared app code to an operating-system API.
- Kept `runtime()` and `textSystem()` as explicit advanced escape hatches. They retain flexibility, at the cost of exposing internal types in the public header.
- `Xui` currently represents one attach/run lifecycle. Multi-window apps, embedding, and custom frame scheduling should use the retained lower-level APIs. A richer multi-window facade would add lifecycle complexity to the common path.
- Font loading is a path-based convenience and must happen before `attach()`. Fonts registered by family/weight are selected through `.xss`; the first loaded font is the default face.
- Removed specialized profiler stage medians from the sample CLI instead of keeping a parallel manual loop. This makes the sample demonstrate the public path, but stage-level diagnosis now belongs in dedicated benchmark targets.

### Verification and status

- Release build: `xui_tests`, `xui_hello`, and `xui_glfw_demo` built successfully with MSVC.
- Release CTest: 1/1 test executable passed after the final facade/import changes; that executable reports 146 tests, 0 failures.
- Final sequential Windows Release hidden profiles: hello reported 7,443 frames / 5.00052 s (1,488.45 FPS); GLFW demo reported 7,552 frames / 5.00032 s (1,510.3 FPS). Earlier profiles overlapped with other GPU/test activity and took about six seconds; isolated runs met the five-second bound. These uncapped results exceed 60 FPS locally.
- The facade integration test confirms successful loading/build/run/close with null backends. These headless checks do not verify visible glyph appearance, mouse interaction, resize behavior, GPU output parity, or cross-OS builds. The renderer and layout paths are shared with the previous implementation, but an interactive visual pass remains needed for those claims.
- Current status: API facade, both migrated examples, and docs are implemented. Release build, CTest (146 tests), and both hidden backend profiles pass. Linux/macOS compilation and visible Windows interaction are not verified in this pass; the hidden profile checks frame throughput and clean process exit but cannot confirm rendered glyph appearance or interactive resize behavior.

### Next steps and open questions

- Perform a visible run of both examples and check readable text, button/control response, window resize responsiveness, and normal close.
- Build and run on Linux/macOS with system GLFW/OpenGL dependencies.
- Consider whether the public class should support reattach after `shutdown()`; currently it is designed for one application lifecycle.

## Documentation set - 2026-10-13

### Audit findings

- Audited `xui::Xui`, `UiRuntime::build`, the core XUI schema, XSS property/state compiler tables, layout/style application, sample assets, CMake targets, and the `Renderer` / `Platform` headers.
- Confirmed the facade is implemented and examples use it. The renderer/platform interfaces remain separately implementable.
- Found a deliberate scope gap: the schema recognizes many widget names that the runtime does not render. Also, several `.xss` constructs compile into `StyleSheet` but have no standard-runtime behavior yet (property filters, theme switching, transitions/keyframe playback, and several paint properties).

### Documentation added

- Added `doc/README.md` as the documentation index.
- Added focused guides: getting started/CMake/backend setup, `.xui` syntax/layout, `.xss` selectors/tokens/states, runtime UI object catalog, property reference, architecture/backend contracts, performance guidance, and troubleshooting.
- Updated root `README.md` with a short doc overview and links, and root `architecture.md` with links to the current detailed guide.
- Kept `tracking.md` at the repository root as requested by the project workflow.

### Decisions and trade-offs

- Organized docs by reader task and language concern instead of creating one long manual. This keeps setup approachable and lets backend authors skip to their contracts.
- Documented the implemented runtime subset separately from schema-only names. This is more precise than calling every validator-accepted widget “available.”
- Listed every `.xss` compiler property and marked runtime support/default behavior. The trade-off is that the reference includes implementation caveats alongside authoring instructions, but it avoids promising effects the UI does not currently render.
- Included current platform-specific implementation as examples only; shared `.xui`/`.xss` and application logic remain backend-neutral. CMake builds the source target `xui_widgets`; an installed/exported package target is not yet provided.
- Linked existing root `architecture.md` rather than duplicating its historical measurements; created `doc/architecture.md` as the current lifecycle and interface guide.

### Current status and next steps

- Status: all requested documentation areas have Markdown guides and are linked from `doc/README.md` and the root README. Examples match the current public facade and describe parser/runtime limitations.
- Verification: source/schema/style tables and bundled samples were audited directly. CTest passed (1/1 executable; 146 cases, 0 failures), including the shared example parse/runtime integration coverage. A local-link scan resolved links across all 12 root/doc Markdown files. No code or runtime behavior changed in this documentation pass; the visible app binaries were not rerun.
- Next: keep the property/widget reference synchronized when runtime support expands; consider adding an install/export CMake target and automated parsing of documentation snippets when the package workflow is introduced.
- Open question: should the broad schema-only widget vocabulary remain available for forward language validation, or should it be narrowed until those runtime controls are implemented?

## Project readiness plan - 2026-10-14

### Audit and verification

- Audited the current public facade (`xui::Xui`), renderer/platform interfaces, example CMake targets, runtime widget catalog, style/property reference, setup/docs, and prior verification notes.
- Confirmed the simplified facade and `doc/` documentation set exist. The current runtime supports a documented subset; many schema-recognized widget names and several compiled style features still have no standard runtime behavior.
- Found no distinct native Win32 backend in current source/build definitions. Both Windows `xui_hello` and `xui_glfw_demo` are built from the GLFW platform and OpenGL renderer sources.
- Freshly built `xui_hello`, `xui_glfw_demo`, and `xui_tests` in the Windows Visual Studio Release tree. The test executable passed **146 tests, 0 failures**.
- Ran both rebuilt example binaries in `--profile` mode. `xui_hello`: 6,633 frames / 5.0006 s / 1,326.44 FPS. `xui_glfw_demo`: 6,598 frames / 5.00011 s / 1,319.57 FPS. These hidden, continuous, uncapped loop measurements show clean bounded exit and local throughput; they do not verify visible glyph quality, interaction latency, resize behavior, or cross-platform operation.
- Did not perform a visible desktop pass or Linux/macOS build in this audit. The previous freeze and white-box reports remain open verification risks despite the automated suite passing.

### Plan created

- Added [`plan.md`](plan.md) with current progress divided into verified, unsupported, partial, and unverified claims; risks; prioritized professional acceptance goals; effort/dependency estimates; milestone exit criteria; blockers; and decisions needed.
- Used the owner-confirmed target themes—over 60 FPS, responsive resize/interactions, correct text/layout, and cross-platform parity—as the provisional professional acceptance baseline. Proposed a 50 ms p95 input-to-frame limit for discussion rather than presenting it as an agreed requirement.
- Made visible regression reproduction the immediate next milestone. The hidden profile is recorded as a loop check only, not as evidence that the visible performance or interaction target is met.

### Decisions and trade-offs

- Kept the plan as a separate root-level roadmap so the `doc/` folder remains user/developer reference material while `plan.md` records readiness and ordered work.
- Reported project structure and evidence from the current tree rather than treating the earlier project description as proof of a native Win32 backend.
- Split short-term stability/performance verification from medium-term widgets and long-term tooling/platform expansion. This preserves focus on the historical severe regressions before feature breadth.
- Listed effort as S/M/L/XL planning estimates and explicitly marked environment-dependent work and owner decisions as blockers; no delivery dates are implied.

### Current status and next steps

- Status: plan created; no source/runtime behavior changed. Windows Release build and 146 automated cases pass. Both hidden examples complete their bounded profile at over 60 FPS on this machine.
- Next: perform and record a visible Windows smoke/stress pass for glyphs, wrapping/clipping, hover/click, popups, resize, and normal close; add regression coverage for any failure found.
- Then decide supported OS/backend scope, including whether a native Win32 adapter is required; define reference hardware/workload and latency acceptance; set up the corresponding platform matrix.
- Open questions and criteria are tracked in `plan.md`, including text shaping/input/accessibility scope, schema compatibility, and packaging/versioning.

## M1 regression investigation - 2026-10-14

### Audit and root cause

- Re-read the current `plan.md`, runtime paint/layout path, facade loop, GLFW adapter, sample `.xui`/`.xss`, and headless runtime/render tests before making changes.
- Opened the pre-fix Windows `xui_hello` window. Actual glyphs were readable; this visible frame did not show white placeholder boxes. Checklist labels were visibly clipped at their first character. The long copy was rendered as wrapped lines; because the post-fix session could not be reopened, current sample layout remains due for a new visible check.
- Root cause for checklist clipping: `UiRuntime::paint()` placed `CheckItem` text at `rect.x + checkOffset`, but the clip region began at `rect.x + textLeft + checkOffset`. With `.xss` padding, glyphs began before the clip and the first character was cut off.
- While attempting the post-fix visible run, Computer Use reported that the physical Escape key had stopped the session and instructed that no further Computer Use calls be made. I stopped UI automation as instructed. This prevented checking post-fix hover/click/resize and the second example in a visible window.

### Change made

- Changed checklist label origin to include the `.xss` left padding as well as the checkbox offset. This keeps layout/style ownership in the runtime and stylesheet; no backend/platform-specific path was added.
- Added `ui_runtime_check_item_text_respects_padding_before_clip`, which builds a styled checklist item with a real test font and checks that the glyph geometry begins at the padded label position. The test failed with the strict original bearing assumption; after allowing the font’s one-pixel negative bearing, it passes while still distinguishing the old 6-pixel offset.
- Corrected the sample paragraph that described the shared document as used by both “Win32 and GLFW hosts.” The inspected targets currently use GLFW/OpenGL; the replacement text describes the backend-neutral layout/style boundary without claiming a native Win32 target exists.
- Updated `plan.md` with the newly discovered bug, exact verification boundary, current suite count, fresh profile data, and remaining M1 blocker.

### Verification

- Rebuilt `xui_hello`, `xui_glfw_demo`, and `xui_tests` successfully using the existing Windows Visual Studio Release tree.
- Full suite: **147 tests, 0 failures**, including the new checklist clipping regression.
- Final hidden bounded profile runs after the asset copy/rebuild: `xui_hello` = **7,643 frames / 5.00006 s / 1,528.58 FPS**; `xui_glfw_demo` = **6,941 frames / 5.00013 s / 1,388.16 FPS**. Both returned exit code 0. These are hidden, continuous, uncapped loop measurements on this Windows host; they do not establish visible-use frame rate, input latency, resizing responsiveness, or Linux/macOS support.
- Normal close: the pre-fix visible `xui_hello` window closed with Alt+F4 and disappeared from the window list. Post-fix visible close was not retested.
- Visible screenshot: readable text was present before the change; first-character clipping was confirmed. No post-fix screenshot was obtained, and `xui_glfw_demo` did not receive a visible interactive pass.

### Decisions, trade-offs, and status

- Fixed the narrow paint-origin defect first and added a low-level regression assertion rather than changing clipping or removing authored padding. The trade-off is that glyph bearings may extend slightly before the origin; the test permits one pixel while ensuring the full style padding is reflected.
- Did not start feature work because the roadmap’s M1 stability and visual acceptance remains open.
- Status: source-level defect fixed and covered; Release build, headless suite, and hidden profiles pass. Visible verification is partial. No claim is made that all interaction, resize, or freeze risks are cleared.
- Next: repeat the visible smoke pass for both rebuilt executables after the UI automation session is available; exercise hover/click, dropdown/color picker, resizing, and normal close; record reference machine/GPU/DPI; then proceed to measured frame/input instrumentation and cross-platform builds.
- Open questions remain those listed in `plan.md`; no additional product-scope decision was required for this bug fix.

## First professional controls slice - 2026-10-14

### Audit and scope

- Re-read `plan.md` before coding. It still places visible stability and cross-platform verification ahead of a broad widget catalog, and explicitly records editable fields and platform input as gaps. The user then supplied the next feature request and approved the proposed first slice: `TextBox`, `TextArea`, single-select `Tabs`, hover/focus tooltips after a short delay, `CheckBox`, typed values keyed by authored `id`, and the agreed default semantics.
- Inspected the runtime build/paint/event paths, schema, event model, GLFW adapter, sample assets, docs, and tests. The runtime already recognized these names in the schema, but did not build them. `TextInput` existed in the normalized event type but GLFW did not enqueue character input. There is still no separate native Win32 backend in the audited CMake targets.

### Changes made

- Added runtime controls for editable single-line and multiline fields, checkbox state, and single-selected tabs. Tab switching hides inactive page content from painting/hit testing and clears focus from hidden descendants.
- Added UTF-8 scalar insertion, basic caret navigation, Backspace/Delete, TextArea newline insertion, max-length counting by Unicode code point, placeholder text/state, read-only behavior, and password display masking. The platform still owns text event generation; GLFW now maps its character callback to `EventType::TextInput`.
- Added delayed hover/focus tooltips through the `tooltip` node attribute, a shared `.xss Tooltip` selector, compact bubble layout/clamping, and timer-based redraw scheduling. The facade waits until the tooltip deadline instead of polling while idle.
- Added `UiValue`, `value(id)`, and `setValue(id, value, notify=false)` for checkbox/checklist booleans, slider numbers, dropdown/text strings, picker colors, and tab indices. Existing specialized accessors remain available. User edits/selections invoke their authored change event; programmatic changes are silent unless explicitly requested.
- Added `examples/controls.xui` and `examples/controls.xss`, copied them beside both GLFW demo targets, and added the `--controls` command-line option. Updated the example event callback to demonstrate reading state by authored id.
- Updated the widget catalog, setup/index pages, styling state reference, runtime architecture guide, root README, and `plan.md`. The docs now explain control semantics, value types, `placeholder` state, supported gaps, and the new example.
- Added tests for field editing/value types, maxlength, checkbox interaction, tab pointer/keyboard/programmatic selection, tooltip timers, and parsing/building the checked-in controls example.

### Decisions and trade-offs

- Kept this first pass small and backend-neutral. TextBox/TextArea handle basic insertion/deletion/navigation but defer selection, clipboard, undo, mouse caret placement, IME preedit, validation, and accessibility integration. This avoids pretending the current platform contract already offers a complete editor.
- Tabs use fixed equal-width headers and a zero-based selected index. Scrolling, reordering, close buttons, and lazy page creation are deferred to preserve a simple layout model.
- Tooltips use a fixed 500 ms delay and one shared `Tooltip` style selector. An authored tooltip attribute avoids introducing an unsupported standalone overlay/control subtree; the trade-off is no per-tooltip style identity or rich content.
- Values use a `std::variant` to keep access typed without adding data-binding infrastructure. `setValue` is silent by default to prevent callback loops; callers may request notification explicitly.
- Kept application ownership of concrete renderer/platform instances and did not change either interface contract. GLFW text characters are normalized at the platform edge; no OS-specific logic was added to the shared runtime.

### Verification and status

- Windows Release build succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests` after the new implementation and examples were added.
- Full headless suite: **149 tests, 0 failures**. This includes the authored controls example loading into the retained runtime and test-level field/check/tab/timer interactions. It does not prove visible glyph appearance or on-screen responsiveness.
- After the final rebuild, concurrent hidden bounded `--profile --controls` runs exited normally: `xui_hello` = **5,016 frames / 5.00014 s / 1,003.17 FPS**; `xui_glfw_demo` = **5,154 frames / 5.00012 s / 1,030.78 FPS**. The runs overlapped. These are uncapped hidden-loop throughput figures on this Windows host, not monitor refresh rate, visible FPS, or input-to-frame latency.
- No visible GUI automation was performed. The previous Computer Use instruction for this task said to stop calling that interface after the physical Escape event, so final visual checks of typed glyphs, tooltip placement, tab headers, resizing, and close remain outstanding. Linux/macOS compilation and behavior remain unverified.
- The project still has no distinct native Win32 backend in this checkout; `xui_hello` is a GLFW/OpenGL target. This feature slice is therefore implemented and tested on the shared runtime / Windows GLFW path, not independently validated on a Win32 renderer.

### Current status and next steps

- Status: the agreed first control slice is implemented, documented, and headlessly verified; the examples build and both hidden profile runs exit. It is not yet production-complete because visible behavior and platform text/IME/resize behavior have not been verified, and advanced editing/accessibility are absent.
- Next: perform a visible `xui_hello.exe --controls` / `xui_glfw_demo.exe --controls` smoke pass; check text glyphs and wrapping, focus/caret, insertion/deletion, tabs, tooltip timing/placement, resize responsiveness, and normal close. Then run the same checks on each supported OS/backend once support policy is set.
- Continue with selection/clipboard/IME only after agreeing the initial release requirements; keep plan's platform, latency, packaging, and compatibility decisions open.

## Showcase widget/layout regression repair - 2026-10-14

### Audit and root causes

- Read the supplied regression report, then re-read `plan.md`, the current showcase files, the runtime build/layout/paint/input paths, the percent resolver in the layout engine, and existing runtime tests before editing.
- The primary size failure was percentage double scaling. Both `.xui width: 100%` and `.xss size: 100% ...` were normalized to a fraction in the runtime, while `LayoutTree` expects percentage points and divides by 100 itself. Thus `100%` became 1% of the containing block. This collapsed Tabs, page, Split, and row widths; children were squeezed to zero width, hit targets collapsed, and text/control geometry appeared overlapped or missing.
- The runtime also accepted generic `.xui` layout properties in schema validation but ignored most of them during build. Only `align` and a handful of Split-specific values were applied. The showcase authored `grow: 1` on Split and depended on flexible tab pages, so pages and panes retained intrinsic heights instead of filling their allotted area.
- Existing interaction logic has headless coverage; the reported erratic behavior was made worse by the collapsed/overlapping hit bounds. This audit did not establish a separate GLFW event-pump defect. Visible on-screen symptoms could not be reproduced directly in this turn.

### Changes made

- Corrected percentage conversion for both stylesheet and `.xui` lengths. `100%` is now passed to layout as 100 percentage points and resolved once by the shared engine.
- Implemented the generic `.xui` layout values the schema already accepts: grow/shrink, width/height/size, min/max size, pad/margin, align, and justify. Row/Column `gap` constructor arguments and CheckList `gap` properties are now also applied. Unsupported units or enum names on these layout properties return a clear build error.
- `.xui` structural values are applied after `.xss` layout defaults, so explicit authored geometry wins. Existing `.xss` layout declarations remain supported for compatibility.
- Added `.xui` padding to text measurement/painting offsets for controls that author padding structurally, while retaining `.xss` padding behavior when `.xui pad` is absent.
- Moved the showcase's geometry, spacing, flex, and padding values from `.xss` into `.xui`; `.xss` now focuses on backgrounds, colors, borders, typography, radii, and interaction states. Added `grow: 1` to each tab page and the Split so the page content and panes fill their parent area.
- Added layout IDs and a regression test that validates control sizes, tab page draw geometry, distinct row positions, split pane positions, and check item ordering at 1080x760. The same test exercises text input, checkbox, tab switching, button callback, dropdown selection, color selection, and checklist state using the authored bounds. Added a separate `.xss` 100% width regression test.
- Updated `doc/xui-guide.md` and `doc/performance.md` to explain structural `.xui` layout, property support, percentage semantics, and flexible page sizing. Updated the root README example to put panel padding in `.xui`. Updated `plan.md` to reflect the new verification and known visible-testing gap.

### Decisions and trade-offs

- Fixed shared runtime/layout conversion and property application rather than adding offsets or fixed dimensions only to the showcase. This makes old `.xss` percentage sizes and new `.xui` geometry consistent across the platform adapters.
- Preserved `.xss` layout defaults for existing assets. Where both files specify the same geometry, `.xui` now wins as the structural source. This retains compatibility while making the intended authoring split actionable.
- The current `.xui` padding/margin shorthand supports one scalar for all edges or a unitless horizontal-by-vertical pair. Full four-edge shorthand and dynamic visibility/opacity as `.xui` state remain out of scope; `.xss opacity` continues to style visuals.
- No features were added; the work repairs schema/runtime mismatch and layout sizing.

### Verification and status

- Windows Visual Studio Release build succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests` after the runtime and showcase changes.
- Fresh Release rebuild of `xui_hello`, `xui_glfw_demo`, and `xui_tests` succeeded. Full suite: **151 tests, 0 failures**. The showcase regression checks all tabs for non-empty draw geometry and valid/non-overlapping representative bounds; it routes text input, checkbox, tab, button, dropdown, color picker, checklist, and splitter-drag interactions and verifies resulting values/callbacks. The percent regression verifies `.xss size: 100% 40px` resolves to a 400px button at a 400px parent width.
- Fresh serial hidden profile runs exited successfully: `xui_hello.exe --profile` = **4,823 frames / 5.00159 s / 964.294 FPS**; `xui_glfw_demo.exe --profile` = **4,961 frames / 5.00071 s / 992.059 FPS**. These exceed 60 FPS in a hidden uncapped loop only; they do not establish visible presentation rate or UI latency.
- No visible interactive smoke pass was performed. The earlier Computer Use stop instruction remains applicable, so glyph appearance, hover/focus styling, physical typing, popup visuals, resizing, and close behavior still need an owner-run or available visible desktop check. Linux/macOS remain unverified.
- Status: source-level sizing and schema/runtime mismatch fixed and headlessly covered. The demo geometry and runtime input paths now pass tests. Production visual/stability acceptance remains open in `plan.md` M1/M2.

### Next steps

- Run a visible smoke pass through all three tabs at default and reduced window sizes; check actual glyph visibility, label clipping, hover/focus/disabled styling, tooltips, dropdown/color popup placement, text entry, and splitter drag/resize.
- Record visible FPS and input latency on the target hardware, then build/run on every advertised OS. If visible checks find a defect, capture the affected widget bounds/state and add a reproducing regression before changing renderer code.

## Unified GLFW widget showcase - 2026-10-14

### Audit and supported scope

- Re-read the runtime widget builder and the `plan.md` verification status. The currently implemented runtime objects are `Window`, `Column`, `Row`, `Panel`, `Split`, `CheckList`, `CheckItem`, `CheckBox`, `Text`, `Button`, `TextBox`, `TextArea`, `Tabs` / `Tab`, `Slider`, `ProgressBar`, `Dropdown`, `ColorPicker`, and `FpsCounter`; tooltips are attached with a `tooltip` property and styled with the `Tooltip` selector.
- Confirmed that names such as `Modal`, `ScrollView`, `Image`, `Menu`, `Spinner`, `ListView`, and `RadioGroup` are schema-only in this runtime and cannot be demonstrated as functioning controls. Kept them out of the runnable example rather than implying support.
- Chose one default, three-tab sample: Input (TextBox, read-only TextBox, TextArea, enabled and disabled CheckBox), Controls (enabled and disabled Button, Slider, ProgressBar, Dropdown, ColorPicker), and Layout (draggable Split with panels/checklist, wrapping text, tooltip target button). FpsCounter is visible at startup. This keeps every built runtime widget reachable in one app while avoiding a crowded single page.

### Changes made

- Added `examples/showcase.xui` for all runtime-supported objects, control IDs, authored callbacks, disabled examples, hover/focus/pressed states, tooltips, wrapping copy, and section organization through Tabs.
- Added `examples/showcase.xss` for the full showcase theme and resting/hover/focus/selected/checked/pressed/disabled/open/placeholder states where relevant. Visual values remain in `.xss`; layout/content/attributes stay in `.xui`.
- Changed `examples/glfw_demo.cpp` to load the unified showcase by default, start with the FPS counter visible, retain F3 toggling, and read typed state by authored id from the runtime in callbacks. The former `--controls` alternate path was removed from the executable; focused `controls.xui` assets remain copied for reference/tests.
- Updated CMake post-build asset copies for both GLFW example targets and added a test that parses, validates, builds, lays out the showcase, confirms its stateful authored IDs, and paints each of the three tab pages to non-empty draw geometry.
- Adjusted the facade FPS-counter wait: the counter value changes once per second, so idle redraws now wait at 10 Hz and platform events still wake the loop promptly. `redrawContinuously` no longer incurs an artificial 120 Hz wait, making hidden continuous profile numbers reflect loop throughput. Trade-off: the idle overlay may refresh less often than the rendering loop when used in a non-continuous app; user input and resize still request redraws immediately.
- Updated the README and `doc/README.md`, `doc/getting-started.md`, and `doc/widgets.md` to describe the default showcase and link the new sample. Updated `plan.md` from 149 to 150 tests and recorded the latest bounded profiles and verification limits.

### Verification

- Windows Visual Studio Release build succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests`.
- Full headless suite: **150 tests, 0 failures**. The new showcase test parses/validates its `.xui`, compiles its `.xss`, builds and lays out the tree, and checks expected authored IDs.
- Serial hidden bounded profile runs both returned exit code 0: `xui_glfw_demo.exe --profile` = **8,748 frames / 5.49181 s / 1,592.92 FPS**; `xui_hello.exe --profile` = **9,055 frames / 5.51635 s / 1,641.48 FPS**. These clear the numeric threshold in an uncapped hidden loop, but do not measure visible FPS, GPU presentation pacing, or interactive latency. Both runs exceeded their nominal five-second bound slightly.
- The runnable demo covers and exposes interaction paths for the runtime-supported objects, but no visible GUI automation was performed due to the prior Computer Use stop instruction in this task history. Therefore readable on-screen glyphs, tab/page placement, hover/focus/disabled visuals, typing, popup selection, resizing, and ordinary close have not been re-verified in this turn. Linux/macOS builds remain unverified.

### Decisions, status, and next steps

- Kept unsupported schema names out of the demo; adding placeholder UI for those names would weaken the runtime support contract. Kept the prior small example assets intact instead of deleting useful references.
- Used window-title changes in callbacks as a lightweight proof that typed values are retrievable without adding a separate status widget or state binding feature.
- Status: complete for the runnable showcase source/assets, docs, parser/runtime asset test, Windows Release build, and hidden loop profiles. Visual/interactive acceptance is still open; the project remains between M0 and M1 in `plan.md`.
- Next: perform a visible smoke pass on `xui_glfw_demo` for initial rendering, all tabs, text editing, tooltips, popup controls, drag resize, responsive feedback, and normal close; record visible FPS and frame pacing. Then run the same checks on supported Linux/macOS hosts and update the platform claim based on evidence.

## Pointer stacking, FPS cadence, and asset cleanup - 2026-10-14

### Audit and root cause

- Traced the reported overlap from paint and input routing. The retained tree paints parents before children and earlier siblings before later siblings, then paints tab headers, dropdown/color popups, and tooltips in separate overlay passes.
- `pointerDown` previously iterated all interactive nodes and assigned focus/pressed state to every overlapping control. `pointerUp` later chose one pressed node in reverse order. As a result, a covered dropdown could open while the visible front control received the release, and hover/focus styling could be applied to several covered controls at once.
- Tabs and popup surfaces had separate event paths with iteration order that did not consistently match their paint order. Inactive tabs were filtered in several paths, but hover had a path that could mark invisible descendants; normal hit checks also lacked a shared window-bound gate.
- The FPS overlay path forced redraws on every run-loop iteration, then waited 100 ms after each rendered frame when idle. That throttled the counter-enabled app to about 10 FPS. Its displayed value updated only once per second, so the readout lagged interaction and resize changes.
- Audited the source `.xui`/`.xss` assets and all non-generated references. `examples/hello.xui` and `examples/hello.xss` were unreferenced by current CMake targets, tests, documentation, or the default demo. `cross_platform`, `controls`, and `showcase` assets remain referenced by tests, docs, copy rules, or the runnable example.

### Changes made

- Added one reverse-paint-order hit test over visible runtime nodes. It clips pointer coordinates to the logical window, excludes inactive tab pages, treats painted controls/text/panels as blockers, and lets transparent `Row`/`Column` containers pass through. The first frontmost target alone receives hover/focus/pressed state and activation.
- Aligned overlay input ordering with paint order: the last painted open popup receives clicks first, tab headers receive clicks above normal tree content, and outside clicks dismiss popups before reaching the newly exposed target. Split gutters receive input only if no later-painted child surface covers them. Tooltip bubbles remain pointer-transparent.
- Changed FPS sampling and display updates to 250 ms. The facade schedules at most four overlay redraws per second while an event-driven app is otherwise idle; ordinary input/resize still wakes it immediately. Continuous redraw/profile mode does not wait for or force a separate FPS redraw.
- Removed unreferenced source assets `examples/hello.xui` and `examples/hello.xss`, plus stale generated copies in `build/verify-vs/examples` and its `Debug` output. Kept `cross_platform`, `controls`, and `showcase` because their current uses were confirmed in tests/docs/CMake/demo.
- Expanded `doc/widgets.md` with `.xui` values, value/event semantics, `.xss` examples and states for TextBox, TextArea, Tabs/Tab, Dropdown, ColorPicker, and Tooltip. Added explicit stacking/hit-testing semantics and limitations. Updated `doc/architecture.md`, `doc/performance.md`, and `doc/styling-reference.md` for tree order, overlay order, pointer routing, the 250 ms FPS cadence, and unimplemented arbitrary `z-index`/general container clipping.
- Added a regression fixture where an active tab-page button overlaps a dropdown beneath the tabs and verifies that the front button receives the event and the covered dropdown does not open. Strengthened tab coverage so a hidden page button cannot receive a click, and added an FPS scheduling deadline assertion.

### Decisions and trade-offs

- Used document tree order as the ordinary stacking contract because it already matches retained paint order and does not require platform-specific behavior. Arbitrary `.xss z-index` remains unimplemented and is now called out directly.
- Treated visibly painted controls, text, and panels as pointer blockers even when disabled, avoiding click-through. Transparent layout-only Rows and Columns stay pointer-transparent. Tooltips are deliberately pointer-transparent; popup surfaces capture only their own bounds.
- Did not add generic clipping in hit testing because standard runtime painting does not apply generic container clipping or `.xss clip`; introducing hit-only clipping would make clicks disagree with visible paint. Current pointer targeting respects the window bounds and selected tab visibility. General clipping remains a separate feature gap.
- A 250 ms idle refresh caps the FPS overlay's extra redraw work at four frames per second while giving the label a much shorter update interval. The displayed idle value consequently reflects event-driven redraw cadence, not hypothetical uncapped throughput. Continuous profiling measures the continuous loop independently.
- The old `hello` sample could have served as a starter, but it was not in current build/docs/tests and duplicated older example structure. The default showcase and the smaller cross-platform sample remain available and documented.

### Verification and current status

- Fresh Windows Visual Studio Release build succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests`.
- Full headless suite: **152 tests, 0 failures**. This includes the tab-page/dropdown overlap scenario, hidden tab-page filtering, FPS timer scheduling, existing popup/control interaction, tab changes, and splitter dragging.
- Final hidden serial profile runs exited successfully: `xui_glfw_demo.exe --profile` = **4,925 frames / 7.44068 s / 661.902 FPS**; `xui_hello.exe --profile` = **4,937 frames / 7.34833 s / 671.853 FPS**. Both exceed 60 FPS in these hidden uncapped runs, but both exceeded their requested five-second window. Earlier profiles of the same binaries completed in about five seconds; this timing variance needs investigation before calling the benchmark reproducible.
- No visible interactive desktop pass was performed. The current automated suite verifies shared-runtime targeting and timer behavior but cannot confirm visible glyphs, overlay placement, hover appearance, resize feel, close behavior, or input-to-frame latency. Linux/macOS are untested. This checkout contains no separate native Win32 adapter; `xui_hello` is built from GLFW/OpenGL sources, so no independent native Win32 result exists.
- Status: source-level z-order/event fan-out and FPS idle throttling are corrected and headlessly covered. Documentation matches the current runtime contract, and unused hello assets were removed after reference checks. Visible Windows interaction/performance acceptance remains open, and the profile duration variance needs follow-up.

### Next steps

- Run a visible GLFW smoke pass with FPS enabled: overlap/tab navigation, popup selection/dismissal, text entry, hover/focus, resize, normal close, and live counter refresh. Record screenshots, visible FPS, and input latency.
- Investigate why `xui_hello.exe --profile` exceeded the configured profile duration; rerun under the same conditions and capture process/loop timing before using it as a benchmark.
- If native Win32 or Linux/macOS are production targets, add/build the corresponding adapter and run the same conformance and visible smoke checklist there.

## Text editing, tooltip controls, frame-time display, and GLFW key delivery — 2026-10-14

### Audit and root causes

- Runtime Backspace/Delete already handled a single adjacent UTF-8 code point when called directly, but TextBox/TextArea had no selection range. They could not erase selected text or replace it with typed/pasted text, and the runtime tests covered only Backspace at the end of a string.
- The end-to-end deletion failure was in `GlfwPlatform::mapKey`: GLFW Backspace, Delete, Home, and End were not mapped to the normalized `Key` enum, so the runtime received `Key::None` for these keys. Both `xui_glfw_demo.exe` and `xui_hello.exe` use this GLFW adapter in this checkout. There is no separate native Win32 backend to verify.
- Tooltip behavior used a fixed 500 ms show delay, no hide delay/configuration, and stayed paint-visible after a target click. Since the tooltip was painted after popup overlays, it could obscure an open dropdown.
- FPS text formatted only the rounded frame rate. Hidden profile output measured loop throughput, not displayed frame time or user input latency.

### Changes and decisions

- Added a UTF-8 byte-offset selection anchor alongside the caret. Shift+Left/Right/Home/End adjusts a selection; Ctrl+A selects all; Ctrl+C/X/V use the abstract `Platform` clipboard; typing, Backspace, Delete, and Enter replace/delete the selected range. `setValue` resets caret and selection to the end. This keeps editing logic in the shared runtime and clipboard ownership behind the platform interface.
- Backspace/Delete/Home/End/Insert now map in the GLFW adapter. This is the direct integration fix for keys previously discarded before reaching the shared runtime.
- Added selection highlighting for focused editable controls. TextArea Enter inserts a newline; Backspace at a line start removes the newline and merges lines.
- Added `.xui` tooltip behavior properties: `tooltipShowDelay` (default 500 ms), `tooltipHideDelay` (default 120 ms), `tooltipTrigger` (`hover`, `focus`, `both`, `none`; default `both`), `tooltipHideOnClick` (default true), and `tooltipDisabled` (default false). The interaction policy belongs in `.xui`; bubble appearance stays in `.xss`.
- A click dismisses a tooltip whose target uses hide-on-click and suppresses it until the target loses hover/focus. Tooltip painting is suppressed while any popup is open; popup/hit-test behavior remains backend-neutral.
- FPS labels now show the average milliseconds per frame over the same 250 ms sampling interval (`FPS: 120 | 8.3 ms`). `FpsCounter[showFrameTime: false]` retains an FPS-only form. Average frame time was chosen over instantaneous timing to reduce display jitter without adding another timer.
- Kept the new work out of steady-state frames where possible: selection geometry is computed only while a focused field has a selection, and popup scanning is performed only while a tooltip is visible. No broader rendering/layout optimization was attempted without stage timings.
- Audited source `.xui`/`.xss` assets and references: `cross_platform`, `controls`, and `showcase` pairs are referenced by demo copy rules, tests, or docs. No source asset was unused, so no files were deleted.
- Updated widget, XUI, style, and performance documentation, plus the showcase's FPS width and tooltip option examples. Existing XSS appearance rules remain unchanged.

### Trade-offs and limitations

- Keyboard selection and clipboard cover common editing without introducing a second platform API. Mouse caret placement, drag selection, undo/redo, IME preedit rendering, validation, complex-script shaping, and accessibility remain unimplemented.
- Selection painting measures prefix layouts only while a field has an active selection. This adds work during selection interaction but leaves the normal unselected paint path unchanged.
- Tooltip trigger/delay values are runtime properties and are not XSS style values. The tooltip bubble remains a shared type selector, without rich content or per-target bubble styles.
- No stage-by-stage profiling instrumentation was added in this slice. The supplied hidden `--profile` check is a throughput ceiling, not representative visible use, and must not be called proof of interaction responsiveness.

### Verification and current status

- Windows Visual Studio Release build succeeded for `xui_hello`, `xui_glfw_demo`, and `xui_tests` after the runtime, schema, example, and GLFW key adapter changes.
- Full automated suite: **152 tests, 0 failures**. Runtime coverage checks Backspace/Delete, Shift selection deletion, Ctrl+A replacement, clipboard copy/paste through `NullPlatform`, UTF-8 input, TextArea newline insertion and line merging, configured tooltip show/hide delays, tooltip disablement, and existing tabs/popups/layout.
- Latest serial hidden profiles exited successfully: `xui_hello.exe --profile` = **4,804 frames / 6.89123 s / 697.118 FPS**; `xui_glfw_demo.exe --profile` = **4,906 frames / 5.00019 s / 981.163 FPS**. These exceed 60 FPS in hidden continuous rendering, but `xui_hello` exceeded the requested 5-second window by about 1.89 s. Earlier runs of both binaries also varied between near-five-second and longer durations; the cause remains unresolved.
- The GLFW key mapping change was compiled into both Windows example binaries. No visible desktop session was run, so physical Backspace/Delete delivery, visual selection appearance, tooltip overlap, resize smoothness, close behavior, and visible FPS remain unverified. Native Win32, Linux, and macOS results are unavailable in this checkout/session.
- Status: shared runtime deletion/selection and tooltip scheduling are covered by tests; the discovered GLFW key-delivery omission is fixed in source and builds. Performance exceeds the threshold in hidden profiles, but no representative visible-use or stage profile confirms production targets.

### Next steps

- Run a visible GLFW smoke pass to verify key delivery, selection/caret visuals, tooltip dismissal over dropdowns, resize, normal close, and readable FPS/ms text.
- Investigate the inconsistent `--profile` duration and add stable stage/frame-time and input-to-frame measurements before claiming performance acceptance.
- Add a distinct native Win32 adapter only if it remains a production requirement; otherwise update the project support policy to name GLFW on Windows as the verified current path.
