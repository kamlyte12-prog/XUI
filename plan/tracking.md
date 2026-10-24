# Xui implementation tracking

This file records project changes, design decisions, trade-offs, current status, and next steps. It is updated as implementation work lands.

## Phase D documentation reference pass — 2026-10-24

### Audit and corrections

- Audited the `.xui` parser, schema, `UiRuntime::buildNode()`, style-property reference, current widget guide, and Phase A–C text/image/window documentation before editing.
- Found the Window widget guide still claimed that `Window(title, size)` were only runtime metadata and that `XuiOptions` alone configured the native window. This contradicted the implemented field-by-field precedence (explicit `XuiOptions` > `.xui` > root `.xss` defaults > facade defaults). Corrected it and linked the canonical precedence section.
- Found `Tooltip` was linked from the runtime widget catalog even though it is implemented as an attached property and overlay behavior; the `Tooltip` schema node itself is not materialized. Reclassified the behavior and added the schema node to the runtime-unsupported list.
- The `.xui` guide described the node form but did not distinguish node suffixes from the same punctuation inside expressions. Added delimiter semantics and clarified that parse/schema support for expressions, `bind`, `if`, `for`, interpolation, imports, and components does not imply runtime evaluation/expansion.

### Changes and decisions

- Expanded the widget guide's opening index into a grouped catalog with anchors to the per-object reference entries. Kept detailed attributes, states, value/event access, examples, and limitations in those existing entries rather than copying that content into a second file.
- Added an `Applies to` column for each accepted canonical `.xss` property. It identifies broad runtime targets, widget-specific properties, synthetic-subpart caveats, and parser-accepted properties without standard runtime behavior. Existing state and composite-widget audits remain the detail source for special cases.
- Added API comments to `xui::Xui`, `XuiOptions`, `XuiRunOptions`, and the public lower-level `UiRuntime` entry points. Font comments state the actual TrueType-outline support instead of implying all OTF outline formats work.
- No architecture or runtime behavior changed. Code edits are documentation comments only; user-facing changes are in `doc/widgets.md`, `doc/xui-guide.md`, and `doc/styling-reference.md`.
- Trade-off: the styling support column groups common paint/text/layout targets to keep the full property table readable. Virtual subparts and exceptions remain in the existing composite-widget audit immediately below.

### Verification and status

- Compared documented runtime widget names against `UiRuntime::buildNode()` and checked parser behavior directly in `src/lang/xui_parser.cpp` and the core schema. The catalog distinguishes runtime aliases from schema-only entries.
- Phase A–C references for TrueType `glyf` fonts, HarfBuzz shaping, static raster/SVG images, root window defaults, and Windows DWM corner behavior were checked for stale claims; their detailed feature docs remain linked from the index.
- No executable behavior changed, so no build or runtime claim is made from this documentation pass. Project-owned Markdown link targets resolve, all canonical XSS property names are present in the table, its four-column rows are consistent, and `git diff --check` passes.
- Next: keep the docs synchronized with runtime changes; visible product behavior and platform limits remain as recorded in `plan.md`.

## Variable-font OpenType shaping and GPOS — 2026-10-24

### Audit and root cause

- The visible spacing problem was pair-specific, not a uniform per-glyph advance error: variable font advances changed correctly with `wght`, but the old layout path only used the font's legacy `kern` table format 0. Roboto Variable supplies the relevant pair positioning through GPOS, including variation data, and has no legacy `kern` table for those pairs. `To`/`Va` therefore had no adjustment in the old path.
- FreeType already applied the selected variable axis before measuring (`FT_Load_Glyph`) and extracting outlines. In the new check, Roboto's `H/i/m/.` advances differ among weights and the shaped single-glyph position and atlas advance exactly match FreeType at all checked weights. This rules out a stale default-master advance, measurement/draw axis mismatch, and an atlas key collision for the tested path.
- Measurement and outline extraction use the same unscaled, unhinted flags: `FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM`. The atlas uses those outlines with the software rasterizer; layout keeps advances in font units and scales them to floating-point pixels without per-glyph pixel rounding. No global rounding rule or FreeType hinting flag was changed.
- Static Segoe UI uses the nearest registered face policy. A 700 request returns the same single static face, with no synthetic bold; it does not exercise variable GPOS deltas. Other variable fonts expose different axis ranges (local Bahnschrift is 300–700; Roboto Flex is 100–1000).

### Changes and decisions

- Bundled HarfBuzz 15.0.0 from `lib/harfbuzz-15.0.0` through CMake and linked its `harfbuzz` target to `xui_text` using the `xui::harfbuzz` interface target. HarfBuzz utilities, subset, raster/vector/GPU features and optional Graphite2/GLib/ICU/Cairo integrations are disabled. FreeType's own HarfBuzz integration stays disabled; the text layer calls bundled HarfBuzz directly.
- Added a per-`Font` HarfBuzz face/font over the font's owned bytes. Variable `wght` updates set the corresponding HarfBuzz variation along with `FT_Set_Var_Design_Coordinates`, so GPOS variation deltas and FreeType outlines use the same effective axis value.
- `TextSystem` now shapes each left-to-right whitespace-delimited run with HarfBuzz and places glyph advances/offsets from the shaping result. Explicit whitespace remains under the existing wrap policy. `liga` and `clig` are disabled to preserve a simple editable-character/caret mapping. This is not an RTL/bidi or complex-script editing implementation.
- Added parent `font-weight` to the runtime's inherited text style set, including build-time inheritance, state/theme re-resolution, live style updates, and picker editor inheritance. A child `.xss` rule still overrides the inherited value.
- Trade-off: HarfBuzz adds a required bundled build dependency and more work on a text-layout cache miss, but provides general OpenType GPOS/variation support instead of a font-specific kerning patch. Cached steady-state text layout and glyph atlas behavior remain in place. No platform/backend API or graphics dependency was added.

### Verification

- Full Windows MSVC Release build completed for tests, benchmarks, and all example targets. CMake reports that HarfBuzz's CMake build is community-maintained and notes unavailable optional PkgConfig/Python tooling; the Xui targets compiled with warnings-as-errors and emitted no compiler warnings. CTest passes, and the test executable reports **197 tests, 0 failures**.
- Windows MSVC Release built `xui_tests` and `xui_glfw_demo` with warnings-as-errors. The full test binary reports **197 tests, 0 failures** with `XUI_VARIABLE_FONT=examples/layout/Roboto.ttf` and static `XUI_TEST_FONT=C:/Windows/Fonts/segoeui.ttf`; it also reports **197 tests, 0 failures** with bundled test font `RobotoFlex-Variable.ttf` plus Segoe UI. `XUI_VARIABLE_FONT=C:/Windows/Fonts/bahnschrift.ttf` was also exercised; its 900 request clamps correctly to its 700 axis maximum.
- For the checked-in `examples/layout/Roboto.ttf`, test diagnostics at size 28 print raw FreeType font-unit advance = HarfBuzz single-glyph advance = atlas advance:

  | `wght` | H | i | m | . | `To` GPOS adjustment | `Va` GPOS adjustment |
  |---:|---:|---:|---:|---:|---:|---:|
  | 100 | 1438 | 422 | 1836 | 439 | −1.35352 px | −0.628906 px |
  | 400 | 1461 | 498 | 1796 | 540 | −1.35352 px | −0.628906 px |
  | 700 | 1446 | 542 | 1774 | 594 | −2.36523 px | −0.628906 px |
  | 900 | 1439 | 563 | 1764 | 620 | −2.84375 px | −0.628906 px |

- Before the shaping integration, these Roboto `To` and `Va` pair adjustments were both zero at all weights. The changed `To` result demonstrates weight-sensitive GPOS; `Va` is nonzero but constant in this font. `Hamburgefontsiv` and `To Va` are also measured at each weight. The companion variable-font test checks distinct font/atlas entries.
- A second variable-font run used HarfBuzz's bundled Roboto Flex test font (axis 100–1000). Its `To` GPOS adjustment at 28 px was −1.27148, −1.79102, −2.05078, −1.91406, −1.65430, and −1.39453 px at weights 100, 300, 400, 500, 700, and 900 respectively; `To Va` runs measured 68.2227, 68.3867, 68.4688, 70.0547, 73.1992, and 76.3301 px. This verifies weight-specific positioning in a second variable font; the asset is used only for a local test and is not an Xui runtime dependency.
- The `.xss` runtime test verifies inherited weight on `Text`, plus child overrides on `Button` and `TextBox`, and renders all three. Existing text caret/deletion, wrapping, row clipping, and runtime tests pass as part of the 197-test suite.
- The latest hidden GLFW/OpenGL animation profile completed **1,979 frames in 5.03855 s (392.771 FPS)** with Roboto selected. This is an uncapped hidden-loop throughput sample, not visible FPS, resize cadence, or input latency. Inter and Source Sans 3 variable font files were not present in the repo or local font folder; they are unverified. No visible manual font comparison was available.

### Current status and next steps

- Tested variable TrueType fonts now use HarfBuzz OpenType GPOS positioning with the selected `wght` coordinate; static single-face fallback stays unchanged. The Windows Release test/build path is green.
- Remaining gaps: verify Inter and Source Sans 3 variable files when available; visually inspect weight-sensitive spacing in a visible GLFW run; run Linux/macOS builds; broader RTL/bidi and editable complex-script shaping remain out of scope. Continue to treat visible performance/resize criteria as unverified by the hidden profile.

## Variable-font weight axis support — 2026-10-23

> Historical implementation record. Its initial package setup and “verification pending” notes were superseded by the bundled-source integration recorded at the end of this file.

### Audit and root cause

- `.xss` already parses numeric `font-weight`, and `UiRuntime::selectFont()` already resolves a request against registered family faces. With one variable font file, that code still selected the same registered `FontHandle` for every weight.
- The text engine is a custom TrueType `glyf` parser, not FreeType. It read the default `hmtx` and `glyf` data and did not evaluate `fvar`/`gvar`/`HVAR`; therefore measurements, advances, and glyphs stayed at the default instance. This was not a parser/cascade bug.
- `GlyphKey` was keyed by glyph index and pixel size only. The atlas stored a font pointer for rebuilds but could return a cached glyph from another font/weight when both used the same glyph ID and size. This would mask distinct faces even after variable instances existed.
- Windows `C:/Windows/Fonts/bahnschrift.ttf` was inspected locally and contains `fvar`, `gvar`, `HVAR`, and `avar`, so it can serve as the local variable-font smoke input. It is not copied into the repository.

### Changes and decisions

- Added FreeType 2.9+ as a required dependency for the text target, with package setup in Linux/macOS/Windows CI and a vcpkg manifest for Windows. Static fonts continue using the existing parser, so their current path remains unchanged.
- Variable `glyf` fonts now expose the `wght` axis range, create cached `Font` instances per effective requested weight, apply coordinates using `FT_Set_Var_Design_Coordinates`, and use FreeType-evaluated advances and outlines for shaping/measurement/rasterization. Out-of-range coordinates clamp to the font axis range; other axes keep their defaults.
- Made atlas keys include a unique font-instance identity. Text layout already keys by font handle, so concurrent font weights now remain separate in both caches.
- `font-weight` without an explicit `font` family now applies to the default face too. Static fonts or fonts without `wght` retain the existing fallback policy: nearest registered face, no synthetic bold and no system fallback.
- Added a six-weight `.xss` parser test (100, 300, 400, 500, 700, 900), a variable-font smoke test that reports the effective coordinate and checks simultaneous instances/outline differences, and a static-font fallback test.
- Trade-off: correct variable-font deltas need a text dependency. FreeType was chosen over implementing `gvar`/`HVAR` in the small custom parser; the later bundled-source integration avoids requiring a separately installed package.

### Verification status and next steps

- `git diff --check` passes. The variable-font smoke asset is available on this Windows machine and its variation tables were confirmed.
- The local vcpkg install was rejected, so FreeType is not installed in the current build environment. A build/configure and the new tests could not be run. No claim is made yet that the implementation compiles, renders six visible weights, or preserves FPS/layout.
- Next: install the declared dependency, build with MSVC warnings-as-errors, run `xui_tests` and `xui_glfw_demo`, set `XUI_VARIABLE_FONT=C:/Windows/Fonts/bahnschrift.ttf` for the variable-font check, then record output and compare visible weight samples. Fix any compile/runtime issues before marking this feature as verified.

## Fluent 2 defaults, focus outlines, and checklist values — 2026-10-21

### Audit and scope

- The framework already had a broad `.xss` property set, declarative keyframes/transitions, transforms, gradients, glow/shadow, and nearest-registered font-weight selection. I kept those APIs intact instead of adding overlapping styling features.
- Missing items in the selected first slice were a coherent default visual baseline, paint-only focus outline properties, and an aggregate value API for `CheckList`. Grid/Stack runtime materialization and additional keyframe semantics remain deferred on the roadmap.

### Changes and decisions

- Added Fluent 2-inspired light defaults to the built-in `.xss` compilation path, covering the supported controls and auxiliary chrome: text, inputs, selection/range widgets, panels and splitters, scroll views, menus, docking parts, badges, toolbars/status bars, image placeholders, spinner, and FPS counter. They are ordinary compiled rules placed before user-authored rules, so normal specificity/order lets project styles override them. Existing base color tokens were preserved for compatibility; the Fluent defaults use their own palette values.
- Added `outline` and `outline-offset` to the style property table/compiler and paint path. The ring is painted beyond the widget box, so it does not affect layout. The built-in focus-visible baseline uses the brand color.
- Added `CheckListValue` (`std::vector<std::string>`) to `UiValue`. An identified list reads checked IDs of its directly nested identified `CheckItem`s in document order. `setValue` validates all IDs before applying changes and fires the parent's `change` callback once when requested. Item-level boolean values and events remain supported.
- Updated the demo callback to read the aggregate list value. Updated `.xss`/widget references and strengthened regression tests. Existing animation tests now sample the last painted control vertex instead of the newly themed window background; the old sample was not measuring the animated control after a default window surface was introduced.
- Alternatives considered: make Fluent defaults opt-in (less disruptive, but no coherent default styling), bake colors into widget paint code (breaks `.xss` ownership), or add a separate aggregate binding subsystem (more API than the selected value-retriever pattern). The compiled-rule baseline and typed value reuse the existing architecture.

### Verification and remaining gaps

- Windows MSVC Release build completed for `xui_tests` and `xui_glfw_demo`; direct test run: **191 tests, 0 failures**. This includes outline compilation/paint, Fluent defaults across the widget catalog, checklist aggregation/set validation/callbacks, and prior regressions.
- Visible GLFW uncapped profiles with the final baseline compiled measured **392.572 FPS** for the general showcase and **340.911 FPS** for the animation section over approximately five seconds. These are visible-window render-loop throughput, not manual refresh-rate, interaction-latency, or resize measurements.
- Before the follow-up fix below, the timer-driven profile produced **84 frames / 5.62184 s / 14.94 FPS**; a later pre-fix run took **19.55 s** for its five-second bound. This pointed to a due-timer wait problem. A temporary looped-meter profile also failed to return within its bound and was stopped; that profile-only change was removed.
- The desktop run verified renderer initialization and process exit, but not visual inspection of every control, pointer/keyboard interaction, live resize, or normal user close. Linux/macOS remain untested.
- Next: visually inspect theme and focus rings, manually exercise interactions/resize, and decide whether runtime Grid/Stack support becomes the next feature slice. The bounded active animation timer profile is now recorded below.

## Timer loop oversleep with due animation frames — 2026-10-21

### Root cause

- `UiRuntime::nextTimerDelay()` clamps an elapsed deadline to `0`. The platform contract defines `waitEvents(0)` as an indefinite block. If rendering passed the next animation deadline, `Xui::run()` could forward that zero and sleep until unrelated input arrived. The visible `--profile-animation-timer` reproduced it: a nominal five-second run lasted **19.55 seconds** and rendered only 61 frames (**3.12 FPS**). This was a real wait-path freeze/oversleep, not an intentional idle measurement.

### Fix and verification

- The run loop now calls `waitEvents()` only for positive timeouts. When a timer is already due, it immediately starts the next loop iteration so `updateTimers()` can process it. An actually untimed run still uses `waitEvents(0)` to sleep indefinitely, preserving low idle CPU.
- Rebuilt the Windows Release tests/demo and reran `xui_tests`: **191 tests, 0 failures**.
- Reran the visible timer profile after the final rebuild: **306 frames / 5.00862 seconds / 61.09 FPS**. The bounded run exits on time and clears the 60 FPS threshold on this machine. An earlier post-fix sample was 308 frames / 5.00139 seconds / 61.58 FPS. These are short Release profiles, not guarantees for every scene or device; manual resize/input pacing remains to be checked.
- The general visible uncapped profile remains **392.572 FPS**, and the animation uncapped profile **340.911 FPS**. These are throughput ceilings, distinct from the timer-driven result.

## Benchmark failures after Fluent defaults — 2026-10-21

### Root causes

- `bench_docking` used a hard-coded splitter point based on a zero-origin DockHost. The built-in Fluent baseline adds DockHost padding, shifting the local splitter hit area relative to that probe. The measured operation was valid; its fixture no longer specified the geometry it assumed.
- `bench_docking_scale` compiled its stylesheet inside `buildFor()` and returned while `UiRuntime` retained a pointer to that stylesheet. With the larger built-in default rules, the dangling pointer was exposed as an access violation during paint. The scale fixture also generated a missing quote in deep-tree target IDs and assumed panels 0 and 1 always shared an outer splitter, which is false for the wide tree.

### Fixes and verification

- Both docking microbenchmarks now explicitly set `DockHost` padding to zero so their geometry is deterministic and independent of visual defaults. The scale benchmark keeps the parsed document, stylesheet, string interner, and text system alive for the runtime's full use. Its deep-tree source now quotes targets correctly, it searches measured panel bounds for a real adjacent splitter, and it exits nonzero if a scenario cannot find one. The simple benchmark prints the measured bounds/probe if its hit test regresses.
- All Release benchmark targets built. `bench_style` compiled 575 rules in **7.10 ms** (30 ms budget); `bench_xui` parsed 2,001 nodes in **4.05 ms** (30 ms budget); `bench_layout` relaid out 938 nodes in **136.8 µs** (500 µs budget); `bench_paint` completed in **359.0 µs** (1,000 µs budget, 16 draw batches). `bench_docking` measured **0.77 µs** drag preview and **2.07 µs** splitter move. All six wide/deep scale scenarios completed; the largest reported paint case was **132.158 µs/frame** (32-panel wide tree).
- Smoke fuzz runs also passed: parser 10 iterations/30 mutations with no crash, layout 100 seeds × 10 steps, and paint 100 seeds × 70 primitives pixel-identical to naive order. These are bounded smoke runs, not the full long-running fuzzer defaults.
- The prior benchmark failure was a fixture failure, not a measured slowdown in those scenarios. The full suite is rerun after these edits; current benchmark and test evidence is Windows MSVC Release only.

## Animation timer cadence, cursor exit, cancellation policy, and F4 draw batches — 2026-10-21

### Audit and root causes

- The shared `Xui::run()` loop already checked `UiRuntime::updateTimers()` and waited through `Platform::waitEvents()`. Active tweens/keyframes scheduled timer wake-ups, so a mouse-stationary transition did have a timer path. However, `nextTimerDelay()` returned a fresh 16 ms interval every time, even when rendering had already consumed part of the interval. The loop therefore slept *after* render time instead of targeting a deadline. A visible timer-driven animation profile initially measured **31.30 FPS**; the equivalent uncapped visible render profile had previously measured **382.77 FPS**. That difference exposed the extra wait cost.
- GLFW never registered `glfwSetCursorEnterCallback`, and only queued `MouseMove`. When the pointer left the client area without a final in-bounds move, runtime hover stayed set. The existing timer fix could not animate hover-out because the state change was never delivered. Earlier tests exercised timers directly but did not cover GLFW's cursor-leave event path.
- The platform contract did not report hidden/minimized visibility. The run loop could not deliberately suspend those animation clocks. The chosen behavior is now explicit: continue while visible even when unfocused; pause while hidden/minimized; preserve animation elapsed time across the pause.

### Changes and decisions

- `UiRuntime::nextTimerDelay()` now uses the remaining time to its active animation deadline, not a fresh 1/60-second wait after every render. The timer test checks that a simulated 5 ms of frame work reduces the remaining delay. This keeps the event wait blocking and idle-friendly while avoiding an additive frame delay.
- GLFW now reports cursor enter/leave. `UiRuntime::pointerLeave()` clears hover-dependent state and tooltip targets without cancelling active drag operations. `Xui::run()` routes these events and restores the arrow cursor after exit.
- Added non-pure `Platform::windowVisible()` (default `true`) so existing custom platform implementations remain source-compatible. GLFW reports false for hidden or iconified windows. `Xui::run()` pauses animation clocks while invisible and skips hidden-window rendering; on restore, animation start/deadline timestamps shift by the pause duration. Visible-unfocused animations continue.
- Added `.xss` `animation-cancel: stop | finish` as a widget policy; omitted means `stop`. `finish` completes a current transition target before applying the new state, or finishes the current keyframe iteration before leaving its state. An animation can override the widget default with `cancel(stop)` or `cancel(finish)` in its `animation` declaration. Existing `animation-keep: until-done` remains available with its earlier retarget-to-new-state behavior. Imperative `cancelAnimation(id)` remains immediate.
- The Motion & Style demo now shows a `Finish hover-out` button and keyframe swatches using per-animation `cancel(finish)`. Normal demo startup no longer enables F3 automatically, so it returns to event-driven idle. F3 still opts into continuous FPS sampling.
- F4 now toggles the latest `Painter::Stats.drawCalls` count alongside the FPS text. This is the backend-neutral draw-list batch count, not GPU submissions. It updates on rendered frames and does not request continuous rendering. GLFW key mapping now includes F4.
- Added `--profile-animation-timer`, which selects the animated showcase page and uses the ordinary timer-driven loop; other `--profile*` runs remain bounded visible uncapped profiles. The demo profile is visible so minimized-window pausing does not produce a misleading zero-frame result.
- Updated animation, styling, performance, widget, and getting-started documentation with timer behavior, cancellation semantics, visibility behavior, profile modes, and the F4 metric.

### Verification and remaining gaps

- Windows MSVC/Ninja build completed for all default targets after the parser/runtime/API changes. CTest passed **1/1**; the test binary reports **188 tests, 0 failures**. Coverage includes no-input animation ticks, cursor leave, finish-current transition, finish-current infinite keyframe iteration, hidden-clock pause/resume, and F4 independence from FPS mode.
- The first visible `--profile-animation-timer` run, before changing the timer wait from a full interval to the remaining deadline, measured **210 frames / 6.70965 s / 31.30 FPS**. At the time, no post-change visible run had been completed; the follow-up zero-timeout fix and current timing are recorded in the next entry. The earlier **382.77 FPS** uncapped profile is a different mode and does not prove normal timer-loop cadence.
- No manual mouse, minimize/restore, or unfocused-window session was completed. Automated runtime tests verify state and clock behavior; the GLFW callback compiles, but desktop interaction should still be confirmed.
- Trade-off: custom `Platform` implementations default to always visible to preserve compatibility. They must override `windowVisible()` to pause clocks on their own hidden/minimized state.
- Follow-up: the bounded profile and zero-timeout wait issue are addressed in the later entry. Manual mouse, minimize/restore, pointer-exit and F4 checks remain.

## Custom titlebar duplication on maximize — 2026-10-21

### Audit and root cause

- The showcase has one authored `TitleBar`; runtime materializes it once and paints the caption buttons as an overlay on that bar. There is no maximize-specific tree insertion or second widget paint path.
- The Windows GLFW subclass installed `WS_THICKFRAME` for native snap/resize, but intercepted `WM_NCCALCSIZE` only when `wParam` was true. The false form fell through to GLFW/`DefWindowProc`, allowing default non-client layout during a maximize/restore frame transition. The custom maximize action itself uses GLFW's maximize/restore calls; it did not explicitly request window decorations.
- The renderer clears and paints the normal retained tree; source audit found no second titlebar draw caused by the double-buffer swap path. The taskbar is separate from the titlebar symptom; maximized client bounds are now explicitly taken from the monitor work area.

### Changes and decisions

- Kept the fix inside the Windows GLFW backend; `platform_interface` and the shared layout/runtime contracts are unchanged. `applySnapStyles()` now explicitly removes `WS_CAPTION` while restoring thick-frame/snap styles. Maximize and restore reassert those styles after GLFW's transition.
- The subclass now handles both `WM_NCCALCSIZE` forms for custom chrome and suppresses native `WM_NCACTIVATE` non-client painting. While maximized, it uses `MONITORINFO.rcWork`, keeping the taskbar visible and avoiding an invisible frame strip.
- macOS/Linux GLFW still intentionally retains native decorations because this backend has no implemented system move/resize operation there. Its authored `TitleBar` remains an in-client bar, so those platforms do not meet the “replacement titlebar only” behavior yet. This existing fallback is documented; no unsupported cross-platform window-drag API was invented in this fix.

### Verification and remaining gap

- Windows MSVC/Ninja rebuilt `xui_glfw_demo`; the full default build completed and CTest passed **1/1** (test binary: **183 passed, 0 failed**).
- The hidden GLFW menubar profile completed **3,066 frames / 6.25464 s / 490.196 FPS**. This verifies backend initialization and hidden-loop throughput only, not visible maximize behavior or input/resize latency.
- Windows desktop automation was unavailable (“Computer Use native pipe is unavailable”), so a screenshot-backed maximize/restore/minimize/close check could not be completed. macOS/Linux builds and visible runs were also unavailable. Do not mark the visual issue fully verified until those runs are recorded.
- The user-facing TitleBar platform caveats in `doc/widgets.md` now describe the Windows non-client handling and maximized work-area behavior.

## Recent-files submenu paint freeze — 2026-10-21

- Reproduced the freeze path in `UiRuntime::paint()`: while an open submenu was painted, the `while (nested)` traversal selected that same root submenu again on every pass because it did not exclude nodes already marked `submenuPainted`. `nested` was therefore reset to true forever. Opening the showcase's “Recent files” submenu caused the redraw to hang the UI thread.
- The submenu traversal now skips `submenuPainted` items. This preserves parent-before-child ordering for recursive submenu painting while allowing each open submenu surface to be painted exactly once per pass.
- Added a regression assertion to `ui_runtime_menu_anchors_items_keyboard_and_outside_close`: it opens the nested menu, paints a frame before choosing a submenu item, and verifies that drawing completes with geometry. This specifically covers the previously non-terminating paint path.
- Verification: rebuilt `xui_tests` and `xui_glfw_demo`, then rebuilt all default Ninja targets. CTest passed **1/1**; direct test output reports **183 tests, 0 failures**. No visible interactive GLFW session or FPS measurement was available, so desktop confirmation and visible performance remain unverified.
- Trade-off: a per-frame marker is used rather than restructuring menu traversal; the existing retained hierarchy and popup z-order remain intact.
- Next: open the GLFW demo and click Menu → Recent files, move into the submenu, select an enabled item, then verify normal close and responsiveness at visible frame rate.

## MSVC errors and anchored menu regression — 2026-10-21

- Fixed the screenshot's `glfw_platform.hpp` parse cascade by qualifying the resize inset types as `xui::f32`. The subsequent missing-member and `resizeInset` errors in `glfw_platform.cpp` were parser fallout from the unqualified type, not separate API mismatches.
- Fixed warnings promoted to errors in `runtime.cpp`: renamed the fallback stepper mark, used the correct `DockHostState` argument for dock damage, and removed local-name shadowing in stepper input and `setValue` branches.
- Repaired compile errors in the existing test source (a lambda mistakenly stored as `Vec2`, packed RGBA passed where `Color` is expected, duplicate local name, and malformed duplicate label syntax). Updated the docking-scale benchmark helper to build into its caller-owned `UiRuntime`, which is non-movable.
- The full test run then exposed a real menu bug: an anchored `Menu` used its structural node's layout rectangle for opening, popup placement, and outside/anchor hit checks. The menu node is intentionally excluded from normal layout, so clicks on the authored anchor could not open it and popup rows were placed incorrectly. Those paths now use the resolved `menuAnchorHitRect()` rectangle.
- Decision: fix the shared anchor geometry helpers in runtime rather than assigning fake layout bounds to the Menu node. This keeps popup geometry, painting, and input based on the actual authored anchor and does not perturb layout or backend code.
- Verification: full Windows MSVC/Ninja build completed all default targets, including `xui_tests`, `xui_glfw_demo`, demos, and benchmarks. CTest passed **1/1**; direct test output reports **183 tests, 0 failures**. `git diff --check` reports no whitespace errors (only standard LF/CRLF normalization notices).
- Remaining boundary: this verifies compilation and headless tests. No visible demo session or FPS measurement was run in this fix pass, so visual menu placement, desktop interaction, and the >60 FPS user-visible target remain subject to the project's existing M0 verification gap.
- This follow-up supersedes the earlier “full target rebuild blocked” note below: the identified compile errors were resolved and the full build now passes.

## Unused resize-inset parameter warning — 2026-10-21

- Removed the parameter name from the default no-op `Platform::setResizeInset(WindowHandle, f32)` declaration. Concrete backends that use the inset retain their named parameters; the interface default no longer triggers MSVC C4100 when warnings are treated as errors.
- `runtime.hpp` has no function parameter named `pixels`; its `pixels` occurrence is in a comment. No unrelated runtime API change was needed.
- Full target rebuild was attempted. It is currently blocked by independent errors in the existing working-tree edits: `glfw_platform.hpp` uses unqualified `f32`, and `src/ui/runtime.cpp` reports an undeclared `mark`, invalid `addDockDamage(u32)` call, and two shadowing warnings under `/WX`. These are outside the reported unused-parameter fix and were left untouched.
- Next: correct those independent compile issues in their owning changes, then rebuild all targets with warnings-as-errors.

## Animation freeze root cause, borderless resize, popup menus, NumberField — 2026-10-21

### Audit and findings

- **Animation freeze (repeat failure, treated as a bug first):** the 2026-10-18 fix addressed the *scheduler* (timer wake-ups, paused keyframes) and was genuinely correct — `updateTimers`/`nextTimerDelay` sustain active animations without input. The real defect was in `paint()`'s transition-tween loop: a tween whose elapsed time reached its duration was **erased without applying its final value**. The last painted frame showed the ~97% sample; the tween list then emptied, the timer loop went idle, `nextTimerDelay` returned −1, and the widget froze short of its target until the next input-driven state change re-resolved the style. Exactly the reported signature: freezes without mouse movement, resumes on input, no idle CPU burn. Keyframes were never affected (they paint `progress = 1` on completion); only hover/press state tweens froze.
- **Borderless resize:** confirmed against the bundled GLFW 3.5.1 source — `getWindowStyle()` gives undecorated windows `WS_POPUP` with **no `WS_THICKFRAME`**, so `DefWindowProc` never enters the native sizing loop even when our `WM_NCHITTEST` returns `HT*`. Drag worked (native `HTCAPTION` modal loop) while resize was dead.
- **Dropdown vs popup menu:** the existing `Dropdown` is a value selector (flat pipe-separated options, committed value changes the control's own text) with no separator/disabled/checked/submenu support; overloading it would have risked the documented dropdown regressions. Per the user's decision, popup menus are a new `Menu`/`MenuItem` widget reusing the dropdown's popup plumbing (anchored overlay, virtual rows, outside-click close, topmost-row hit testing).
- **NumberField:** audit showed it already reuses the `TextBox` editing path (caret/selection/focus/blink), fires `change` per valid keystroke (per-keystroke `commitNumberText`), and has steppers, arrow keys, and clamping. Gaps: no invalid visual state, fixed stepper visuals.

### Changes

- **Animation:** completed tweens now paint `to` exactly before retiring, then the loop returns to idle (regression test drives a transition to completion with no input and asserts the final color plus an idle timer path). New per-widget `.xss` property `animation-keep: none | until-done` (`PropId::AnimationKeep`): `until-done` keeps a mid-flight transition animating to the new state's values instead of snapping when the state exits, using the first declared `transition` timing on that widget. Keyframe/one-shot/loop/cancel semantics unchanged.
- **Borderless resize:** the GLFW chrome subclass now also restores `WS_THICKFRAME` (with the snap boxes) so the native sizing loop engages from all edges/corners; `WM_NCCALCSIZE` keeps the frame invisible. New `Platform::setResizeInset(WindowHandle, f32)` sets the hit-test margin in physical px (default = native frame metric, clamped 0–64). Per-edge cursors come from the OS because the edges are non-client.
- **Popup menus:** `Menu` (anchor label, `anchor: "<id>"`, `context: true` for right-click) and `MenuItem` (label, `press`, `shortcut`, `checked`, `disabled`, `-` separators, one submenu level via nested `MenuItem`s). Rows are retained nodes kept out of the layout tree; geometry (popup placement, flip when clipped, row rects) is computed on demand and shared by painting, hit testing, and keyboard navigation. Routing: menu popups hit-test above all other popups; right-press only reaches context menus; opening any menu closes dropdowns/other menus and vice versa; press on the open anchor toggles. Keyboard: Up/Down (enabled rows only), Enter, Escape (closes submenu first), Left/Right for submenus, hover-open for parents. Value API: `value(menuId)` = last activated item; `setValue(menuId, bool)` open/close; `setValue(itemId, bool)` check/uncheck; item `press` fires, falling back to the menu's handler. Painted through the `Menu`/`MenuItem` selectors with `resting`/`hover`/`checked`/`disabled` states, checked mark, shortcut column, and submenu arrow.
- **NumberField:** unparseable text now sets the `.xss` `error` state (existing `State::Error`) and keeps reporting the last valid value; losing focus with invalid text reverts the editor to the canonical last-valid text. Stepper buttons paint through the styleable `NumberButton` part (`resting`/`hover`/`pressed`), shared with the `Stepper` widget, with the previous built-in look as fallback. Live per-keystroke commit behavior is documented (it already existed).
- **Demo:** the showcase TitleBar gains a Menu button with an anchored menu (items, separator, checked row, disabled item, shortcut, submenu) and a right-click context menu on the section tabs; `onMenuAction` reports the activated item in the action readout. Styles added to `showcase.xss`.
- **Tests:** tween-completion regression (final color + idle-after-completion), menu behaviors (disabled rows stay open, activation + value, Escape, outside close, keyboard activation, submenu), NumberField (live per-keystroke commits, invalid blur revert, arrow stepping). 34 tests total in the binary once compiled.

### Decisions and trade-offs

- The freeze fix was made at the paint site (apply `to` before erase) rather than keeping completed tweens alive — keeping them alive would have kept the timer loop ticking for no visual change.
- `animation-keep` reuses the first declared transition's timing for properties the new state does not itself transition — simple and predictable; a per-property timing override was rejected as over-configured.
- Menu rows are virtual (never appended to the layout tree), matching the dropdown/picker pattern; this keeps menu items out of hit testing, timers, and layout at the cost of not being individually addressable by selectors.
- Menu activation happens on press (not release), consistent with the existing dropdown rows.
- Submenus are limited to one nesting level; deeper chains need a recursive overlay design (documented).
- The resize inset is physical pixels (what `WM_NCHITTEST` works in) and backend-level, not `.xss`-styleable: window edges are non-client, so the `.xss cursor` option does not apply there.

### Verification status

- **Not yet compiled or run.** All four changes are code-complete with headless tests written. Required pass: framework targets, `xui_tests` (37 total test cases expected incl. the 3 new ones), `xui_glfw_demo` visible run — borderless resize from all edges/corners with correct cursors, menu open/navigate/activate with z-order above other popups, animations running with the mouse still and resuming correctly after completion, NumberField live/invalid/steppers, plus the standing regression checklist.

### Next steps

- Build + fix reported compile errors; run the suite; visible demo pass per the checklist above.
- Revisit the paused docking-optimization task (scale benchmark staged in `benchmarks/bench_docking_scale.cpp`).

## Custom window chrome: `platformTopMenu` + `TitleBar` — 2026-10-19

### Audit and findings

- Task: hide the demo window's native decorations and provide a custom draggable top menu with real window controls, as reusable framework capability rather than demo-only code. A prior decision set the placement: the option lives as a `.xui` `Window` attribute (`platformTopMenu: false`), the framework provides optional caption buttons, and platforms that cannot host custom chrome keep native chrome.
- Audited `include/xui/platform/platform.hpp` (no decoration/drag/window-control surface existed), `src/ui/xui.cpp` (window creation happens before `runtime.build()`, so the document must be consulted first), the GLFW backend (GLFW 3.5.1; `glfw3.h` no longer includes `<windows.h>`), and the runtime's widget materialization/dock-chrome patterns (`DockHeader`/`DockCloseButton` style parts, `Tabs` header reserve trick).

### Changes and decisions

- **`platform_interface`:** added `WindowDesc::decorations`, `WindowEdge`, and `Platform::setDecorated/beginSystemMove/beginSystemResize/minimizeWindow/toggleMaximize/windowMaximized/closeWindow` with conservative default implementations, so custom backends keep compiling. All window actions flow through this interface; no GLFW/OS calls in the demo, runtime, or widget code.
- **GLFW backend:** on Windows, a `WM_NCCALCSIZE`/`WM_NCHITTEST` subclass strips the native frame while keeping resize borders; `WS_MAXIMIZEBOX|WS_MINIMIZEBOX` are re-added so Aero Snap survives decoration removal. `beginSystemMove/beginSystemResize` post native `WM_NCLBUTTONDOWN` (`HTCAPTION`/edge hits) so drag, snap, drag-to-restore, and modal resize all stay native; the existing live-resize hook keeps rendering during snap resizes. Minimize/maximize/restore/close use GLFW calls on every platform. On non-Windows, `setDecorated(false)` refuses (GLFW has no window-move API), `createWindow` keeps decorations, and the fallback is reported to callers.
- **Runtime:** new `TitleBar` widget (horizontal Row container). Caption zones (minimize/maximize/close, 46 px each) are framework-painted at the bar's right end from a styleable `TitleButton` style part (`resting`/`hover`/`pressed`); the bar's right padding is reserved so authored children cannot underlap the zones. Drag starts only on non-interactive areas (hit-test aware: interactive children keep clicks; non-interactive children drag). Double-click toggles maximize and is detected press-to-press because native move loops swallow the first release. Caption buttons trigger on release-inside like native buttons. `titleBarRegions()` exposes geometry for tests/tools.
- **Document contract:** `Window[platformTopMenu: false]` requires at least one `TitleBar`, and a `TitleBar` requires the attribute — either mismatch is a `UiRuntime::build` error, so a window can never end up with neither native nor custom chrome. `documentUsesClientChrome()` lets the facade read the attribute before window creation (no native-chrome flash).
- **Facade:** `Xui::attach` derives `WindowDesc::decorations` from the document, wires `runtime.setPlatformWindow(platform, window)`, and clears it on shutdown.
- **Demo:** `showcase.xui` now sets `platformTopMenu: false` and replaces the client-area `MenuBar` row with a `TitleBar` holding the same File/Edit/View dropdowns, a Help button, and a fill title text; `showcase.xss` styles `TitleBar.top-menu`, `TitleButton`, and the title. All menu IDs are unchanged, so demo/profile event wiring still applies.
- **Tests:** added two runtime tests — caption-zone geometry plus close/maximize-restore/minimize actions (release-inside semantics, release-outside cancel), drag from empty bar and non-interactive title text, no drag from interactive children, double-click toggle, and a paint smoke; and the `platformTopMenu`/`TitleBar` contract errors plus `documentUsesClientChrome`. `NullPlatform` records chrome calls for assertions.

### Trade-offs

- Windows-only custom chrome: hiding decorations elsewhere (macOS/Wayland) would leave an unmovable, unresizable window — worse than the native chrome it replaces. The fallback keeps a fully functional window and the `TitleBar` degrades to a plain bar. Manual move via `setPosition` polling was rejected (janky, and blocked outright on Wayland).
- GLFW's own `GLFW_DECORATED` attribute drives Win32 styles; the subclass only intercepts `WM_NCCALCSIZE`/`WM_NCHITTEST` and re-adds snap styles, keeping one style path. If GLFW ever reapplies styles on a path we do not hook, worst case is a restored native caption, not a broken window.
- Caption glyphs are painter primitives (minimize/maximize/restore) plus the proven `×` label for close, avoiding font-coverage risk for geometric glyphs across Segoe UI/Arial/DejaVu.
- Press-to-press double-click detection accepts that a native drag can start on the first press; the drag threshold and the modal move loop make accidental toggles require a real double press without movement.

### Verification status

- **Not yet verified: none of this code has been compiled or run.** Compile fixes so far came from user-run builds: missing `<windows.h>` include in the GLFW header (GLFW 3.5.1 no longer provides Windows types), and an invalid `this` in the static `installChrome`. A further build/test pass (framework targets, `xui_tests`, `xui_glfw_demo`) is required before any claim lands; visible drag/snap/resize/close behavior on Windows and the non-Windows fallback need a desktop session.

### Next steps

- Build framework targets + tests, fix fallout, run the full suite; run `xui_glfw_demo` visibly: no native title bar, drag from the menu, double-click maximize, min/max/close, edge resize, interactive children not dragging, FPS above target during drag/resize, no idle CPU burn, and the standard regression checklist (docking, animations without input, caret, color picker, `Row` clipping, resize freeze, cursor icons, float visibility).
- Revisit the paused docking-optimization task (scale benchmark staged in `benchmarks/bench_docking_scale.cpp`); profile numbers must come before any "optimized" claim.

## Showcase rewrite and layout coverage — 2026-10-16

### Audit and findings

- Re-read `plan.md`, the runtime widget materialization list, layout schema/solver, current showcase assets, GLFW callbacks, widget/layout guides, and the showcase asset test. Runtime supports the controls/containers enumerated in `doc/widgets.md`; `Tooltip` is an attached behavior, while `Grid` and `Stack` remain solver-level only.
- The prior showcase named most controls but mixed categories, repeated IDs (`density`, `save-key`, readouts), left several value changes without a visible value display, and did not demonstrate absolute positioning through `.xui`. The layout engine already enforced authored min/max floors for containers and Split panes, but showcase-level regression coverage for `.xui` and `.xss` floors was incomplete.

### Changes and decisions

- Rebuilt `examples/showcase.xui` and `examples/showcase.xss` as six deliberate tabs: Layout, Text input, Choices, Popups, Display, and Containers. The structure is authored in `.xui`; the coherent theme, spacing, typography, borders, and widget states are in `.xss`. Existing tab order and useful IDs remain stable for the test/demo contract; duplicate IDs were removed.
- Added visible examples of fixed/percentage/clamped sizing, both XUI and XSS minimum floors, flow and absolute positioning, rows/columns, alignment/justification, spacing/insets, nesting, and horizontal/vertical Split. Added vertical and horizontal ScrollViews and demonstrations for all runtime-backed widget/container types; Tooltips remain attached to target widgets and styled through the `Tooltip` selector.
- Value callbacks now update adjacent text for text entry, checkbox/checklist, switch/toggle, radio/segmented selection, slider/progress, dropdown, color picker, date picker, keybind, breadcrumb, rating, stepper, scroll offset, selected tab, and action count. C++ only handles the event/value bridge; it contains no demo layout or theme values.
- Exposed the layout engine's existing absolute-child support through backend-neutral `.xui position: Absolute` and logical `left`/`top`/`right`/`bottom` insets. The demo reserves flow width so the anchored badge does not overlap text. Did not expose Grid/Stack because their track/stack runtime integration is a separate unfinished capability; docs name this limitation.
- No layout solver rewrite was needed: its explicit floors and root-shrink behavior already existed. Added runtime showcase assertions for `.xui minSize`, `.xss min-size`, nested containers, split pane floors, root shrink, absolute inset placement/non-overlap, vertical Split, and a complete runtime-widget inventory. Keeping the existing floor implementation avoids unnecessary solver risk while making its behavior regression-tested.

### Trade-offs and verification

- A six-tab categorized gallery keeps each view manageable; longer sections use ScrollView so the complete gallery remains reachable at smaller window sizes. This is a usable reference, not a claim that every unsupported schema token renders.
- `.xui minSize` accepts a fixed pixel pair; individual `.xui minWidth`/`minHeight` and `.xss min-size`/longhands accept percentage constraints. Floors are explicit rather than implicit for every container. When a min constraint exceeds parent space, geometry can overflow; scrollable parents keep content reachable. Split panes can jointly satisfy floors only when the Split is sized to fit their sum and gutters.
- Windows Release build succeeded for `xui_tests` and `xui_glfw_demo`. The suite passed: **159 tests, 0 failures**, including full showcased-type presence, per-tab layout, min floors at 1080×760 and 400×300, absolute placement without overlap, and vertical/horizontal split/scroll examples.
- Headless GLFW/OpenGL profile after the final asset/ID cleanup: **2,307 frames / 5.0008 s / 461.326 FPS**. This confirms the bounded hidden profile loop is above 60 FPS, not visible interactive FPS.
- No desktop screenshot or mouse/keyboard session was available in this run. Actual visual readability, all-widget appearance, popup interactions, live resize smoothness, and manual close remain unverified. Do not report the >60 FPS target as visually accepted.

### Current status and next steps

- The showcase assets parse, build, and lay out across all six tabs; state/value wiring and explicit minimum/positioning behavior are covered by automated tests.
- Perform a visible GLFW smoke pass across all tabs, then exercise hover/focus/press/disabled, typing/editing, popup interactions, scroll, horizontal/vertical split drag, continuous resize, and normal close while watching FPS. Capture representative normal and narrow-window views.
- Decide whether to implement XUI runtime materialization for Grid/Stack after this showcase slice; until then, they remain lower-level solver capabilities and are explicitly excluded from runtime widget coverage.

## Documentation consolidation — 2026-10-16

### Audit, changes, and decisions

- Audited 40 Markdown files outside generated build/output and Git directories: Xui root/docs/plan files plus the bundled GLFW project's own documentation. No identical project-owned files were found. Most duplication was repeated setup, backend, conventions, and status summaries. GLFW Markdown remains third-party and out of scope for edits.
- Moved the detailed root tracking history into this canonical `plan/tracking.md` file without dropping its historical entries. The root `tracking.md` now links here as a compatibility stub. Replaced the stacked, conflicting plan snapshots with the current structured roadmap in `plan/plan.md`; root `plan.md` is likewise a compatibility stub.
- Kept `doc/architecture.md` as the user-facing architecture/backend guide. Replaced the former `plan/architecture.md` pointer with the agent source map and change boundaries, which link to the public guide rather than repeating its data-flow/interface content.
- Shortened the root README into an overview and links. Configuration, dependency, CMake, and demo details moved to `doc/getting-started.md`. Contribution workflow remains in `CONTRIBUTING.md`; duplicated architecture/style/widget/performance instructions now point to the architecture guide and agent conventions.
- Kept the project guide files in `doc/` as separate topic references and made `doc/README.md` list each. `plan/README.md` lists each agent-context file. No project Markdown was deleted: every project guide has a defined index/workflow role; root roadmap/tracking compatibility stubs protect existing inbound links. Vendored GLFW docs stay with the dependency and its docs build.
- Updated stale documentation: ScrollView is now listed as runtime-supported in the `.xui` guide; the troubleshooting color-picker section now reflects alpha support; performance guidance describes the current live-resize hook and links to the tracker for measurements rather than repeating profile numbers.

### File disposition

- **Moved:** the full root `tracking.md` history is now `plan/tracking.md`; the current roadmap content is canonical in `plan/plan.md`.
- **Merged:** root README setup, dependency, build, and configuration details moved into `doc/getting-started.md`; repeated contribution guidance now links to canonical architecture and conventions guides. No unique content was intentionally discarded.
- **Removed:** no project Markdown files were deleted. Root `plan.md` and `tracking.md` remain short compatibility pointers; the former `plan/architecture.md` pointer is now an agent-facing source map.

### Trade-offs and remaining gaps

- Keeping short root stubs costs two tiny files but avoids breaking old in-repository links and any external bookmarks to `plan.md` or `tracking.md`. The full canonical contents live only under `plan/`.
- `CONTRIBUTING.md` still links the canonical technical guides rather than duplicating their checklists. The root README retains its existing major section headings as lightweight pointers for stable section anchors.
- Internal Markdown file links and heading anchors resolve after the move. External backlinks cannot be discovered from this checkout; compatibility stubs are the mitigation. Vendored GLFW docs were not content-audited for Xui accuracy because they describe upstream GLFW.
- No framework code changed. Documentation-only validation is link/index/duplicate review; no build or runtime test is needed.

### Current status and next steps

- Canonical homes: user guides in `doc/`, agent rules and status/history in `plan/`, concise navigation in root `README.md`, contribution workflow in `CONTRIBUTING.md`.
- Both `doc/README.md` and `plan/README.md` enumerate every Markdown file in their respective folders. No project Markdown file was deleted; the three former pointer files were repurposed as canonical plan/history and agent source-map files. Keep the history here and current status in `plan/plan.md`.

## Initial agent-context setup — 2026-10-16 (superseded by documentation consolidation below)

### Audit and decisions

- The repository already had a root `plan.md`, a root `tracking.md`, and the current architecture guide at `doc/architecture.md`. Existing contribution notes, README sections, docs, and historical tracking entries link to those locations. No root `AGENTS.md` or `CLAUDE.md` existed.
- Created a root `AGENTS.md` pointer and a `plan/` folder with `agents.md`, `conventions.md`, and an index. Added `plan/plan.md`, `plan/architecture.md`, and `plan/tracking.md` as links to the canonical files rather than moving or copying them. This preserves every historical link and avoids divergent duplicate plan/tracking content.
- Updated the root README project tree/docs section and CONTRIBUTING entry point so agents can discover the rules. No framework code or runtime behavior changed.

### Current status and verification

- `plan/agents.md` now records the `.xui`/`.xss` and backend invariants, the supplied anti-regression list, root-cause and verification rules, the >60 FPS/live-resize target, and required documentation updates.
- `plan/conventions.md` documents current C++ practices, asset responsibilities, widget/property implementation paths, tests, and verification boundaries; it links to contributor and user guides for details instead of duplicating them.
- Verified that canonical existing documents remain at their original paths and that the new index/pointer files target those paths. Since this is a documentation-only change, no code build or runtime test was run.
- Next: agents should follow the new entry point on future work; keep the guidance synchronized when architecture, verification targets, or contribution conventions change.

## Live resize: modal-loop root cause and frame hook — 2026-10-15

### Root cause (why the earlier fixes never took effect)

- Interactive window resize on Windows runs an OS **modal sizing loop** inside `glfwPollEvents()`/`glfwWaitEvents()`. GLFW delivers size callbacks *during* that loop, but the application's frame loop does not regain control until the drag ends. Every earlier fix lived in the facade (`Xui::run`) — per-event relayout, `requestRedraw` on Resize, coalescing in the event loop — and was therefore unreachable mid-drag: the Resize events queued correctly, but the loop that would consume them and redraw was not running. The same applies to the earlier coalescing guard (`m_polledForCurrentDrain`): it prevents event starvation, not the modal trap.
- Secondary consequence: time-driven widgets (FPS counter, spinner, indeterminate progress, caret blink) also froze mid-drag, because their updates advance inside `paint()`/`updateTimers()` on the same stalled loop.

### Fix

- `Platform` gained an optional `setLiveResizeHook(WindowHandle, fn, context)` (default no-op; `NullPlatform` inherits it, so headless behavior is unchanged). Backends whose resize is modal invoke the hook from their size callback to render each live step; non-modal backends never need it.
- `GlfwPlatform` implements it per-window (hook stored on the `Window` record) and calls it from `onSize` after queueing the `Resize` event.
- The facade extracted its render path into `Xui::renderFrame()` — fresh size/DPI query, renderer backbuffer resize on change, layout, paint, submit, present — and registers `renderFrameThunk` in `attach()` (cleared in `shutdown()`). The normal loop now calls `renderFrame()` when a redraw is pending; the hook calls it mid-drag.
- **Animated widgets stay live during resize** because the hook runs the *complete* paint path: the FPS counter, spinner phase, indeterminate sweep, and caret blink all advance inside `paint()` on every hook invocation.

### Trade-offs and notes

- Rendering from inside a GLFW window-size callback is the standard technique for live resize on Windows (same thread, context already current). Each hook call presents one frame; there is no vsync re-entrancy because `glfwPollEvents` is not re-entered.
- Frames rendered mid-drag increment `m_stats.frames`, so profile FPS now includes resize frames (arguably more honest than excluding them).
- Hook fires on any size change (restore, maximize, programmatic `setSize`) — one extra presented frame in those cases; harmless.
- Remaining gap: the hook renders but does not drain queued mouse-move events mid-drag, so hover state updates only after release; acceptable for live resize and unchanged from prior behavior.

### Verification and current status

- Not executed this session. Verify: `xui_glfw_demo` — drag the border continuously: layout re-flows every step, no freeze, FPS counter/spinner keep updating during the drag, release ends cleanly; `xui_hello` same. `xui_tests` must stay 158/158 (NullPlatform path untouched). Regression checklist: slider realtime, dropdown hover, color picker, tooltips, caret blink unaffected (event paths unchanged after drag end).

## Cursor mapping fix + blue theme rewrite — 2026-10-15

### Cursor audit and fix (`UiRuntime::cursorAt`)

- Before: scrollbar tracks already returned `ResizeEW`/`ResizeNS` and editable fields `IBeam`, but every other interactive widget fell through to `Arrow` — buttons, checkboxes, switches, toggles, radios, dropdown/pickers, segmented, rating, breadcrumb, steppers, keybind and date fields, and tab headers showed the plain arrow; sliders had no resize cursor; split gutters had none; disabled widgets still advertised `IBeam` on text fields.
- After, in precedence order: disabled → `Arrow` (never advertise interaction); split-gutter rects → `ResizeEW`/`ResizeNS` matching the drag axis handled in `pointerDown()`; `Slider` → `ResizeEW`; editable+non-readonly → `IBeam`, with the SearchField clear glyph now `Hand` (was `Arrow`); all clickable controls (button/check/switch/toggle/radio/dropdown/color/date/segmented/rating/breadcrumb/stepper/keybind/tabs) → `Hand`; everything else `Arrow`. Verified the GLFW backend already maps `Hand`/`ResizeEW`/`ResizeNS` to standard cursors; no backend change needed (`NullPlatform` is a no-op).
- Read-only fields now show `Arrow` instead of `IBeam` (they are not editable); scrollbars unchanged.

### Blue theme

- `showcase.xss` fully rewritten around a deep-blue palette: navy canvas `#0e1a2e`, panels `#14243d`/`#1a2f4f`, vivid accent `#4d9fff` (hover `#79b8ff`), consistent radii (4/6/8 px), tooltips and selected tabs accented, scrollbar thumbs in the blue family. Selector set unchanged — every class the `.xui` uses keeps its properties, so only colors moved.
- Decision: the `.xui` structure/ids were **kept** rather than rewritten from scratch. The suite hard-codes them (value lookups for ~24 ids, tab-header fractions, `split.h > 300`, input-row ordering), a structural rewrite could not be compile/test-verified in this session, and the last audit already showed full widget coverage and no overlap. The from-scratch visual work is entirely in the `.xss`; live value reflection already flows through the value API into readout `Text` nodes on every tab.

### Verification and current status

- Not executed this session (owner builds). Expected: suite unchanged (no schema/id changes; the cursor function has no test coverage — a gap), demo renders in the new palette. Visible pass should check: `Hand` over buttons/menus/pickers, `ResizeEW` over slider tracks and horizontal split gutters, `ResizeNS` over vertical gutters and scroll thumbs, `IBeam` only in editable fields, `Arrow` over disabled/read-only controls and panels.
- Open: cursor behavior has no unit tests (`cursorAt` is public and testable); deeper `.xui` restructuring remains available if wanted once a build/verify loop is available.

## Demo polish: password mode, layout audit, theme consistency — 2026-10-15

- **Coverage audit:** every runtime-supported widget kind is demonstrated in the showcase — containers (Window, Column, Row, Panel, Split, CheckList, Tabs/Tab, Toolbar, StatusBar, ScrollView vertical/horizontal/nested), inputs (TextBox, TextArea, SearchField, NumberField, Stepper, CheckBox, Switch, ToggleButton, RadioGroup, SegmentedControl, Slider, Dropdown, ColorPicker, DatePicker, KeybindField), display (Text, Label, Badge, Separator/Divider, Image, Spinner, ProgressBar determinate + indeterminate, FpsCounter), and tooltips in several trigger/delay configurations. The one gap was the **password TextBox mode** — parsed, schema-validated, and styled but never shown; now added.
- **Overlap audit:** budgeted every tab vertically (page usable ≈ 584 px inside the 620 px Tabs at the test's 1080×760 layout): Input ≈ 504, Controls ≈ 535, Layout fixed ≈ 262 with the Split flexing to ≈ 322 (the suite's `split.h > 300` still holds), Widgets ≈ 446, Scrolling fixed ≈ 362 within a growing row. No overflow; sequential Row children cannot overlap each other — the only risk was window-edge clipping at minimum size.
- `showcase.xui`: added a password `TextBox` (`id: "passphrase"`, `password: true`, change handler) as the third field of the input row; row widths rebalanced to 380/200/170 so three fields fit; no other layout changes.
- `showcase.xss`: radius consistency only (SearchField/Stepper 3→4 px, Image 3→6 px). The committed blue-grey theme already reads as one system (shared tokens, one accent, state blocks on every widget); a deeper redesign without a visible run would be churn.
- `glfw_demo.cpp`: `onPassphraseChange` handler (reports the passphrase *length* in the title — a password demo that echoes the secret would model the wrong behavior) and `minWidth` 760→900 / `minHeight` 640 so the three-field row never clips at minimum size. Trade-off: below 900 px the input row would exceed the window edge (rows lay children sequentially; fields never overlap each other, but the flow engine does not wrap yet).
- Not executed in this session; expects the suite to stay green (the showcase test parses/builds the new node; no assertion covers the input row's internal widths) and a visible pass to confirm masking, the length readout, and no overlap at 1080×800 or the 900×640 minimum.

## Demo asset packaging cleanup — 2026-10-15

- Audited the runtime loads in `examples/glfw_demo.cpp` against the CMake post-build copy rules. Both `xui_glfw_demo` and `xui_hello` load only `showcase.xui` and `showcase.xss`; copying `cross_platform.*` and `controls.*` beside those executables was unnecessary.
- Removed those four redundant copy commands from `examples/CMakeLists.txt`. The demo output now receives only the `.xui`/`.xss` pair it actually opens.
- Kept `cross_platform.*` and `controls.*` in the source tree because `tests/test_runtime.cpp` loads them and the docs link to them as smaller reference examples. Deleting them would remove active test fixtures and documented examples, rather than clean up unused files.
- Trade-off: runtime deployment is smaller and its asset contract matches the code; tests/docs continue to rely on source-tree examples. No parser/runtime or visual behavior changed.
- Verification: regenerated and built `xui_tests`, `xui_glfw_demo`, and `xui_hello` with the Visual Studio CMake/Ninja toolchain. `xui_tests.exe` passes **158/158**. Removed stale generated copies from the verified in-workspace output directory; it now contains only `showcase.xui` and `showcase.xss` among `.xui`/`.xss` files. No new FPS or visual claim is made for this packaging-only change.
- Next: no packaging follow-up is needed unless additional demo launch profiles are added; keep CMake runtime asset copies in sync with actual `loadXui`/`loadXss` calls.

## Showcase theme, intrinsic sizing, and widget coverage — 2026-10-15

### Audit and findings

- `examples/showcase.xss` used the same dark/blue direction as its token palette but mixed many one-off grays and inconsistent radii/padding into selectors. This made input, scroll, disabled, tooltip, and panel surfaces look unrelated.
- `src/ui/runtime.cpp::UiRuntime::measure()` returned zero for several empty controls when no explicit `.xui`/`.xss` size was supplied. `TextArea` also did not share the text-field measurement branch. Containers were correctly deriving size from children, but could consequently collapse when their children had zero intrinsic sizes.
- The showcase had not exercised the supported `Toolbar`, `StatusBar`, or `Divider` aliases. Schema-only names (`Grid`, `Menu`, `Modal`, `ListView`, and others) are explicitly unsupported by `UiRuntime::build()`; authoring them into the showcase would make the document fail at runtime.
- A fresh file reference scan found all three source asset pairs still in use by docs and/or tests. The runtime executable itself loads only `showcase.*`; the redundant CMake copies of `cross_platform.*` and `controls.*` were subsequently removed (see the packaging cleanup entry above). The source fixtures remain because they are referenced.

### Changes and decisions

- Added preferred content-box sizes for empty/blank controls: Button 96×36, CheckBox 20×20, CheckItem 160×28, TextBox 160×28, TextArea 240×96, Slider 160×24, ProgressBar 160×16, ColorPicker/Dropdown 156×34, Switch 44×24, and FPS counter 168×26 logical pixels. Search/Number fields start at 180 px wide; labeled check controls and switches include room for their indicator/track, and longer dropdown labels grow beyond their preferred size. Text fields measure current text or placeholder and retain useful minima; content edits invalidate cached measurements and the regression case verifies a long field grows. `LayoutTree` adds declared padding/borders and still applies explicit dimensions and min/max clamps.
- This is a general runtime sizing fix rather than fixed dimensions in the demo. Container measurement remains child-driven; it now receives a usable preferred size from empty controls. Explicit small dimensions remain intentional constraints and can clip content.
- Centralized showcase surface/border/pressed/placeholder/readout/disabled/scrollbar colors as `.xss` tokens and normalized control radii and padding. Kept palette literals only in the token definitions so the stylesheet remains an idiomatic reference.
- Added `Divider`, `Toolbar`, and `StatusBar` examples in the Layout tab. These are supported aliases (Divider→Separator; Toolbar/StatusBar→Row) and remain styled declaratively.
- Added docs describing intrinsic default dimensions, the aliases, and why parser-recognized but runtime-unsupported objects are excluded. Kept `controls.*` and `cross_platform.*` because tests/docs/build copy steps still reference them.
- Trade-off: preferred sizes are widget defaults in logical pixels, not theme variables. They provide content-appropriate starting sizes, while apps can override them declaratively; this avoids forcing one theme's preferred geometry into the backend or stylesheet system.

### Verification and current status

- Release build succeeded for `xui_tests`, `xui_glfw_demo`, and `xui_hello`. The initial Debug build was blocked by MSVC C1902 (PDB manager mismatch); reconfiguring the build tree for Release with embedded debug information avoided that toolchain issue.
- Automated tests: **158 passed, 0 failed**, including the new auto-sized blank-control test and the existing complete showcase parse/build test.
- Latest hidden `--profile` runs after the composite-control sizing changes exited normally: `xui_hello.exe` **572.588 FPS** (4,137 frames / 7.22509 s); `xui_glfw_demo.exe` **583.659 FPS** (4,216 frames / 7.2234 s). These exceed 60 FPS for the measured hidden loop, but do not establish visible presentation pacing or resizing/interaction FPS. Earlier profile figures were superseded by these runs.
- Visible verification remained unavailable: the CUA session had no Windows app-launch/window-list methods, and the approved shell launch did not leave a discoverable running window for inspection. Therefore theme appearance, overlap/clipping, interactive resize, and normal-close behavior remain visually unverified. No widget can be called visually verified from the hidden profile.

### Next steps

1. Run the GLFW/OpenGL showcase visibly and inspect every tab, including the added aliases and the scroll clipping states; check typing, hover, popup interactions, and normal close.
2. Perform a sustained interactive resize with FPS visible. Capture any remaining overlap as an `.xui` fixture before changing the layout solver.
3. Keep parser-only widgets listed as unsupported until runtime behavior, styling, values, and backend rendering are implemented.

## Live resize, constrained percentages, picker and text input polish — 2026-10-15

### Audit and diagnosis

- `GlfwPlatform::pumpEvents()` polled GLFW each time the Xui event-drain loop asked for an event. During continuous window-resize callback production, the drain could keep polling/dispatching and postpone the frame submission point. The runtime already handles resize events by updating logical dimensions and laying out before drawing; the starvation was in the example platform's poll/drain boundary.
- Layout already had percentage `width`/`height`, and flex items already clamped pixel min/max values, but `.xui minWidth/maxWidth/minHeight/maxHeight` did not reach the solver. The `.xss min-size/max-size` path also discarded the unit, which prevented percentage constraints. Parent padding was included in child usable size; border insets were not consistently part of the content box.
- ColorPicker geometry had been expanded and paint/editor rects shared a new geometry helper, but hit testing retained stale fixed row offsets. While converting the hit test, an `else-if` chain also meant the default-on text editor section swallowed every non-format popup click, so SV/hue/alpha surfaces were unreachable.
- TextArea inserted configured indentation on Tab but literal tab shaping used the font's control-glyph advance. Fonts can assign a nonzero advance to that glyph. Caret positioning also needed a stable per-line advance for a trailing empty line/whitespace.

### Changes and decisions

- GLFW now polls once per `pumpEvents()` drain cycle, drains all queued callbacks, returns `false`, and gives the Xui frame loop a chance to lay out/draw before polling again. This is a platform-interface implementation detail, not layout logic in the demo. It favors a rendered frame between continuous callback batches; callbacks remain queued and are handled in order.
- Added CSS-like `minWidth`, `maxWidth`, `minHeight`, `maxHeight` and size shorthand handling through `.xui`; added `.xss min-width`, `max-width`, `min-height`, `max-height` while preserving percentage units in `min-size`/`max-size`. Percentages resolve against the parent content box after padding and borders, and min/max clamp the resolved border-box dimension. Grid, stack, flex, absolute children, and split panes now honor supported constraints; split max caps leave remaining free space available rather than violating the authored cap. Existing low-level float min/max fields remain compatible.
- Added border insets to layout's content-box calculation so child percentages and positions use the content area. Text layout now records per-line advances, treats literal tab as four actual space advances without drawing the control glyph, and TextArea inserts `tabSize` spaces on Tab (default 4, range 1–16); Tab on other controls remains focus traversal. Control characters received via text input are filtered. Caret/selection x uses the active line's shaped width so newline/trailing whitespace doesn't reset it.
- ColorPicker uses one popup geometry model for paint, editor placement, and pointer hit testing. Defaults are 300×352 logical pixels (safe minimum 260×340), with a larger SV field, hue/alpha strips, format/preview row, aligned labels, and editable fields. XSS still styles surfaces and typography; internal hue/SV geometry remains a widget default.
- Reworked `examples/showcase.xss` with a tokenized dark palette, visual hierarchy, consistent panel and field padding, row gaps/margins, and hover/focus/pressed/disabled states. The `.xui` comment and notes control now accurately demonstrate four-space Tab insertion.
- Updated `doc/xui-guide.md`, `doc/xss-guide.md`, `doc/styling-reference.md`, `doc/widgets.md`, and `doc/performance.md`; `plan.md` records the phase, actual test count, and current measured profiles.

### Trade-offs

- CSS-like min/max is expressed on the existing runtime length type instead of adding a new constraint DSL. Percentage constraints in an auto-sized/unbounded parent cannot resolve and follow the existing percentage-as-auto behavior. When split minimums exceed available space, the pre-existing policy proportionally shrinks panes to avoid negative geometry; strict minima cannot all be honored in an impossible space.
- ColorPicker uses more SV color tiles (48×22) for a smoother default surface. They are emitted as regular painter primitives and rely on existing painter batching. This session did not profile an open popup under continuous visible redraw, so popup-heavy cost remains to be measured.
- TextArea Tab now indents with spaces, while Tab elsewhere still traverses focus. This gives code/notes entry a predictable indent without changing form navigation.
- The GLFW drain change does not claim Win32 behavior. The current `xui_hello` and `xui_glfw_demo` targets both use the GLFW/OpenGL example backend in this checkout.

### Verification and current status

- MSVC Release build succeeded for `xui_tests`, `xui_glfw_demo`, `xui_hello`, and `bench_layout`.
- Automated suite: **156 tests, 0 failures**. New checks cover percentage min/max after border/padding, split max clamping, TextArea tab sizing/control filtering, tab advances, and per-line text widths; existing picker and full showcase interactions pass.
- Latest hidden uncapped `--profile` loops after the final Release build: `xui_hello.exe` 4,184 frames / 7.15079 s = **585.11 FPS**; `xui_glfw_demo.exe` 4,197 frames / 7.14915 s = **587.063 FPS**. Both exceed the 60 FPS target in that hidden loop. They do not measure actual screen presentation, resizing, caret blink visibility, or picker-open redraw cost.
- `bench_layout` after the constraint/split changes (938 nodes): cold build 0.32 ms, first layout 0.124 ms, full relayout 111.9 μs, resize 102.5 μs, one leaf content change 2.3 μs, split-drag frame 97.2 μs, idle 0 μs; the 1,000-child scroll lookup microbenchmark remains 0.008 μs/iteration.
- No visible GUI session was run in this verification. Therefore continuous visible resize responsiveness, picker proportions/legibility, caret blink placement, and text whitespace appearance remain visually unverified. There is no separate Win32 app/backend target in the current build for parity testing.

### Next steps

1. Run a visible GLFW demo smoke pass: drag-resize continuously, exercise tabs/text fields/picker, verify caret blink and popup legibility, close normally, and record FPS while resizing and with the picker open.
2. If visible resize still stalls, capture per-frame layout/paint/swap timings while resizing and inspect platform callback frequency rather than inferring from the hidden loop.
3. Add or restore a distinct Win32 platform target before claiming backend parity; this change currently covers the GLFW event pump only.

## Test fixes: caret-blink timer lifetime + showcase 4-tab drift — 2026-10-15

### Root causes (2 failed tests out of 153)

- `ui_runtime_text_fields_check_box_tabs_and_delayed_tooltips` — `CHECK_EQ(ui.nextTimerDelay(), -1.0)` at lines 570 and 589. The caret-blink feature arms a timer whenever an editable field is focused, but the deadline was only disarmed during `paint()`, which these headless tests never call. Line 570 fails legitimately (the field is focused and just typed — a blink tick *is* pending); line 589 fails through a stale deadline after focus moved to a checkbox. The reported "slider" failures were a misattribution: those line numbers sit inside the text_fields test; the slider test itself passes.
- `showcase_example_assets_parse_validate_and_build` — the test encoded the old 3-tab showcase: header clicks at `width*0.5`/`width*0.84` and a 3-iteration paint loop. The demo now has four tabs, so the header clicks landed on the wrong tabs (0.5×4 → tab 2, 0.84×4 → tab 3) and every subsequent interaction check failed in cascade (lines 730–763).

### Fixes

- `UiRuntime`: new `caretEditableFocused()` helper; `nextTimerDelay()` and `updateTimers()` now treat the blink as pending only while an editable field is actually focused, and `updateTimers()` clears a stale deadline when focus has moved away. `paint()` reuses the helper. Contract: a focused editable field wakes the idle app at most ~2×/second for the blink; once no editable field is focused, `nextTimerDelay()` returns −1 again.
- `ProgressBar` gained `value()`/`setValue()` (`f64`, clamped to min/max) — it held state but was the only control not exposing it, and the new showcase checks read it by id.
- `tests/test_runtime.cpp`: the line-570 assertion is now blink-aware (`nextTimerDelay() >= 0.0` with a comment); the showcase test iterates four tabs, clicks header centers at `0.375`/`0.625` (equal-width fourths), and checks the Widgets tab: value lookups for all Phase 1 + second-wave ids (`query`, `delay`, `whitespace-toggle`, `live-sync`, `mute`, `density-choice`, `shading`, `priority`, `subdivisions`, `due-date`, `save-key`, `trail`, `busy-bar`) and non-empty rects for the Widgets-tab controls in the paint loop.

### Verification and current status

- Not yet rebuilt in this session (owner runs builds). Expected: suite green at 153 tests. The blink-lifetime change is test-covered by the existing assertions (570 expects a pending blink, 589 expects none after focus moves).
- The misattributed slider test was left untouched; it passes.

## Second widget wave + Blender grey theme — 2026-10-15

### Added

- Six new runtime widgets, each with `.xui` properties, `.xss` states, pointer + keyboard interaction, focus registration, and value-API exposure: `SegmentedControl` (options/value, click + Left/Right, selected cell uses `fg`), `Stepper` (value/min/max/step/precision/unit, −/+ buttons, Up/Down, f64 value), `Rating` (max pips, click/Left/Right, click-last-pip clears), `Breadcrumb` (per-label-width clickable path), `KeybindField` (captures mods+key, normalized labels; Tab/F3 reserved; Escape clears), and `DatePicker` (`YYYY-MM-DD` value, month-grid popup reusing the dropdown/picker overlay pattern, ‹ › month nav, day selection, arrow-key day stepping via proleptic-Gregorian date math).
- `Toolbar`/`StatusBar` materialize as Row-equivalent containers (schema names existed; no state).
- `ProgressBar[indeterminate: true]` sweeps via the shared 80 ms idle tick (paint and both timer loops extended; no continuous redraw).
- `NumberField` finally paints its −/+ stepper buttons and routes clicks in the reserved 44 px zone (the Phase 1 reserve existed without buttons or hit zones).
- Demo: new "Widgets" tab exercising all of the above with per-control live readouts via `value()`/`setValue()`; six new handlers in `glfw_demo.cpp`; `showcase.xss` fully restyled to a flat grey Blender-inspired theme (canvas `#1d1d1d`, panels `#303030`, widget surfaces `#545454`, dark `#212121` outlines, Blender blue `#4772b3` selection accent, 3 px radii, no gradients).

### Decisions and trade-offs

- Keybind capture excludes Tab (focus traversal) and F3 (FPS toggle) so global shortcuts stay intact; documented in `doc/widgets.md`.
- DatePicker has no "today" marker (no wall clock in the runtime) and no hover highlight inside the popup; day selection commits and closes like ImGui-style pickers. PageUp/PageDown shift ±30 days rather than exact calendar months (documented).
- Calendar/dates use civil-date math valid across the full proleptic Gregorian range; no locale — month/weekday names are English-only.
- `SegmentedControl` selected fill and `Rating` pip colors derive from `fg`; there are no per-segment/per-pip sub-selectors yet (same virtual-target limitation as dropdown rows).
- Stepper defaults 0–100 step 1; authored min/max override. No typed entry — that stays `NumberField`'s role.

### Verification and current status

- **Not compiled or run in this session** (owner directive: no builds/executions). This is a large unverified delta on top of the also-uncompiled caret/whitespace change; the very first next action must be a full build. Expected risk areas: the new paint blocks, the popup-loop condition changes (datePicker must not fall into the color-picker `else`), and `keyLabel`'s F-key arithmetic.
- Prior baseline: Phase 1 runtime compiled clean under MSVC `/W4 /WX`; suite was green (152 tests) before the demo/theme changes; none of the new widgets have tests yet.

### Next steps

1. `cmake --build build --config Release --target xui_tests xui_glfw_demo`, fix compile errors, run the suite.
2. Visible smoke pass: every Widgets-tab control, date popup nav, keybind capture (incl. reserved keys), indeterminate sweep + spinner under F3, resize smoothness in the grey theme.
3. Add unit tests: segmented/rating/stepper value math, breadcrumb hit widths, keybind label normalization, date parse/shift round-trips, indeterminate timer scheduling.
4. `--profile` FPS check with the busy bar + spinner visible.

## Caret blink, whitespace visualization, demo polish — 2026-10-14

### Audit and diagnosis

- The caret was drawn every frame while an editable field was focused: no blink existed, and the timer system had no caret state. Text input itself already handled spaces (printable codepoints) and TextArea newlines correctly; Tab already moved focus and never inserted into fields — but none of this was documented, and the demo offered no way to see whitespace/control characters.
- Caret-prefix measurement used the raw stored text while display could differ (password bullets); the new whitespace substitution would have widened that mismatch, so prefix measurement had to share the substitution.

### Changes and decisions

- **Blinking caret**: `UiRuntime` now blinks the caret of focused, enabled, writable `TextBox`/`TextArea` fields on a 530 ms tick driven by the existing idle timer (`updateTimers`/`nextTimerDelay`), so an idle app with a focused field wakes at most ~2x/second and no other frame is touched. Blink resets to solid on typing, caret movement, deletion, paste, and focus gain (`resetCaretBlink()`); it is suppressed for read-only/disabled fields and stopped entirely when no editable field is focused. Picker popup editors blink through the same mechanism. Only the focused fields' rectangles get damage per blink.
- **Whitespace visualization**: new `setWhitespaceVisible(bool)` debug API renders spaces as a middle dot (·, U+00B7) and tabs as a right arrow (→, U+2192) inside editable fields. Substitution is one-codepoint-to-one-codepoint and caret/selection prefixes are measured through the same `visualizeWhitespace()` mapping, so caret and selection stay aligned with the drawn glyphs. Password fields are excluded. The showcase demo exposes it as an "Show whitespace marks" checkbox.
- **Tab decision (documented)**: Tab always moves keyboard focus; it never inserts a tab character. Textual tabs can still be pasted. This avoids conflicting keyboard semantics and matches the existing focus system; `doc/widgets.md` now states it.
- **Demo polish**: added the whitespace-mark toggle to the Input tab with a tooltip. The existing stylesheet already provided a token-driven hierarchy with consistent radii/pads and full hover/focus/pressed/checked/readonly/disabled coverage; a larger visual redesign was deliberately not attempted in the same change as two runtime behavior features (verification is serialized, and the visual baseline is already cohesive).

### Trade-offs and limitations

- Password-field caret position is measured against the raw text while the display shows bullets; this mismatch predates this change (bullets are one codepoint per source codepoint only for ASCII) and is still open.
- Blink uses the global idle timer rather than per-field timing; with a focused field the process wakes twice per second in an otherwise idle state — same trade-off as the FPS counter, kept for consistency.
- The whitespace substitution covers space and tab; newlines render as real line breaks in `TextArea` and cannot occur in `TextBox`.

### Verification and current status

- **Not compiled.** The caret/whitespace runtime changes and the demo additions were made without build or test execution in this session (owner directive). The previously delivered Phase 1 runtime compiled clean under MSVC `/W4 /WX` before these edits; everything since — including these edits and the earlier showcase changes — awaits one compile + suite run + visible smoke pass.
- Automated coverage is missing for blink timing and whitespace substitution; both are runtime-observable behaviors that deserve unit tests in the next suite run.

### Next steps

1. Compile `xui_tests` + `xui_glfw_demo`; fix any compile errors (highest-risk edits: the caret-prefix lambda and blink block in `paint()`).
2. Run the suite, then the demo: verify caret blinks when focused and goes solid while typing, whitespace marks toggle renders and keeps caret alignment, Tab moves focus between all controls, and no text overlap or freezes during resize.
3. Add tests: caret blink scheduling (`updateTimers`/`nextTimerDelay` around a focused field), `visualizeWhitespace` round-trip with caret prefixes, and Tab focus behavior.

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
- Added the GLFW implementation of the framework's `Platform` boundary (now `backends/glfw/glfw_platform.*`) and the OpenGL texture presentation renderer (now `backends/opengl/gl_drawlist_renderer.*`). Both the Win32 and GLFW hosts then loaded the same `cross_platform.xui` / `.xss`, which demonstrate a checklist, interactive button, styled panels and a draggable split. The UI app code consumes Xui APIs; GLFW calls are contained in the backend implementation.
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
- Added the focused `controls.xui` / `.xss` sample pair (now stored under `tests/fixtures/` and linked from the user guides), copied them beside both GLFW demo targets at that time, and added the `--controls` command-line option. Updated the example event callback to demonstrate reading state by authored id.
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

## ScrollView runtime integration and layout audit — 2026-10-15

### Audit and findings

- Read the newly supplied request and re-audited the changed checkout before editing. Git showed a clean committed working tree at the start, so the existing user changes were preserved.
- The layout engine already had dirty-subtree invalidation, constraint-keyed measure caching, content-size tracking, clamped scroll offsets, and an unbounded scroll main-axis. The missing feature was integration: the core schema listed `ScrollView`, but `UiRuntime::buildNode` did not materialize it; the shared event loop ignored `EventType::Wheel`; and `GlfwPlatform` did not register a scroll callback.
- Measured the pre-integration Release layout baseline with the 938-node benchmark: cold build 0.31 ms, first layout 0.118 ms, full relayout 107.9 μs, resize 102.3 μs, one leaf change 2.2 μs, and same-size idle layout effectively 0 μs. These timings indicate that replacing the existing solver would add risk without evidence of a bottleneck. I retained its cache/invalidation approach and focused code changes on wiring the feature and avoiding layout work during scrolling.

### Changes made and decisions

- Materialized `.xui ScrollView` as `LayoutKind::Scroll` in the shared `UiRuntime`. `vertical` defaults true and `horizontal` false; both can be enabled. Added `.xui` behavior controls `scrollSpeed`, `scrollBehavior`, and `scrollbar` (`auto`, `always`, `never`).
- Added `Wheel` dispatch in `Xui::run()` and registered GLFW's platform scroll callback, which emits normalized wheel deltas plus logical pointer coordinates. No native API was added to shared runtime code.
- Added per-descendant clip scopes from scroll ancestors for paint and hit tests. Scroll offsets use the existing `LayoutTree::setScrollOffset()` result path; they do not invalidate or rerun layout.
- Added track/thumb painting and hit/drag behavior using `.xss` `scroll-track`, `scroll-thumb`, `scrollbar-size`, `scrollbar-visibility`, `scroll-behavior`, and `scroll-speed`. Standard hover/pressed state blocks control thumb colors; `radius` rounds tracks/thumbs. Added a separate shared overlay pass so descendant content cannot paint over a scrollbar.
- Added deepest-first wheel routing with ancestor fallback, arrow/PageUp/PageDown/Home/End keyboard support for focused scroll containers (while leaving text and existing arrow-key controls in control), Tab-focus scroll-into-view, and smooth easing driven by the existing idle timer mechanism. Momentum/touch handling was left out because the request made it optional and the project has no gesture abstraction.
- Added runtime APIs `scrollOffset(id)`, `scrollTo(id, Vec2, smooth)`, `scrollIntoView(id, smooth)`, and `value(id)` returning `Vec2` for a ScrollView ID. `change` callbacks allow applications to read and display the live offset.
- Expanded the GLFW showcase with vertical, horizontal, nested, smooth, always-visible, and hidden scrollbar cases; a programmatic jump button; and a live offset readout. Updated widget, XUI, XSS, architecture, performance, README, plan, and docs index pages.
- Existing behavior deliberately left intact: the retained solver's measure cache and relayout boundaries, draw-list renderer interface, glyph handling, existing control routing, and authored `.xui`/`.xss` split.

### Trade-offs and limitations

- Per-node clip scopes repeat ancestor checks while painting/hit-testing. This keeps ordering correct in the flat retained paint traversal and does not add per-scroll layout recomputation; a persistent clip stack would need a broader renderer traversal refactor.
- `auto` scrollbars are shown while their view is hovered/focused; always/never visibility is explicitly controllable. Scrollbars are custom overlay geometry styled by XSS, not native controls.
- Smooth motion uses a short exponential ease. No inertial momentum, touch input, overscroll chaining policy, scrollbar corner styling, accessibility tree, generic `overflow` clipping, or list virtualization is included.
- The layout solver itself was not modified because its measured baseline was already inexpensive. This request therefore adds ScrollView capability and documentation of measured layout efficiency rather than claiming an unmeasured solver speedup.

### Verification and status

- Added automated coverage for `.xui`/`.xss` parse and schema validation, vertical wheel movement, nested view consumption and parent fallback, keyboard scrolling, programmatic scroll-to/scroll-into-view, clamping, and scrollbar dragging. The showcase asset test also parses/builds the new fifth tab.
- Windows Release compilation succeeded for `xui_tests` and `xui_glfw_demo` after the shared runtime and GLFW adapter edits. At this point the test suite has 154 tests; final rerun/profile results are recorded below once complete.
- Hidden profile mode measures loop throughput only; no visible desktop interaction/resize pass has been run in this turn. Linux/macOS and separate native Win32 remain unverified. Do not treat a high hidden FPS as proof of visible interaction latency.

### Next steps

- Complete the final test rerun, benchmark comparison, and bounded GLFW hidden profile; append measured results here.
- Perform a visible smoke pass for nested wheel handoff, thumb drag, keyboard focus visibility, text glyphs/clipping, window resize, and normal close.
- Next plan items remain Grid and titled/collapsible containers. For virtualized data widgets, first extend ScrollView's scroll-into-view and measurement contracts before adding ListView/Table.

### Final verification addendum — 2026-10-15

- Rebuilt Windows Release targets `xui_tests`, `xui_glfw_demo`, and `bench_layout` successfully with MSVC warnings treated as errors.
- Final automated suite: **154 tests, 0 failures**. New scroll test covers wheel movement, change callbacks, Vec2 `value()` retrieval, silent-by-default `setValue` plus `notify=true`, keyboard scrolling, nested deepest-first consumption and ancestor fallback, scroll-into-view, thumb drag, and range clamping. The existing showcase asset test parses/builds the new tab and `.xss` properties.
- Final Windows Release hidden GLFW profile: **4,025 frames / 5.00004 s / 804.994 FPS**. This clears the numeric 60 FPS target in the uncapped hidden loop; it is not a visible scrolling/resize FPS measurement.
- Final `bench_layout` run: 938-node full relayout **103.8 μs**, resize **99.8 μs**, single-leaf change **2.4 μs**, and idle layout **0.000 μs**. Baseline was 107.9 μs, 102.3 μs, 2.2 μs, and 0.000 μs. These are within normal run variance; no layout-solver speedup is claimed. New 1,000-child scroll-offset + descendant `absRect()` benchmark: **0.008 μs** median per operation, **0 nodes arranged**.
- The final benchmark preserves its descendant-rect checksum to prevent the compiler from removing the measured work. The generated demo assets were copied by the Release build and consumed by the successful hidden profile.
- Remaining acceptance gap: no visible GUI session was used, so readable on-screen text, scrollbar appearance, smooth wheel feel, responsive resize, and normal close have not been visually verified. Linux/macOS and a separate native Win32 backend also remain unverified.
- Current status: ScrollView runtime, style vocabulary, GLFW wheel delivery, showcase and documentation are implemented and headlessly verified. Performance acceptance is partial because only hidden loop throughput and deterministic synthetic layout measurements are available. Next: do a visible GLFW smoke pass; then continue Phase 2 with Grid and titled/collapsible containers.

## Layout, theme, resize, and visibility audit — 2026-10-15

### Audit and root causes

- Re-read the current layout solver, runtime paint/hit-test traversal, `Xui::run()`, GLFW event drain, showcase `.xui`/`.xss`, docs, and the screenshot supplied with the request. The screenshot is the Scrolling tab: horizontal and nested `ScrollView` contents are clipped to their own viewports, and hidden tab pages are intentionally stacked. Those visual clips are expected; they are not flow siblings painting over one another.
- No shared-flow overlap defect was reproduced in the source or regression tests. `Row`/`Column` place flow siblings sequentially; `Stack`/`Tabs`, explicit grid coordinates, and `Position::Absolute` intentionally allow overlapping rectangles. Min/max can intentionally force overflow when constraints conflict. Existing layout tests cover percentage sizing against the parent content box, min/max clamp combinations, nested resize, cache invalidation, and differential fuzzing. No layout-solver behavior was changed without a failing case.
- Resize was not deferred to mouse-up: the GLFW size callback emits `Resize`, `Xui::run()` reads the latest logical client and physical framebuffer sizes each iteration, resizes the renderer, and calls runtime layout before the next paint. The prior `pumpEvents()` once-per-drain guard is present. The specific resize freeze is still not closed by an interactive reproduction in this turn; the desktop automation window lost its process handle when a border-drag test was attempted.
- The screenshot's `FPS: 4 | 251.6 ms` came from event-driven idle redraws: the FPS widget refreshed itself every 250 ms and counted the wait between frames as render time. This was a measurement-mode bug, not evidence that one draw took 251 ms. Turning the overlay on now selects continuous redraw so its FPS value measures frame cadence. A visible GLFW screenshot during this turn showed `FPS: 97 | 5.1 ms`.
- Theme resolution was in fact pinned to palette index zero at every runtime style resolve site, including tabs, tooltips, and generated picker editors. Composite draw code also had a handful of state/highlight colors that bypassed foreground/radius/border style values.

### Changes and decisions

- Added `UiRuntime::setTheme(name)` and `Xui::setTheme(name)`. The facade accepts a theme after `loadXss()` and before or after `attach()`; it validates the name and preserves a pre-attach choice. Runtime resolves all existing nodes against that palette, refreshes inherited text styles/font selection, invalidates layout measurements, and redraws. The palette still uses compiled style sets; no recompile or selector rematch occurs.
- Routed every runtime `.xss` resolve site through the selected theme index, including dynamically-created picker editors and tooltip bubbles.
- Changed editable selection, checkbox borders/radius, switch off track, radio/segmented/rating hover outlines, and missing-image placeholders to derive appearance from resolved `.xss` foreground/background/border/radius values instead of isolated fixed colors. Kept only high-contrast marks and semantic color-picker ramps fixed by design.
- Updated theme and widget styling references with a subpart limitations table. Theme changes currently re-resolve paint/text but do not reapply theme-dependent layout values because the current merge path must retain explicit `.xui` precedence; geometry tokens should remain invariant across themes.
- Enabled continuous frames while the FPS overlay is active. Trade-off: accurate sampling and smooth visible animation use more CPU/GPU until F3 disables the overlay. Normal static applications still sleep in event-driven mode.
- No layout-solver rewrite was made: source inspection and automated tests did not show an ordinary flow-overlap cause. Altering its well-tested min/max and content-box rules without a reduced failing `.xui` case would add regression risk.

### Verification and remaining gaps

- Rebuilt `xui_tests`, `xui_glfw_demo`, and `xui_hello` from the working tree with MSVC Debug and `/W4 /WX` after the final code changes.
- Automated suite: **157 tests, 0 failures**, including new runtime theme activation/draw color coverage and existing layout percentage/min/max, root resize, nested scroll clipping, interaction, text wrapping, popup, and widget tests.
- Hidden profile runs after theme wiring but before the final widget-subpart tint changes reported `xui_hello` **4,026 frames / 5.00028 s / 805.155 FPS** and `xui_glfw_demo` **4,035 frames / 5.00035 s / 806.944 FPS**. After the final tint changes, both profile processes exited successfully but the shell capture returned no FPS summary, so those latest runs are not counted as measured data.
- A visible GLFW/OpenGL frame was captured with all five tabs, text, horizontal/vertical/nested scrolling, and the FPS overlay visible at **97 FPS / 5.1 ms**. No white-box glyphs or sibling overlap appeared in that captured state. Content at viewport edges was correctly clipped.
- Manual click/typing/scroll/resize/close verification was not completed: the computer-use bridge returned `foreground window did not report a process id` during the resize attempt and then no longer listed the demo process. A visible `xui_hello` run was also not completed. Therefore live resize freeze-free behavior and interaction latency are not claimed as verified, despite the correct source path and passing headless interaction tests.
- Layout-dependent themed values remain partial. Theme-specific padding/size/gap are not re-applied. The exact user-reported flow-overlap case remains unreproduced, so the current layout engine has not been changed to mask overflow or clip all containers.

### Next steps

- Reopen the rebuilt GLFW demo in an interactive desktop session and continuously resize while checking the live FPS display; then test click, hover, text input, nested scroll, and normal close.
- Run the same visible smoke test for `xui_hello.exe` and confirm text, resize, and close behavior.
- Add a focused `.xui` regression fixture if any non-intentional sibling overlap remains; capture node bounds, parent content boxes, and z-order before changing the solver.
- Rework theme application to reapply layout-affecting stylesheet values while preserving authored `.xui` overrides, then verify all geometry/state combinations.

## Root README refresh — 2026-10-16

### Changes and decisions

- Replaced the root README's duplicate table of contents and repeated setup/usage sections with a short project description and links to the canonical `doc/` and `plan/` indexes.
- Added a compact `xui::Xui` startup example using the current GLFW/OpenGL backend class names, with matching minimal `.xui` and `.xss` files. The sample expects the application's TrueType font at `font.ttf`; font files are not bundled in this repository.
- Added the existing `doc/showcase.png` with a caption describing only visible showcase content, and linked its `.xui`, `.xss`, C++ entry point, and widget documentation.
- Kept the fuller setup and backend build instructions in `doc/getting-started.md` rather than duplicating them in the root entry point.

### Verification and status

- Inspected `include/xui/xui.hpp`, the GLFW demo and backend declarations, getting-started/architecture guides, and the screenshot. The methods, options, backend class names, and referenced files in the README match the current source tree.
- Confirmed `doc/showcase.png` exists and visually shows the tabbed GLFW/OpenGL showcase, themed controls, a checklist, splitter panels, and the FPS/readout area.
- A fresh compile could not be run in this environment because `cmake`, `cl`, and `msbuild` are not available on `PATH`. Existing prebuilt demo binaries are present, but they do not constitute a fresh verification of this README snippet. No framework code or build files were changed.
- Current status: README content and internal links were source-checked; compile verification of the exact inline sample remains unconfirmed. The user can follow the linked getting-started build instructions to compile the bundled GLFW example.

## DockHost implementation slice — 2026-10-16

### Changes and decisions

- Added the recursive `DockTree` model and runtime `DockHost`/`DockSpace` plus `DockPanel` support. A host accepts direct panel children; panels can form nested edge splits, center tab groups, close, and float as overlays inside the host window. Splitter drag updates ratios live, panel layout honors explicit min/max and percent constraints, and Escape cancels an in-progress panel drag.
- Added `.xui` `region`, `target`, `floating`, `closable`, `icon`, `preferredSize`, `gutter`, and `headerHeight` handling; common `minSize`/`maxSize` rules still apply. Added `DockPanelValue` retrieval/set and `dock`/`undock`/`resize`/`tabChange`/`close` handler routing.
- Added XSS-resolved subparts `DockHeader`, `DockTab`, `DockCloseButton`, `DockSplitter`, and `DockPreview`, with hover/selected/pressed/dragging states. Added a Docking tab to `examples/showcase.xui` and matching theme rules in `examples/showcase.xss`; the GLFW app reflects dock state in a readout.
- Added `doc/docking.md`, linked it from the docs index and widget/layout/architecture guides, and updated the runtime support list.
- During integration, overlay hit-testing and paint initially considered dock hosts inside inactive showcase tabs. This stole pointer input from other pages and could draw hidden dock chrome. The dock paint, hit-test, cursor, and pointer-start passes now skip hosts whose tab ancestry is inactive.
- Floating is in-app as requested. Recursive splits use a dedicated tree rather than ordinary `.xui` Split nodes. Initial placement uses `region` because `dock` names the runtime event. The model's move/split-drag paths do not allocate; dock topology changes may grow storage at drop time. Closed panels currently cannot be reopened through the value API.

### Verification and status

- Configured a fresh Visual Studio Ninja Release build; rebuilt `xui_tests` and `xui_glfw_demo` with MSVC `/W4 /WX`.
- Automated suite: **161 tests passed**, including a new runtime case for nested/center placement, panel minimums, splitter resize, release-outside floating, and value-API redocking. Existing showcase asset and interaction checks pass with the seventh tab.
- Final hidden GLFW profile from the freshly linked verification binary: **3,345 frames / 5.00007 s / 668.991 FPS**. The hidden `xui_hello` profile measured **2,886 frames / 5.00116 s / 577.066 FPS**. Both exceed 60 FPS in the hidden loop, but neither measures visible frame cadence, resize, or user interaction.
- Added and ran the Release `bench_docking` harness (four authored panels, 200,000 operations, MSVC Release): latest drag-preview pointer updates measured **0.77 μs/move** and splitter resize updates **2.00 μs/move**. An earlier run measured 0.69/1.87 μs; these small differences show normal host-load variance. The harness exercises the `UiRuntime` path headlessly; it excludes painter work, OpenGL submission, event pumping, vsync, and display presentation, so these numbers are not visible FPS or end-to-end latency.
- Launched the rebuilt GLFW demo in a visible window. This environment has no native-window inspection/input control, so I could not verify the visible appearance or manually exercise dock tabs, previews, close, Escape, and splitters. A normal close request was not acknowledged by the process, so the visible window/close behavior remains unverified. The final binary was freshly linked to an alternate verification output directory while that original process held its executable open.
- No interactive docking screenshot or visible resize/drag FPS has been recorded. The visible acceptance list remains: tab/header paint, cursor, close, Escape cancel, live preview, nested edge drops, float movement, ordinary resize, and >60 FPS during docking.
- Current status: source, sample assets, and user docs are implemented and headlessly verified. Docking remains partially verified pending a visible desktop pass.
- Next steps: run the rebuilt demo visibly; test each drop zone and cancel path; capture visible FPS during tab and splitter drags; test close and re-dock another panel; verify the other six tabs remain interactive.

### Docking drag-release regression fix — 2026-10-16

- Root cause: `dockPointerUp()` preferred the last valid in-host preview before checking where the release occurred. A fast pointer release outside the host, with no final move event, could therefore dock to a stale target instead of floating.
- Changed the release decision to use the pointer-up coordinates as authoritative: a preview is committed only when pointer-up is inside the host; an outside release floats the panel. This also covers event streams that omit a final pointer-move.
- Extended the runtime test to perform a real tab drag with an in-host preview followed directly by pointer-up outside the host. It verifies the panel becomes floating and can be redocked through `setValue`.
- Rebuilt `xui_tests`, `xui_glfw_demo`, `xui_hello`, and `bench_docking` in the Windows Release Ninja build using MSVC `/W4 /WX`. Automated suite: **161 tests, 0 failures**.
- Latest hidden profiles: GLFW demo **3,399 frames / 8.04815 s / 422.333 FPS**; hello **3,467 frames / 8.11852 s / 427.048 FPS**. These exceed 60 FPS in the hidden loop only; they do not measure visible rendering cadence or input latency.
- Latest four-panel headless docking benchmark: **0.67 μs/drag-preview move**, **1.88 μs/splitter-resize move** over 200,000 operations. The benchmark excludes paint, OpenGL submission, event pumping, vsync, and presentation.
- `git diff --check` passes. A visible GLFW window was started for a smoke check, but this environment did not provide native window input/inspection and its graceful-close request was not acknowledged. The process was absent at the final process check. Appearance, visible docking gestures, responsive resize, and normal close remain unverified.
- Current status: the outside-release edge case is fixed and covered headlessly. Docking remains partially verified pending an interactive desktop pass; no visible FPS or manual gesture claim is made.

## Repository backend and fixture layout — 2026-10-16

### Audit, changes, and decisions

- Audited tracked sources, docs, CMake files, and references. The only concrete backends under `examples/` were the GLFW `Platform` adapter and OpenGL `Renderer`; `glfw_demo.cpp` is the application and remains in `examples/`.
- Moved `glfw_platform.cpp/.hpp` to `backends/glfw/` and `gl_drawlist_renderer.cpp/.hpp` to `backends/opengl/`, as approved. Added `xui_backend_glfw` and `xui_backend_opengl` static targets in `backends/CMakeLists.txt`. The GLFW demo links both targets; core libraries do not link either adapter. Backend headers are exposed through their linked targets as `<glfw/...>` and `<opengl/...>`; the stable public `include/xui/` paths are unchanged.
- Moved `cross_platform.xui/.xss` and `controls.xui/.xss` from `examples/` to `tests/fixtures/`, as approved. Updated `XUI_TEST_FIXTURE_DIR`, runtime fixture loading, user guide links, and source-layout documentation. `showcase.xui/.xss` stay in `examples/` because the demo, runtime tests, README, and docs share them. Other small tests author markup inline in their owning C++ tests.
- Moved the `XUI_BUILD_GLFW_EXAMPLE` option to the root CMake configuration so the optional backend/example build is discoverable alongside other project options. Examples add the backend subdirectory only after GLFW and OpenGL targets are available. No install/export rules existed for the backend adapters; none were added, so the stable installed/public include contract remains unchanged.
- Searched the repository (excluding build outputs and vendored GLFW). All authored asset pairs have references: showcase is demo/tests/docs; cross-platform and controls are tests/docs. No unused `.xui` or `.xss` files were identified, so no assets were deleted. No `.xss` deletion was needed.
- Updated the root README, `doc/README.md`, user architecture/getting-started/widget/XUI guides, `plan/architecture.md`, and this tracking entry for the new paths. The root plan notes the location split and uses the latest profile numbers.

### Verification and status

- Incremental backend-stage configure and Release build succeeded for `xui_glfw_demo` and `xui_hello`; both backend static libraries compiled with MSVC `/W4 /WX`.
- Fixture-stage Release build and test run succeeded: **161 tests, 0 failures**.
- A clean Release configure/build was run in `build/layout-reorg-clean` from scratch: all **85/85** build steps completed, including `xui_tests`, `xui_glfw_demo`, `xui_hello`, benchmarks, and fuzz targets, with warnings treated as errors. CTest reports **1/1 tests passed**.
- Hidden profiles from the clean build: `xui_glfw_demo` **3,357 frames / 5.00014 s / 671.382 FPS**; `xui_hello` **3,460 frames / 7.99591 s / 432.721 FPS**. These exercise the hidden GLFW/OpenGL loop and clear 60 FPS numerically, but do not measure visible presentation pacing, interaction latency, or resize behavior.
- No user-facing files were deleted. Existing `cross_platform` and `controls` paths under `examples/` are now absent; the replacement fixtures are under `tests/fixtures/`. A repository search found no live source/CMake/docs links to the old locations. Historical tracking prose retains prior change chronology where applicable.
- `git diff --check` is clean apart from standard Git line-ending normalization warnings. Visible demo interaction and resize were not manually inspected in this structural-change pass; prior project records still identify that as an open acceptance gap.

### Remaining work

- Run a visible GLFW smoke pass for the demo after the path changes when native desktop inspection is available; test startup asset loading, interaction, docking, resize, and normal close.
- Keep the GLFW/OpenGL adapters optional and separate from core targets; add install/export support only if a later packaging task defines a stable backend distribution contract.

## GitHub Actions workflow alignment — 2026-10-16

### Audit and changes

- Audited `.github/workflows/ci.yml`; it is the repository's only workflow. It already runs on push and pull request across Ubuntu GCC 13, Ubuntu GCC 13 with ASan/UBSan, Ubuntu Clang 18/libc++, macOS 14, and Windows 2022. Linux installs X11/OpenGL development packages and disables GLFW Wayland because that workflow job is configured for X11.
- The CMake restructure introduced the explicit `XUI_BUILD_GLFW_EXAMPLE` option and separate `xui_backend_glfw` / `xui_backend_opengl` targets under `backends/`. CI now explicitly enables that example and builds `xui_tests` plus `xui_glfw_demo`, ensuring the relocated adapters are compiled rather than relying on an implicit default all-target build.
- Kept existing push/PR triggers and platform matrix. Added no secrets, caches, release/publish jobs, or extra platforms. CTest remains the test runner, with its existing direct-binary diagnostic fallback.
- No previous failing workflow run was available in the local checkout, so this update does not claim to fix a recorded hosted failure. The change addresses coverage drift after the backend/CMake restructure.

### Verification and status

- Windows local validation used a fresh Debug Ninja configure with the same CI options and MSVC `/W4 /WX`; the explicit `xui_tests` and `xui_glfw_demo` targets built successfully (67/67 build steps), including both backend libraries.
- CTest passed: **1/1 registered test passed** (`xui_tests`, 17.57 seconds; the executable reports the suite's cases internally).
- Source review confirmed the workflow references current target and option names; old `examples/` backend and fixture paths do not appear in workflow commands. No `actionlint`, YAML parser, or GitHub-hosted runner result is available locally; final hosted YAML parsing and the Ubuntu/macOS jobs remain unverified until Actions runs.
- No visible-window FPS or interaction measurement was part of CI validation. Hidden profile results elsewhere in this tracking file are not evidence of visible >60 FPS performance.

### Decisions and next steps

- Explicit targets make CI intent clearer and prevent an accidental target-selection change from skipping the demo or tests. Linking the GLFW demo compiles both backend adapters through CMake's target dependencies.
- Retained the current OS matrix because it is already the project's stated target coverage. Linux CI intentionally validates X11, not Wayland; add Wayland only after the required dependencies and support claim are established.
- Next: inspect the hosted matrix result for this workflow revision, fix any platform-specific build failures, then consider adding an independent YAML/actionlint check if the project adopts that tool.

## Row text tail clipping — 2026-10-18

### Audit and root cause

- Reproduced the authored tree in a runtime regression test: the original window/Column plus a `Row(gap: 10)[class: "bar"]` containing `Text("World! Extension")`, with adjacent cases for `!`, `World!`, a wrapped long line, and a button child.
- The Row solver was not subtracting an extra trailing gap: it uses exactly `n - 1` gaps. The failure came from text metrics exposed by the tight Row child box. Text measurement used the runtime's previous/default DPI (layout ran before `paint()` assigned the current DPI), while glyph rasterization used the current DPI. Text shaping also truncated fractional pixel sizes, while the atlas rounded them. A Row's intrinsic child width exposed the mismatch and clipped the glyph run; a Column stretched its cross-axis child and masked it.
- `TextLayout.width` also represented only pen advances, while the glyph atlas emits raster quads with glyph ink bounds and a one-pixel antialiasing border. This left no guaranteed room for glyph overhangs in content-sized text widgets.

### Changes and decisions

- Added a DPI-aware `UiRuntime::layout(width, height, dpiScale)` overload. The existing two-argument overload retains the latest DPI. `Xui::renderFrame()` now supplies the platform DPI before measuring; `paint()` re-lays out if a lower-level caller changes DPI only at paint time.
- Text layout now rounds to the same pixel size as the atlas. `TextLayout.width` includes rasterized horizontal glyph bounds; the new `advanceWidth` retains the pen measure used for caret, selection, and text-input scrolling. `inkLeft` lets paint align the visual bounds without clipping a negative glyph bearing.
- Runtime intrinsic measurement now shapes text at physical DPI, converts measured dimensions back to logical units, and uses the visual width for text and labeled controls. Glyph drawing uses the same rounded raster size. This fixes the metric contract centrally rather than adding Row-specific sizing or text hacks.
- Added text layout and runtime regressions. The runtime case checks `!`, `World!`, `World! Extension`, a constrained wrapping case, a button in Row, the Column stretch case, calculated child widths against measured visual bounds at 1.0×/1.25×/1.5×, and glyph draw clips against child rectangles.
- Updated the widget/layout guides and architecture text for DPI-before-layout measurement and intrinsic glyph bounds. No demo asset or backend-specific logic was changed.

### Verification and status

- MSVC Release build of `xui_tests` succeeded with warnings as errors. Direct suite run: **163 tests, 0 failures**; CTest: **1/1 registered test passed**.
- MSVC Release build of `xui_glfw_demo` succeeded, compiling the GLFW and OpenGL adapters. The final hidden profile measured **3,333 frames / 5.00068 seconds / 666.509 FPS**. This confirms hidden-loop throughput only; it does not establish visible FPS or visual interaction.
- The current CMake project has no `xui_hello` target and the repository has no separate native Win32 backend. Thus the fix was not verified in a second native backend or a `hello.exe`; the GLFW/OpenGL path is the only runnable graphics backend here. The requested visible window check also remains outstanding.
- `git diff --check` passes. Existing user edits in `examples/CMakeLists.txt` and new `examples/button`, `examples/layout`, `examples/text` directories were preserved.

### Remaining work

- Run the demo visibly at normal and fractional DPI, verify the text tail and wrapped case, then resize and interact while observing layout and clipping.
- Add equivalent verification if/when a distinct native Win32 renderer/platform backend is restored. Hidden profile numbers do not demonstrate the >60 FPS visible-interaction target.

## Two adjacent Row text labels — 2026-10-18

- Added a runtime regression using the exact two-child structure `Row { Text("World! Extension") Text("World! Extension") }`. It checks both parsed values, intrinsic widths against the full unwrapped text, non-overlapping child bounds, one-line layout at each child width, and all 15 non-space glyphs in each draw command.
- The focused scenario passes in the Release `xui_tests` executable; the complete suite reports **164 tests, 0 failures**.
- Rebuilt `xui_layout_demo` from the current source. This also refreshed the executable-directory copies of `layout.xui` and `layout.xss`, which were older than the source files before the build. The current layout demo target is `build/final-release/examples/layout/Release/xui_layout_demo.exe`.
- This verifies parser, layout, and draw-list output, but not a visible desktop session. Do not claim the screenshot symptom is visually resolved until that rebuilt executable is opened and inspected. If the same screenshot persists from that exact executable, capture the resulting child rectangles and renderer clip at runtime; the headless regression currently passes.

## Declarative animation and paint-style expansion — 2026-10-18

### Audit and decisions

- The XSS parser and `StyleSheet` already retained transitions, easing curves, and keyframe definitions, but the standard runtime did not animate them. `opacity`, alpha colors, `font-size`, numeric `font-weight`, nearest registered font selection, `size: auto`, and two-stop linear gradients already existed in partial form. They were extended instead of introducing duplicate concepts.
- Animations are authored in `.xss` only, as confirmed by the user. State transitions and named keyframes animate paint; there is no imperative animation API. The runtime uses a 16 ms animation deadline while a visible animation is active and returns to event-driven waiting after completion or when the animated node is hidden.
- Gradient support now includes linear, radial, and conic fills, with 2–4 evenly spaced stops. Explicit stop positions, gradient borders, and gradient text are not included. `glow` is an independent zero-offset halo; the existing `shadow` keeps its offset drop-shadow behavior. No glow texture cache was added because the renderer uses an SDF draw primitive.
- Missing font weights resolve to the nearest registered face, with no synthetic bold. The demo attempts to load a 700 face when available. Alpha changes paint only; hit testing remains active as requested.
- Content-sized `Button` nodes now lay out authored child content, rather than only their label, and use existing intrinsic sizing plus XSS padding/gap and min/max constraints. The standard `size: auto` model remains unchanged.

### Changes

- Added gradient/glow property IDs, compiler validation for gradient functions/stops, and draw-list payload values for up to four gradient colors. CPU SDF and GLSL implementations now evaluate linear/radial/conic gradients with rounded clipping and alpha.
- Added a distinct glow style path and animated paint interpolation for color/number values, state transitions, and keyframe playback. A hidden node does not tick; removing and later reapplying a keyframe animation starts it from the beginning.
- Nested child nodes on `Button` now use a column layout and clear the separate label to avoid double painting. Added tests for intrinsic nested content and min/max constraints.
- Nested showcase tabs exposed an ancestor-visibility bug: `isNodeVisible()` returned the selected status from the nearest `Tabs` instead of checking outer tab ancestors. It now rejects a non-selected page but continues the ancestor walk. This restored pointer targeting in all showcase sections and is covered by the existing showcase interaction test.
- Added the eighth showcase section and styling examples for paint, fonts, auto sizing, transitions, and keyframes. Updated `doc/styling-reference.md`, `doc/xss-guide.md`, `doc/widgets.md`, and `doc/architecture.md`.

### Trade-offs and limitations

- The OpenGL vertex payload grew to carry two more colors per vertex. This keeps gradient evaluation in the renderer and avoids per-widget CPU tessellation, at the cost of a larger static vertex stream.
- Glow adds an SDF shadow-style primitive per widget and is not cached. Large radii or many simultaneously glowing widgets can be expensive; no valid glow-heavy FPS number is available from this environment.
- Interpolation is restricted to compatible numeric values and colors. Layout geometry, font size/weight, and font face are not animated. Gradients require matching value shapes to interpolate.
- Gradient stops are evenly spaced; custom positions are deferred. `font-size` and `font-weight` were already supported style properties, so this work documents and demonstrates them rather than replacing their selection model.

### Verification and current status

- Windows Release build of `xui_tests` and `xui_glfw_demo` succeeded with the configured warnings-as-errors settings. CTest passed **1/1** registered test; the test binary reports **171 tests, 0 failures**. A separate nested-tab test verifies that a selected inner page cannot become visible or intercept input while one of its outer tab pages is hidden.
- Parser tests cover gradient and glow syntax; the CPU SDF test covers multistop linear/radial/conic color evaluation; runtime tests cover animated state/keyframe values, transparent-background hit testing, nested content sizing, and showcase interactions.
- The built demo's `--profile-glow` and `--profile-animation` modes were attempted both sandboxed and with elevated execution. Both runs stopped at “Could not initialize the renderer backend,” before frames were rendered. Desktop UI inspection also failed because the Computer Use native pipe was unavailable. Therefore there are **no measured FPS/ms figures** for these cases and no visible verification of gradient/glow appearance, animation cadence, or input feel. No >60 FPS claim is made for this change.
- `git diff --check` reports no whitespace errors after cleaning the newly added shader-line trailing spaces (the remaining output is Git's LF-to-CRLF normalization notice for the existing Windows checkout).

### Next steps

- Open the built GLFW demo in a desktop session with a working OpenGL context. Inspect each new style, hover/press transition, finite fade, and pulse animation; measure visible FPS in steady, glow-heavy, and animation-heavy states.
- If visible profiling shows glow below 60 FPS, reduce its primitive cost or add a renderer-level cache without moving style semantics into the backend.
- Verify font-weight face selection on a system where a 700 face is available, then repeat on a system where it is not.

## Animation controls, transforms, cursors, and docked window chrome — 2026-10-18

### Changes

- Added explicit facade playback controls for authored keyframes: `playAnimation(id, Once|Loop)` restarts a named visible animation, while `cancelAnimation(id)` freezes its current sample. Paused keyframes do not keep the event loop awake; active visible animations continue while the window is unfocused. `ProgressBar` keyframes can animate the numeric `value` property.
- Added paint transforms for nodes and descendants with inverse-transform hit testing for ordinary widget nodes. Transformed clips are converted to axis-aligned scissor bounds. `.xss cursor` now selects runtime cursor shapes and GLFW maps them to its standard cursor set, including hidden, with documented fallbacks for unsupported shapes.
- Refined ColorPicker format editing so HEX mode presents the HEX field and RGB/HSL/HSV modes present the fields for the selected representation. Added empty focused TextBox caret rendering and tests for the caret path.
- Raised floating dock panels to the front when activated, ordered their paint by z-order, and added a default shadow for floating panel chrome. Added a `DockTree` overlap test proving `panelAt()` follows raise order.
- `MenuBar` materializes as a styled horizontal client-area container, so menus can be authored with ordinary controls. Per the user's selected fallback, the native title bar remains. Dedicated Menu/MenuItem behavior, native decoration controls, drag-to-move window support, and menu-specific keyboard navigation are not included.
- Added a client-area File/Edit/View menu row to the showcase using ordinary Dropdowns and a Help Button. Added `--profile-menubar` alongside the animation, transform, glow, and docking profiles. Added paused ProgressBar keyframes with Play Once, Loop, and Stop controls wired from demo events.
- Updated the feature profiles in `examples/glfw_demo.cpp` and corrected stale animation claims in `doc/styling-reference.md` and `doc/troubleshooting.md`. Added menu and profile documentation to `doc/widgets.md` and `doc/performance.md`, animation event usage to `doc/animations-fonts-transforms.md`, and transform/animation data flow to `doc/architecture.md`; refreshed `plan/plan.md` with verification boundaries and remaining gaps.

### Decisions and trade-offs

- Animation authorship remains declarative in `.xss`; the C++ facade only starts/cancels an authored animation by widget ID. This preserves stylesheet-owned timing/property definitions while allowing application-controlled playback.
- Transforms apply to paint subtrees and ordinary node hit testing, as requested. Scissor APIs are rectangular, so rotated clip corners are approximate. Overlay passes and virtual hit targets need additional coordinate mapping before the transform contract is complete for every composite widget.
- GLFW uses its standard cursor shapes. Where GLFW has no matching diagonal resize, move, grab, or not-allowed shape, the adapter uses a documented fallback instead of platform-specific cursor artwork.
- MenuBar is kept inside the client area and the OS title bar stays enabled. The platform contract does not provide decoration removal or custom native-window movement, so a borderless/custom draggable menu bar would require a platform API design first.
- `paused` animations schedule no timer until explicitly started; visible active animations continue while unfocused. The platform interface does not report minimized/occluded status consistently, so hidden/minimized animation suspension is not promised.

### Verification

- Windows MSVC Release/Ninja rebuilt `xui_tests` and `xui_glfw_demo` with the repository's configured warnings-as-errors settings.
- Direct test run: **178 tests, 0 failures**, including transformed paint/hit testing, paused/active animation scheduling and playback, empty TextBox caret, cursor style override, MenuBar construction, and floating-panel raise/hit order.
- Sequential hidden GLFW/OpenGL section profiles from the rebuilt demo executable:
  - Animation: **3,554 frames / 7.54225 s / 471.212 FPS**.
  - Transform: **3,615 frames / 7.51053 s / 481.324 FPS**.
  - Glow/shadow: **3,248 frames / 5.00044 s / 649.543 FPS**.
  - Docking: **3,457 frames / 7.13242 s / 484.688 FPS**.
  - Client MenuBar: **3,489 frames / 7.12765 s / 489.502 FPS**.
- Animation, transform, glow, and docking profiles each selected the corresponding showcase state. The MenuBar profile exercises the always-visible client menu row with dropdown controls closed. All modes are hidden and uncapped; they are not measurements of on-screen refresh, animation cadence under vsync, resize performance, or input latency. The numerical target is exceeded for these loop-throughput runs only.
- CTest: **1/1 registered test passed** (3.72 seconds); the binary reports **178 tests, 0 failures**. The first attempt to invoke `ctest` by bare command failed because PowerShell did not have it on `PATH`; rerunning the Visual Studio-bundled CTest executable passed.
- These profiles validate hidden loop throughput and backend initialization only. No visible window, resize, pointer/cursor, keyboard, menu, caret blink, or normal-close session was available, so visible 60+ FPS and the end-user feel remain unverified.
- `git diff --check` was run; only the checkout's standard LF-to-CRLF normalization notices remain.

### Current status and next steps

- Core animation playback, ordinary-widget transforms/hit testing, style-selected cursor mapping, floating panel z-order, and client-area MenuBar layout have headless regression coverage.
- Remaining gaps: visible smoke testing; exact transformed clip shapes; transforms for popups, scrollbars, and tab-header virtual targets; cursor shape parity outside GLFW; a dedicated menu model and accessible keyboard navigation; and minimized-window animation suspension.
- Keep the native title bar. Before adding native borderless menu behavior, specify a cross-platform window-decoration and drag contract in `Platform`.

## OpenGL renderer initialization failure — 2026-10-18

- The demo's generic `Could not initialize the renderer backend` came from `GlDrawListRenderer::init()` returning false while compiling the updated GLSL 1.20 fragment shader. The multistop-gradient branch declared its two temporary stop colors as `a` and `b`, shadowing the function's scalar alpha accumulator `a`; the later scalar alpha assignment then targeted a `vec4`, making the shader invalid. The backend had returned no compiler log, so `Xui::attach()` could only expose its generic message.
- Renamed the temporary colors to `stopA`/`stopB`. Added optional `Renderer::lastError()` without making it a new implementation requirement, and made the OpenGL backend retain shader compile/link logs and buffer/function-loading errors. `Xui::attach()` now includes that diagnostic after the generic context if a renderer still fails.
- Rebuilt `xui_glfw_demo` and `xui_tests` in Windows Release. The demo now initializes and its hidden profiles complete: glow page **2,020 frames / 5.00018 s / 403.986 FPS**; animation page **3,190 frames / 5.00016 s / 637.98 FPS**. Direct test run: **171 tests, 0 failures**. These rates establish hidden-loop backend throughput only, not visible FPS or interaction smoothness.
- Current remaining verification gap: desktop visual inspection and normal visible-window interaction are still unavailable in this session. The initial startup regression is fixed and covered indirectly by the same backend initialization path exercised in both profiles.

### Follow-up report — 2026-10-18

- Rechecked the reported startup message against the current Release demo executable. `xui_glfw_demo.exe --profile-glow` initialized and completed at **366.075 FPS**; `--profile-animation` initialized and completed at **554.182 FPS**. Both use the GLFW/OpenGL backend, so the current executable no longer reproduces renderer initialization failure.
- The exact generic message is emitted by the older `Xui::attach()` path when its renderer does not expose a detailed error (or when the platform reports a zero-sized framebuffer). The current OpenGL renderer returns shader/context/buffer diagnostics through `lastError()`. The most likely cause is launching a stale executable or a different build tree; `build/final-release/examples/Release/xui_glfw_demo.exe` is the verified path in this checkout. There is no `xui_hello` target in the current CMake sources; older build directories may still contain that obsolete executable.
- Corrected `doc/getting-started.md`, which incorrectly claimed Windows builds an `xui_hello` target. The current build target is `xui_glfw_demo`.
- An incremental MSBuild refresh was attempted, but the process stopped producing output and was interrupted. Thus the profile results verify the existing Release binary, not a fresh rebuild in this follow-up. No source code changed in this follow-up.
- Next step if the error remains: launch the current `xui_glfw_demo` target from a fresh build directory and copy the complete error text, including anything after the colon. A current OpenGL shader failure should now include its driver log.

# 2026-10-23 — Build FreeType from the bundled source

- Audit: `lib/freetype-2.15.0` is a complete FreeType 2.15.0 source tree. No prebuilt library files were present. The vcpkg manifest listed only FreeType; CI used vcpkg only to install FreeType for Windows. There is no root `CMakePresets.json` or `.gitmodules`.
- Changes: replaced system package discovery with `add_subdirectory` for the bundled FreeType source and a stable `xui::freetype` interface target. The text library links that wrapper. The FreeType target is kept static. System zlib and optional bzip2, PNG, HarfBuzz, Brotli, and HVF integrations are disabled; FreeType keeps its internal zlib inflater.
- Removed package-manager wiring for the one declared dependency: removed `vcpkg.json`, the Windows vcpkg install/toolchain steps, Linux `libfreetype-dev`, macOS Homebrew FreeType installation, and the stale setup instructions. GLFW's unrelated system requirements remain.
- Decision: use source instead of platform-specific prebuilts. This keeps the dependency build consistent across Windows/macOS/Linux and avoids accidental system FreeType selection. Optional modules are disabled because Xui calls FreeType for variable `glyf` axis evaluation/outlines and does not use their services.
- Trade-off: configure/build now compiles FreeType each time in a clean build tree, increasing build time while removing a separately installed dependency. The vendored source is pinned to 2.15.0.
- Verification: fresh Windows MSVC Debug and Release configure/builds and the full 195-test suite now pass. The variable-font smoke tests report effective axis coordinates and confirm multiple weight instances render in the software renderer. Bahnschrift clamps 100 to 300 and 900 to 700; clamped requests are tested against the effective coordinate. Link rules show the Debug demo uses `build/.../_deps/freetype/freetyped.lib` and Release uses the corresponding bundled `freetype.lib`; these are static libraries, so no FreeType DLL appears in runtime imports. All example targets, including `xui_glfw_demo`, build in both configurations. No visible desktop interaction/FPS claim is made here.
- Status/next steps: the bundled source is compiled into a static FreeType library and linked by Xui targets. Verify CI on Linux/macOS and perform a visible GLFW smoke test when a desktop session is available.

# 2026-10-23 — Fix variable font-weight face selection

- Diagnosis before the fix: `.xss` parsing and cascade retained numeric weights 100, 300, 400, 500, 700, and 900. FreeType exposed Bahnschrift's `wght` axis with min/default/max **300/400/700**, and `FT_Set_Var_Design_Coordinates` accepted the corresponding fixed-point values. `GlyphAtlas` already keyed entries by unique font cache ID, glyph index, and pixel size, so separate weight instances were not sharing glyphs.
- The missed runtime case was face selection. Rules without `font-family` returned the first loaded face directly, bypassing registered faces under the default family. The showcase has weight rules without an explicit family. Also, when a family did contain both variable and static faces, nearest registered-face selection could choose static bold before consulting the variable axis. The old runtime test named the variable family explicitly and registered no competing static face, so it did not cover either failure.
- Changes: implicit-family styles now resolve through the registered `default` family. A variable face is selected when its clamped axis coordinate is closer to the requested weight, and it wins equal-distance ties against a static face. Static-only families still choose the nearest registered face and do not synthesize bold. Added opt-in `XUI_TRACE_FONT_WEIGHT=1` diagnostics for axis metadata and `FT_Set_Var_Design_Coordinates`, runtime face selection, the actual FreeType coordinate at glyph loading, and atlas keys. Added tests that reproduce the default-family + registered static-bold setup, inspect runtime-selected weights, compare rendered rows for each distinct effective weight, and assert one glyph has one atlas entry per distinct effective weight.
- Diagnostic values for Bahnschrift: parser/cascade requests **100, 300, 400, 500, 700, 900**; effective axis values **300, 300, 400, 500, 700, 700**. `FT_Set_Var_Design_Coordinates` logs fixed coordinates **19,660,800; 26,214,400; 32,768,000; 45,875,200** for the distinct effective weights. The glyph-load trace confirms the same axis values immediately before FreeType loads glyphs. Atlas test: one glyph at 28 px creates **4 entries for 4 distinct effective weights**; repeat requests create no more entries. Software-rendered row coverage counts were **1485, 1485, 1857, 2092, 2453, 2453**; row comparisons prove each distinct effective weight produces different pixels, and clamped pairs are identical.
- Decision/trade-off: preserve the previously agreed static fallback (nearest registered face, no synthetic bold). Prefer the variable font on an equal-distance tie because it can honor the authored axis and should not be masked by a static companion face. Trace output is disabled unless its environment variable is set.
- Verification: Windows MSVC Debug and Release builds of `xui_tests` and `xui_glfw_demo` succeeded. Full suite in both configurations: **195 passed, 0 failed** with `XUI_VARIABLE_FONT=C:/Windows/Fonts/bahnschrift.ttf`. The latest five-second Release GLFW `--profile-animation` run with `XUI_DEMO_FONT` set to Bahnschrift exited normally at **401.733 FPS** without tracing. A separate diagnostic run with tracing enabled also exited normally; tracing output is not used as the performance baseline. The profile exercises real GLFW/OpenGL frames but does not replace manual visual inspection or interaction testing. Linux/macOS remain unverified.
- Remaining scope: this change addresses `font-weight` only. Suggested later text-feature slice: explicit line-height, text decoration, text transform, and overflow/ellipsis controls, after choosing syntax and behavior. No broader text features were added.

## 2026-10-24 — Fix damaged variable-font glyph fills and layout demo styling

- Diagnosis: the screenshot is produced by `examples/layout/layout.xui` and its `.xss`. The title rule explicitly requested `font-size: 100px` and omitted a foreground color, explaining the oversized wrap and dark, low-contrast text. The two row strings are fully visible in the screenshot, so Row tail clipping is not reproduced there. Separately, `src/text/rasterizer.cpp` claimed non-zero fill but actually filled sorted edge pairs (even-odd). TrueType variable/composite glyphs can contain overlapping contours; even-odd fill cancels their overlap and creates the rectangular gaps visible in the glyphs.
- Changes: raster scanlines now accumulate signed crossings and fill spans while the winding count is non-zero. Coincident crossings are grouped to avoid seams. Added `rasterizer_uses_nonzero_winding_for_overlapping_and_nested_contours`, which verifies same-direction overlapping contours remain solid while a reversed inner contour remains a counter. Updated `examples/layout/layout.xss` to use a 32px title, explicit high-contrast foreground colors, and consistent page/row padding and gap. Added a troubleshooting note about contour-fill artifacts.
- Decision/trade-off: use the TrueType/OpenType non-zero winding rule at the shared rasterizer, rather than special-casing FreeType glyphs or the GLFW backend. The same correction applies to static and variable outlines. Kept the `.xui` structure and text content unchanged; only the demo theme controls title sizing/contrast.
- Verification: Windows MSVC Release build completed for `xui_tests` and `xui_layout_demo`; the test executable reports **198 passed, 0 failed**, including the new overlapping-contour/counter test and existing Roboto variable-weight tests. CMake rebuilt/copied the updated layout example assets.
- Verification limits: no visible GLFW desktop inspection was available in this run, so the corrected appearance in the user's exact screenshot is not visually confirmed. No interactive FPS sample was taken; this change only alters raster fill work on newly baked glyphs and demo style values. FPS, visible resize, and interaction remain unverified here.
- Current status / next steps: the known contour-fill cause is corrected and covered by a synthetic regression. Run `build/examples/layout/Release/xui_layout_demo.exe` to visually check the title, glyph contours, and two complete Row labels; then inspect the full showcase with Roboto at the intended production sizes.
## 2026-10-24 — Add The Ashen Gate turn-based game UI example

- Choice: implement the user-selected turn-based encounter, with combat actions driven by UI events. `Xui::run()` does not expose a per-frame application callback, so the sample does not add a parallel GL render loop or bypass the backend interfaces. The agreed scope is an event-driven game-UI sample; real-time simulation and per-frame HUD updates are out of scope.
- Added `examples/game/` with `game.cpp`, `game.xui`, `game.xss`, a target-specific `CMakeLists.txt`, and `README.md`. Six authored pages cover Main Menu, Encounter, Paused, Settings, Inventory, and Results. The UI document owns widget structure; the stylesheet owns the theme, control states, and notice animations. C++ implements strike/heavy strike, guard, healing, difficulty, equipment bonuses, and state-to-widget updates through the existing typed value/event API.
- Added `xui_game_demo` under the existing GLFW example option and copy rules for the game documents and the shared Roboto font. Root README and `doc/README.md` now link the sample. Added `turn_based_game_example_assets_parse_validate_and_build` to parse, validate, build, lay out, and check its authored control IDs in the null backend.
- The first build caught an unused local under warnings-as-errors. The asset test then caught malformed `.xui` property delimiters and button callbacks placed in the constructor parameter list instead of the widget property block. Fixed these at the example source; added a live `Hints: on/off` readout update alongside the combat hint.
- Trade-off: the pause page is a tab, not a focus-trapping modal; there is no game canvas or sprite rendering; idle gameplay has no simulation loop. These choices exercise framework UI without introducing platform-specific rendering or changing the public run-loop API.
- Verification: Windows MSVC Release build succeeded for `xui_tests` and `xui_game_demo`; the target links the GLFW/OpenGL backend libraries. The full test executable reports **199 tests, 0 failures**, including the new game asset/runtime case. `git diff --check` found no whitespace errors; its output contains existing LF-to-CRLF working-copy notices.
- Verification limits: the game was not visibly launched and played in this session. Therefore no claim is made about visible widget appearance, interactive playthrough, live resize, visible FPS, or idle CPU. The 60+ FPS target is not established for this game sample; its loop is event-driven and only redraws for UI work/animation.
- Next: run `build/examples/game/Release/xui_game_demo.exe` visibly; play through start, combat, pause/resume, settings, inventory, defeat/victory, and restart; resize during combat; record visible FPS and idle behavior. Fix any runtime findings without moving UI structure or styling into C++.

## 2026-10-24 — Phase F: isolate optional build features and backend targets

- Audit: the core targets do not include FreeType, HarfBuzz, LunaSVG, or stb_image headers from public headers. Image implementation was already gated by `XUI_ENABLE_IMAGE_SUPPORT`; when disabled, `xui_image` retains its public API and reports a clear unsupported result. FreeType and HarfBuzz are required by the current text implementation and are built from bundled source. Benchmarks/fuzz harnesses are isolated under `XUI_BUILD_BENCHMARKS`; tests use the in-repository harness. GLFW/OpenGL adapters were previously created only as a side effect of enabling the GLFW example, which prevented selecting the backend independently.
- Changes: added `XUI_BUILD_GLFW_BACKEND` (default OFF) so the adapters can be selected without examples. The existing `XUI_BUILD_GLFW_EXAMPLE` path still adds the backend automatically when examples are enabled, preserving the previous default configuration. Moved GLFW source setup, OpenGL discovery, and backend target creation to the root build gate; `examples/CMakeLists.txt` now only creates examples and checks that required adapter targets exist. Changed FreeType/HarfBuzz linkage on `xui_text` from `PUBLIC` to `PRIVATE`, keeping dependency include paths out of downstream compile usage requirements while retaining static link dependencies. Updated architecture and setup docs with the option contract and required-vs-optional dependency distinction.
- Decisions/trade-offs: text shaping/outlines are not disabled because those bundled libraries are required for the framework's current text functionality. Static consumers still receive the link-only requirements through CMake; public headers do not expose those library APIs. `XUI_BUILD_GLFW_EXAMPLE` remains ON by default for compatibility, so a minimal build should explicitly disable both that option and examples; backend-only users can set `XUI_BUILD_GLFW_BACKEND=ON` and examples OFF.
- Verification: fresh Windows MSVC Release/Ninja minimal configure used tests ON, benchmarks/examples/GLFW example/backend/image support OFF; all 156 build steps completed and CTest passed **1/1** (the direct suite reports **204 passed, 0 failed**). A separate fresh normal configuration with tests/examples/GLFW demo/image support ON built `xui_tests` and `xui_glfw_demo`; CTest passed **1/1** and the direct suite again reported **204 passed, 0 failed**. A third fresh configuration with examples/tests/benchmarks/image support OFF and `XUI_BUILD_GLFW_BACKEND=ON` built both `xui_backend_glfw` and `xui_backend_opengl`. The minimal build's test compile command contains no FreeType, HarfBuzz, LunaSVG, or stb_image include paths. Configure logs show HarfBuzz's existing advisory that its CMake build support is community-maintained; it did not prevent either build.
- Limits: only Windows MSVC Release was tested. No visible demo interaction/FPS check was performed because this phase changes build wiring and dependency exposure, not runtime behavior. Linux/macOS, shared-library consumer packaging, and an installed/exported package remain unverified/out of scope.
- Status/next steps: optional image support and backend selection are isolated by explicit options, and the full existing default example path remains buildable. Continue with Linux/macOS CI results and consider package export separately; keep FreeType/HarfBuzz required until a deliberate text-feature policy says otherwise.

## 2026-10-24 — Phase G: align CI with current targets and cache bundled dependencies

- Audit: `.github/workflows/ci.yml` is the only workflow. It runs on push and pull request with Ubuntu GCC 13, Ubuntu GCC 13 ASan/UBSan, Ubuntu Clang 18/libc++, macOS 14, and Windows 2022 jobs. Each job configures tests and examples and builds `xui_tests` plus `xui_glfw_demo`; it did not build the Tetris `xui_game_demo`. The workflow had no dependency build cache. There are no release/publish jobs, credentials, or external service integrations.
- Changes: added an `actions/cache@v4` entry for `build/_deps/freetype`, `build/_deps/harfbuzz`, and `build/_deps/lunasvg`, keyed by runner OS, matrix job/toolchain, root CMake configuration, and vendored dependency source hashes. CI now builds `xui_tests`, `xui_glfw_demo`, and `xui_game_demo` in every existing matrix job. Existing triggers, platforms, compiler variants, dependency installation, and sanitizer coverage are unchanged.
- Decisions/trade-offs: cache only the bundled dependency sub-builds rather than the complete build tree, so project targets still configure/build from the current checkout. Keep the current OS/compiler matrix because the framework and GLFW build currently target those platforms; do not add unproven platforms or hide failures with `continue-on-error`. The cache uses the official GitHub Actions cache service and requires no secrets.
- Verification: the new backend/CMake arrangement was clean-configured on Windows MSVC Release. `xui_tests` and `xui_glfw_demo` built; CTest passed **1/1**, and the suite reported **204 passed, 0 failed**. `xui_game_demo` then built successfully. This locally verifies the Windows commands/targets, not the hosted workflow or Linux/macOS jobs. `git diff --check` passes apart from the checkout's LF-to-CRLF normalization notices.
- Limits/status: no local YAML parser or `actionlint` executable is installed, so the workflow was statically reviewed but not parser-linted. It has not been dispatched to GitHub during this task. Cache hit rate and platform results remain unknown until hosted CI runs. No visual runtime/FPS claim is made.
- Next: inspect the hosted matrix results and cache hits on the next push/PR; fix any real platform-specific build failures without suppressing them. Keep the dependency hash in the cache key if vendored source versions change.

## 2026-10-24 — Add text-path telemetry, benchmark, and fix cache-index growth

- Audit: `TextSystem` already cached layouts by font/text/size/wrap width and `GlyphAtlas` retained a lifetime rasterization total, but neither reported per-frame cache or atlas activity. No benchmark isolated text layout/shaping from full runtime rendering. Inspection of the open-addressed layout cache showed that growing `m_index` with `vector::resize()` retained old slots under a smaller bit mask instead of rehashing them for the larger table.
- Changes: added `TextSystem::FrameStats` for layout requests, hits, misses, rehashes, shaping runs, shaped glyphs, and elapsed HarfBuzz shaping nanoseconds. Added `GlyphAtlas::FrameStats` for rasterizations, uploads, and rebuilds. `Xui::renderFrame()` resets the text counters before runtime layout, so intrinsic measurement and paint requests share one frame's stats. Added `bench_text` with cached static text, 400 distinct input edits, cached long wrapped text, and a repeated 300-width resize sweep. Added tests for counter reset/accuracy and for retaining all 256 cache entries across index growth.
- Root fix: when the layout index grows, rebuild its open-addressed slots from all retained cache entries using the new mask. The old behavior caused repeated layout misses once the table grew, including in a width sweep that should have had one cold pass and one warm pass.
- Decision/trade-off: keep stats attached to the text components and reset them at the existing frame boundary. Time only actual HarfBuzz `shapeText()` calls, which avoids charging cache hits. The benchmark uses `NullRenderer` and Painter draw-list assembly to isolate CPU text work; it intentionally makes no claim about GPU submit/presentation or on-screen frame rate.
- Baseline before cache fix, Windows MSVC Release, same 600-operation width sweep: **119.2 µs/op**, 343 misses / 257 hits, 14,063 shaping runs, 19.76 ms cumulative shaping, 30 glyph rasters/uploads, and no atlas rebuilds. The extra 43 misses proved the index-growth fault. Static cached label: 3.39 µs/op in that initial run.
- After fix, three consecutive runs: width sweep **99.16 / 96.96 / 102.40 µs/op** (median **99.16 µs/op**), exactly 300 misses / 300 hits, 12,300 shaping runs, 15.99–16.95 ms shaping total, 30 atlas rasters/uploads, zero rebuilds. The 99.16 µs median is about 16.8% below the one-run pre-fix sample; compare cautiously because the baseline is a single sample. Static label median across the three runs: **2.66 µs/op**, 10,000 hits, no shaping or atlas work. The 400 unique edits cost 45.76–48.78 µs/op with 400 misses and 1,200 shaping runs. A 3,000-operation warm wrapped paragraph cost 15.64–16.26 µs/op with all cache hits. These are microbenchmarks, not visible UI or FPS measurements.
- Verification: Windows MSVC Release built `xui_tests`, `bench_text`, and `xui_glfw_demo` under warnings-as-errors. Direct test run: **201 passed, 0 failed**, including the new cache-rehash and text-counter cases. `bench_text` completed three runs. The rebuilt GLFW demo's hidden animation profile completed at **425.29 FPS over 5.04 seconds**. This confirms the app initializes and its hidden loop runs, but is not visible FPS or resize/input verification. Debug, Linux, and macOS were not tested.
- Current status / next steps: per-frame text diagnostics and a reproducible benchmark are available; index growth now preserves cache hits. Next, use visible profiling to decide whether cold shaping/measurement during typing and resizing is a real frame-time bottleneck. The image/SVG slice is recorded below; native-window precedence, Canvas/Tetris, and CI remain later approved phases.

## 2026-10-24 — Add optional stb_image and LunaSVG image loading

- Audit: `lib/std_image/stb_image.h` is the bundled single-header decoder; `lib/lunasvg-3.5.0/` contains LunaSVG source plus plutovg. The `Image` runtime previously decoded only the project's custom PNG format via `readPng()`. The showcase generated a PNG from C++ at startup and its asset path depended on the working directory. Renderer already exposed RGBA8 texture creation, so image loading could remain in the shared resource layer without backend-specific rendering code.
- Changes: added `xui_image` as a separate target and `XUI_ENABLE_IMAGE_SUPPORT` (default ON). stb_image is limited to PNG/JPEG/BMP/TGA; LunaSVG and plutovg build as static CMake subprojects. `readImage()` returns straight-alpha top-left RGBA8 pixels and validates dimensions before allocation. `SvgImage` owns a parsed LunaSVG document and rasterizes aspect-preserving output on demand. Runtime images resolve relative paths from the entry `.xui` directory, use natural intrinsic dimensions for auto sizing, upload through `Renderer::createTexture`, and release textures when the renderer detaches. SVG raster output is reused until a 64-pixel physical-size bucket changes. Existing `readPng()`/`writePng()` remain unchanged for golden fixtures.
- Demo: replaced the C++-generated PNG with checked-in `examples/xui-demo-image.svg`, featuring vector paths and linear/radial gradients. `showcase.xui` refers to the SVG; CMake copies it beside the executable with the `.xui` and `.xss` files.
- Decisions/trade-offs: build-time optional support avoids making image dependencies mandatory for headless/minimal consumers. LunaSVG source is preferred over a system package for consistent implementation and to use the user-provided copy. Maximum decoded/raster dimensions are 16,384 pixels per side and 64M pixels total. SVG text, filters, masks, clip paths, and animation are explicitly not supported. Image alpha and texture behavior stay in the shared RGBA path; layout/style remain in `.xui`/`.xss` and renderer/platform interfaces remain unchanged.
- Verification: Windows MSVC Release configured with `XUI_ENABLE_IMAGE_SUPPORT=ON`; `xui_tests` and `xui_glfw_demo` compile under warnings-as-errors. Direct test run reports **202 passed, 0 failed**, including a generated-PNG decode roundtrip and LunaSVG intrinsic-size/raster pixel checks. The `--profile-images` GLFW/OpenGL run selected the Display page, completed normally at **410.077 FPS over 5.0015 seconds**, and exercised both SVG Image nodes through the real renderer path. This is an uncapped visible-window profile, not a manual visual quality check. The demo links bundled static LunaSVG/plutovg.
- Verification limits: the actual texture path ran, but the artwork was not manually inspected for visual correctness. Windows Release with `XUI_ENABLE_IMAGE_SUPPORT=OFF` also builds `xui_tests` and reports **202 passed, 0 failed**, after which the normal ON configuration was restored and the test/demo targets rebuilt. Linux/macOS are not tested. JPEG/BMP/TGA are enabled in stb_image's compiled decoder set but do not yet have dedicated fixture tests.
- Next: inspect SVG appearance and contain/crop visually; test Release/Debug and Linux/macOS; add tiny JPEG/BMP/TGA fixtures and exercise the disabled option in CI.
+

# 2026-10-24 — Resolve native window defaults and custom-chrome corners (Phase C)

- Audit: `Xui::attach()` previously created the native window from `XuiOptions` before the runtime build, while `.xui Window(title, size)` was only retained as runtime metadata. The `.xss` compiler already accepted general `size` and `radius` properties, but no window-specific title declaration existed. The GLFW Windows backend already strips the native frame and owns `WM_NCHITTEST`, making it the backend boundary for DWM window-corner hints and matching hit tests.
- Changes: added `.xss window-title` and resolved root `Window` style defaults before creating the platform window. Native title and size now follow per-field precedence: explicit `XuiOptions` > `.xui Window(title, size)` > `.xss window-title/size` > facade defaults (`Xui`, 720×440). Non-default direct option assignments count as explicit; three override flags let callers explicitly select a default-valued title/width/height. The effective metadata is synchronized back into `UiRuntime`, and `setWindowTitle()` continues to update runtime metadata.
- Custom corners: added a backend-neutral preferred `WindowDesc::cornerRadius` sourced from the root `Window` XSS `radius`. On Windows, the GLFW backend calls `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)` for a client-chrome window, records whether DWM accepted the hint, and excludes the outside rounded pixels from hit testing only for restored windows. If DWM rejects the preference (including unsupported Windows versions), hit testing stays rectangular. Maximized windows stay rectangular/work-area-sized. CMake links `dwmapi` with the Windows GLFW backend. Other platforms retain the native window shape.
- Decisions/trade-offs: native title and size are startup configuration, so switching XSS themes after attach does not recreate or resize the window. DWM rounding is a preference and may be ignored by Windows; this change does not use `SetWindowRgn` or platform-independent shaped windows. Rounded hit testing is gated on DWM success to prevent dead corner regions when the platform draws square corners. Radius values are clamped to 0–64 logical pixels before being passed to the platform.
- Tests: added `xui_facade_resolves_window_defaults_with_fieldwise_precedence`, checking stylesheet defaults, `.xui` overrides, explicit option overrides, a width-only option override, explicit facade-default flags, resolved runtime metadata, and root radius transmission into `WindowDesc` via `NullPlatform`.
- Verification: Windows MSVC Release build succeeded for `xui_tests` and `xui_glfw_demo` under warnings-as-errors, including the Windows DWM code and CMake link. Direct `xui_tests.exe` run: **203 passed, 0 failed**. GLFW idle profile exited normally: **465.384 FPS over 5.00017 seconds**. This profile verifies startup and loop throughput, not visible frame pacing or resize responsiveness.
- Limitations: this environment did not perform manual visual inspection of DWM rounded corners, hit-test feel at the corner edge, or maximize/restore/minimize interaction. Windows build evidence is available; macOS/Linux were not built or tested in this phase. The >60 FPS professional-use criterion remains unverified for normal visible interaction and resize.
- Next: visually check rounded restored windows and corner hit testing on Windows 11, including maximized/restore transitions; verify native title/size in the real GLFW demo; run platform builds on Linux/macOS. Then proceed to the next approved phase in the original request.

## 2026-10-24 — Replace the turn-based game with Tetris (Phase E)

- Audit: the existing game was still The Ashen Gate and had no timed application callback. The runtime recognizes `Canvas` in schema metadata but does not construct or paint it, so a Canvas-based board would need unsupported render infrastructure. The UI runtime already supports value-backed `ProgressBar` nodes and `Xui::run()` owns the event loop.
- Changes: replaced the game pages and styling with a Tetris menu, game board, pause, settings, and game-over pages. `game.xui` declares all 200 cells, controls, readouts, and FPS display; `game.xss` owns the palette, focus/hover/pressed states, and line/game-over keyframes. Game logic implements a seven-piece shuffled bag, rotation with horizontal wall kicks, collision/locking, row clears, drop and line-clear scoring, level progression, and gravity acceleration. Only cells whose occupied state changes are sent through `UiRuntime::setValue()`.
- Runtime integration: added `Xui::setTickHandler(callback, context, intervalMs)` to schedule app updates on the existing UI thread. The timer shares the event-loop wait deadline with runtime animations; hidden windows pause the callback, and disabling the handler returns the loop to normal event-driven waiting. Added a scheduler test. No thread, backend-specific renderer, or Canvas implementation was introduced.
- Decisions/trade-offs: the board uses 200 existing styled ProgressBar widgets to preserve the backend abstraction and value API. This keeps state updates declarative, but all tetrominoes share one color and the example uses buttons rather than keyboard input. A separate Canvas/path renderer was deferred because the runtime has no Canvas painting contract and was not needed for this phase. The tick API is generic for applications that need periodic updates.
- Documentation: rewrote `examples/game/README.md`, updated the root and documentation indexes, and described the scheduled callback in Getting Started and the architecture data flow. The roadmap now identifies which parts are headless-verified and which remain visually unverified.
- Verification: Windows MSVC Release configured/build from `build/docking-ninja` with the Visual Studio developer environment; `xui_tests` and `xui_game_demo` linked successfully. Direct test run: **204 passed, 0 failed**; CTest reports **1/1 passed**. The Tetris asset test parses, validates, builds, lays out, and finds all 200 cell value IDs plus HUD, settings, and page controls. The tick test verifies multiple callback invocations and elapsed time during a bounded facade run. Final `git diff --check` reports no whitespace errors; Git only notes working-tree LF/CRLF normalization.
- Verification limits: no desktop playthrough or visible GLFW launch was completed in this pass. Therefore appearance, button hit behavior, live resize, normal close, idle CPU, and the >60 FPS target are not claimed as verified. Linux/macOS were not tested.
- Current status / next steps: game rules and asset/runtime integration compile and pass headless coverage. Run the game visibly; exercise menu/start, movement, rotations, soft/hard drop, line clearing, settings, pause/resume, top-out, restart, close, and resize. Measure visible FPS during active play and while paused. Consider per-piece colors and keyboard controls only after the current UI implementation is visually checked.

## 2026-10-24 — Showcase images and window defaults (Phase H)

- Audit: the GLFW showcase already had SVG image examples, a custom `TitleBar`, and a two-weight text sample. The Display page had no raster image, the tracked PNG asset was not copied beside the demo, and the root `.xui Window(title, size)` took precedence over the `.xss` defaults. The Windows backend already requests DWM rounded corners from root-window `radius`; other platforms retain their native window shape.
- Changes: rewrote the root showcase window declaration to leave title and size to `.xss` defaults while preserving the authored custom titlebar. The Display section now presents the checked-in PNG and SVG side by side with captions and hover tooltips. The build copies the PNG alongside the existing XUI, XSS, and SVG files. The showcase layout regression test verifies both image nodes have nonzero bounds, do not overlap, and remain alongside the toolbar/statusbar. No runtime or backend logic changed.
- Style/API demonstration: the root `Window` rule now sets `window-title`, `size`, and `radius`, making the documented precedence observable in the example. The existing Motion & Style page remains the text-weight example; static font faces are used unless a variable font is explicitly supplied, so this is not a claim that the bundled demo font uses variable axes.
- Decisions/trade-offs: reused the already tracked PNG instead of generating demo art at startup, keeping asset loading representative and avoiding C++ demo-only layout/render logic. Kept the showcase's established sections and placed the two images inside the Display category rather than adding a new top-level page.
- Verification: Windows MSVC Release built `xui_tests` and `xui_glfw_demo` successfully. Direct test executable: **204 passed, 0 failed**; CTest: **1/1 passed**. Confirmed the PNG and SVG are copied beside the executable. The visible `--profile-images` run completed 1,988 frames in 5.0017 seconds (**397.465 FPS**) and exited normally. This profile exercises the Display section/image renderer path, but is not a manual visual inspection or a resize/input latency measurement.
- Limits/current status: Windows Release only; Linux/macOS were not tested. The visual quality of the artwork, Windows DWM rounded corners, and native title/size appearance still need manual inspection. The FPS sample is above 60 but does not verify live resizing, interactions, or frame pacing under load. No performance or runtime claims are inferred from the headless layout test.
- Next: inspect the demo visually, exercise resize and native maximize/restore, and run the same build/profile on Linux and macOS. Validate appearance and font-weight differences with the intended variable font separately.
