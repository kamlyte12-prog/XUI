# Architecture and backend development

Xui separates authored document data, style resolution, retained layout, drawing commands, rendering, and native window/input handling. Applications describe a UI once in `.xui` and `.xss`; backend implementations decide how those platform-neutral commands reach a window.

## Components

| Component | Responsibility |
| --- | --- |
| `xui::Xui` | Recommended app facade. Loads font/UI/style files, creates the backend window, builds runtime state, runs events/layout/render, and cleans up. |
| `.xui` parser/loader/schema | Parses the widget tree and imports, validates known object parameters, and reports source diagnostics. |
| `.xss` parser/compiler | Parses selectors, tokens, states, transitions/keyframes; compiles declarations into a `StyleSheet`. |
| `UiRuntime` | Builds the supported widget subset, matches styles, handles layout and interactions, and emits paint operations. |
| `LayoutTree` / `DockTree` | Retained logical-pixel layout for flow rows/columns, Split panes, scroll views, and leaves; `DockTree` owns recursive docking groups, tabs, split ratios, floating panel bounds, and min/max constraints. |
| `TextSystem` / glyph atlas | Loads TTF faces, shapes/wraps text, rasterizes glyphs, and stores coverage textures. |
| `Painter` / `DrawList` | Converts logical paint operations into clipped, DPI-scaled, batched backend-neutral geometry and glyph quads. |
| `Renderer` | Owns drawing device/surface/textures and executes `DrawList` frames. |
| `Platform` | Owns native window(s), normalizes input events, reports logical/pixel dimensions and DPI, waits while idle, and provides context/presentation hooks. |

For the agent-facing module/source map and change boundaries, see [`plan/architecture.md`](../plan/architecture.md). Project invariants and anti-regression checks are in [`plan/agents.md`](../plan/agents.md).

The public interfaces allow a Win32-specific implementation, but the current interactive sample targets use GLFW on Windows, Linux, and macOS; this tree does not currently build a separate native Win32 window backend. The bundled examples pair GLFW with an OpenGL renderer. Neither `.xui` nor `.xss` calls either backend directly.

## Data flow

```text
main.cpp
  Xui::loadFont / loadXui / loadXss
       |            |           |
       |            v           v
       |        XUI parser    XSS compiler
       |        + imports     + tokens/styles
       +------------+-----------+
                    v
       Xui::attach(Renderer&, Platform&)
                    |
       UiRuntime::build + TextSystem glyph atlas
                    |
       Xui::run: normalized events <---- Platform
                    |
          LayoutTree (logical pixels)
                    |
          UiRuntime::paint -> Painter -> DrawList
                    |
          Renderer begin/submit/end/present
                    |
          Platform swap/presentation hook
```

Input travels in the opposite direction: `Platform::pumpEvents()` yields normalized pointer/key/text/wheel/window events; `Xui::run()` dispatches supported input to `UiRuntime`; a changed interaction requests another frame. Scroll offsets are updated without remeasuring the subtree. ScrollView clips are applied to each visible descendant's draw and hit rectangles. Docking pointer events update only the DockTree and the affected host subtree; panel headers, tab groups, splitters, and previews are painted by the normal overlay path with XSS-resolved styles. Floating dock panes are ordered by their retained z-order, and pointer activation raises the pane before routing subsequent input. Smooth scroll, delayed tooltip deadlines, and active visible animation deadlines are passed into the platform wait timeout, so static and paused apps do not need a polling loop. Resizes update renderer framebuffer dimensions/DPI and rerun layout using logical client dimensions; the DPI scale is supplied before layout so intrinsic text measurement uses the same physical font size as glyph painting. Backends whose native resize enters a modal loop can use the optional `Platform::setLiveResizeHook`; the GLFW example invokes the facade's full render-frame path from each size callback.

## Backend-neutral coordinate and event contract

The `Platform` contract documents pointer coordinates and logical client dimensions in logical pixels with a top-left origin. `windowSizePixels()` reports physical framebuffer dimensions. `dpiScale()` maps logical size to physical pixels. Concrete platforms must normalize native coordinates before returning events.

The runtime does not handle native messages. The platform queues and returns `xui::Event` values, including `MouseMove`, `MouseDown`, `MouseUp`, `Wheel`, `KeyDown`, `TextInput`, `Resize`, `DpiChange`, `CloseRequest`, and `Quit`. File/IME event data is platform-owned according to the interface comments and must stay valid for the documented period.

The shared text layer shapes/measures text and owns a glyph atlas. HarfBuzz 15.0.0 shapes each left-to-right text run and applies OpenType GPOS positioning, including variable-font positioning deltas. The same requested `wght` coordinate is sent to the HarfBuzz font and FreeType face. FreeType 2.15.0 supplies variable-axis metrics and TrueType outlines; the atlas rasterizes those outlines with the project's software rasterizer. CMake builds both bundled dependencies as static libraries exposed to the text target through `xui::harfbuzz` and `xui::freetype`; neither is a renderer or platform backend. The text layout currently disables `liga` and `clig` substitutions to keep editable text's glyph/caret mapping simple. See [font support and layout limits](animations-fonts-transforms.md#font-file-and-glyph-support) and [Getting Started](getting-started.md#build-xui) for dependency layout and scope.

### Paint order and pointer targeting

The retained tree paints parents before descendants and earlier siblings before later siblings. Pointer targeting walks visible nodes in reverse paint order, stops at the first painted hit surface, and updates only that target's hover/focus/press state. Transparent `Row` and `Column` nodes pass through; painted panels, text, disabled controls, and controls block widgets behind them. Inactive tab and dock-panel subtrees are excluded from painting and pointer targeting, including dock hosts on inactive showcase pages. Dock title/tab chrome, splitters, previews, and floating overlays are styled subparts on the shared overlay layer. Dropdown/color-picker popups remain above the tree and dock chrome; tooltips remain pointer-transparent and are hidden while popups are open. Arbitrary `.xss z-index` and generic `overflow` clipping are not implemented, so ordinary stacking follows tree order and fixed overlay layers.

`.xss transform` is composed through widget descendants during paint, and ordinary node pointer tests invert that matrix so input follows the painted widget. Rotated clips use an axis-aligned scissor bound, while overlay passes and virtual subtargets such as tab headers/scrollbars are not independently transformed. This is a node-level transform contract, not a complete transformed-overlay system; see [transform limits](animations-fonts-transforms.md#widget-transforms).

## Implement `xui::Renderer`

See [`renderer.hpp`](../include/xui/render/renderer.hpp). A concrete renderer must implement:

1. `init(RendererDesc)` and `shutdown()` for device/surface lifetime.
2. `resize(width, height, dpiScale)` for physical framebuffer changes.
3. `createTexture`, `updateTexture`, and `destroyTexture`. `A8` stores glyph coverage; `RGBA8` stores color textures. Preserve handle validity/lifetime semantics.
4. `beginFrame(FrameInfo)`, `submit(DrawList)`, `endFrame()`, and `present()`. Consume commands, vertices, indices, scissor clips, texture references, and custom commands in draw-list order.
5. `caps()` with only capabilities the renderer implements.

`DrawList` data is temporary and remains valid only until the next painter frame. Copy or consume it synchronously. Honor framebuffer pixel dimensions and premultiplied-alpha conventions used by the draw-list renderer. The optional custom-command callback is installed with `setCustomHandler()`.

The bundled `GlDrawListRenderer` is in [`backends/opengl/gl_drawlist_renderer.cpp`](../backends/opengl/gl_drawlist_renderer.cpp). `SoftRenderer` is a CPU reference renderer for tests/headless work; its full-frame raster cost is not suitable as a high-frame-rate interactive backend at common window sizes.

## Repository layout and backend targets

Framework implementations and public headers remain under `src/` and `include/xui/`. Optional concrete adapters live at the repository root under `backends/`: `backends/glfw/` implements `Platform`, and `backends/opengl/` implements `Renderer`. The example application itself stays in `examples/` and links the `xui_backend_glfw` and `xui_backend_opengl` targets declared by [`backends/CMakeLists.txt`](../backends/CMakeLists.txt). These targets are built only when `XUI_BUILD_GLFW_EXAMPLE` is enabled; they are not linked into the core framework targets.

The interactive `showcase.xui`/`.xss` pair stays in `examples/` because the GLFW demo and runtime asset tests both use it. Focused `cross_platform.*` and `controls.*` fixtures live under `tests/fixtures/`; tests load them from `XUI_TEST_FIXTURE_DIR`, and user guides link those same files as syntax examples. Other test-authored UI snippets remain inline in their owning tests.

## Implement `xui::Platform`

See [`platform.hpp`](../include/xui/platform/platform.hpp). Implement:

- `init` / `shutdown` and `createWindow` / `destroyWindow`.
- Title, logical client size, physical framebuffer size, DPI, and opaque native handles.
- `windowVisible()` for hidden/minimized timer policy. It defaults to `true`; implement it when the backend can query visibility so the facade can pause animation clocks while hidden.
- A normalized event queue through `pumpEvents()` and a blocking/non-busy `waitEvents(timeoutSec)`.
- Monotonic `time()`, cursor and clipboard methods.
- GL context activation (`makeGlCurrent`, `nativeGlContext`) and `swapBuffers`; use safe no-ops when your platform/renderer pair does not need them.
- `requestRedraw()` as a coalesced normalized tick.

The interface includes platform/context escape hatches to support backend setup. Keep their use inside concrete backend code; shared app code and `.xui`/`.xss` should not inspect OS-specific handles.

## Lifecycle through `Xui`

```cpp
xui::Xui ui;
ui.loadFont("font.ttf", "Inter", 400);
ui.loadXui("main.xui");
ui.loadXss("main.xss");
ui.attach(renderer, platform);
return ui.run();
```

The renderer/platform objects are caller-owned references and must outlive the attachment. The facade currently supports one window and one attach/run lifecycle. `XuiOptions` controls window size/title, resize constraints, visibility, GL context request, vsync, and atlas size. `shutdown()` or destruction tears down atlas, renderer, window, then platform state.

For a host-managed loop, the lower-level order is: load/validate the `Program`, compile the stylesheet, load faces, initialize `Platform` and create the window, activate context if needed, initialize `Renderer`, initialize glyph atlas, build `UiRuntime`, then repeat event dispatch → logical-size layout → paint/draw-list submission → presentation. Keep the same `.xui`/`.xss` assets when switching backends.

## Current runtime scope

The parser/schema has more widget names and language syntax than `UiRuntime` supports. See [UI object reference](widgets.md) for implemented controls and the schema-only list. Dynamic expression evaluation, `if`/`for`, component expansion, and property-filter selector lookup are not implemented by the standard runtime. The standard runtime supports declarative `.xss` paint transitions and keyframe animations, plus `Xui::playAnimation`/`cancelAnimation` for authored paused or restarted keyframes; layout geometry and font selection do not animate. Active animations continue while the window is visible even if it is unfocused. Named `.xss` palette themes can be selected with `Xui::setTheme("dark")` after loading the stylesheet (or before attach); theme-dependent layout geometry such as padding and gap still needs full reapplication support, so keep structural dimensions in `.xui` until that is resolved. `MenuBar` currently builds a client-area horizontal container; native decoration changes and dedicated Menu/MenuItem runtime behavior are outside the current platform contract.

This guide is the current interface reference. Current readiness and historical implementation notes are recorded in [`plan/plan.md`](../plan/plan.md) and [`plan/tracking.md`](../plan/tracking.md).
