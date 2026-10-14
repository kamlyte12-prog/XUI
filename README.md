# Xui

Xui is an experimental C++23 GUI framework. `.xui` describes the widget tree and layout intent, `.xss` describes visual rules, and the `xui::Xui` facade connects them to replaceable `Platform` / `Renderer` backends. `UiRuntime` remains available as the lower-level API.

## Quick start: Windows host

Requirements: CMake 3.25+, a C++23 compiler, and OpenGL 2.1 support. The Windows hello target uses the same GLFW `Platform` and direct OpenGL `Renderer` as the cross-platform example:

```powershell
cmake -S . -B build/hello -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/hello --config Release --target xui_hello
.\build\hello\examples\Release\xui_hello.exe
```

The host loads `showcase.xui` and `showcase.xss` beside the executable. The example layout and style are backend-neutral; GLFW contains the window and context calls. The OpenGL renderer consumes the backend-neutral `DrawList` directly. Text is rasterized into a TrueType glyph atlas at the current DPI.

## Cross-platform GLFW example

The demo builds on Windows, Linux and macOS using the bundled GLFW 3.5.1 source and the system OpenGL development package. It uses the same `Platform`, `Renderer`, `.xui`, and `.xss` boundaries as the rest of Xui:

```sh
cmake -S . -B build/glfw -DXUI_BUILD_GLFW_EXAMPLE=ON -DXUI_BUILD_TESTS=OFF -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/glfw --target xui_glfw_demo
```

Run `xui_glfw_demo` from any working directory; it loads the copied `showcase.xui` and `showcase.xss` beside its executable. The demo tries common system font locations; set `XUI_DEMO_FONT` to a `.ttf` path if it cannot find one. Its Input, Controls, and Layout tabs demonstrate the runtime-supported widgets, including editable fields, checkbox, tabs, delayed tooltips, dropdown, color picker, checklist, resizable panels, slider, progress bar, and FPS counter. GLFW-specific calls stay in the platform adapter; UI input and layout use Xui interfaces.

Both sample executables render SDF shapes and glyphs directly from `DrawList` buffers with the shared OpenGL 2.1 renderer. The CPU `SoftRenderer` remains available as a deterministic reference and headless backend. `xui_hello.exe --profile` and `xui_glfw_demo.exe --profile` run a hidden bounded application-loop profile and print frame count and effective FPS.

Press **F3** in either example to toggle the `.xss`-styled FPS counter. The showcase displays it initially and keeps it visible on first launch. The progress bar is display-only; the framework does not yet have data binding between widgets. `--profile` runs a hidden, continuously redrawn five-second loop; it measures loop throughput, not visible presentation or input latency.

## Authoring

```xui
Window(title: "Demo", size: 800x500) {
  Split(direction: Horizontal)[ratio: 0.4, gutter: 8, grow: 1] {
    Panel[class: "sidebar", pad: 16px] {
      CheckList {
        CheckItem(label: "First task", checked: true)
        CheckItem(label: "Second task")
      }
    }
    Panel[class: "content", pad: 16px] { Text("Drag the divider") }
  }
}
```

```xss
Panel.sidebar, Panel.content { resting: { bg: #172033, border: 1px solid #344563, radius: 10px } }
CheckItem { resting: { fg: #f5f7ff, font-size: 15px }
  hover: { bg: #263752 }
  focus: { border: 1px solid #5b8cff }
  checked: { fg: #82aaff }
  disabled: { opacity: 0.45 }
}
```

Layout values such as `align`, `size`, `pad`, `margin`, `gap`, `grow`, and `justify` feed the retained layout engine. `.xss` styles paint: background/foreground, borders, radius, shadow, opacity, font family/size/weight, letter spacing, text alignment and interaction states. Load a font with `Xui::loadFont(path, family, weight)` before attaching backends, then use `font: "Family"` in `.xss`. Low-level users can register faces through `UiRuntime::registerFont()`. Font selection falls back to the host's default face when no matching family is registered. `active` is accepted as an alias for `pressed`.

For a basic application, use the `xui::Xui` facade below. It reads the files, loads fonts, parses and validates `.xui`, compiles `.xss`, builds the runtime, creates the window through the supplied platform, and owns the ordinary event/layout/paint loop. The lower-level `loadUiAssets`, `UiRuntime`, `LayoutTree`, `Painter`, and text APIs remain available for custom lifecycle or embedding needs.

The runtime supports `Window`, `Column`, `Row`, `Panel`, `Split`, `CheckList`, `CheckItem`, `CheckBox`, `TextBox`, `TextArea`, `Tabs` / `Tab`, `Text`, `Button`, `Slider`, `ProgressBar`, `Dropdown`, `ColorPicker`, `FpsCounter`, and tooltips via the `tooltip` property. Stateful controls can be accessed by authored id using `ui.runtime().value(id)` and `setValue(id, value)`. Slider changes are reported as the logical value moves; the color picker accepts typed HEX/RGB(A)/HSL(A)/HSV(A) input while retaining RGBA `Color` as its canonical value. See the [widget guide](doc/widgets.md) for supported options and limitations. Parent text properties (`font`, `font-size`, `fg`, `align`, and `letter-spacing`) cascade as defaults; explicit child rules override them. Wrapped text advances line baselines, contributes measured height when auto-sized, and clips to its widget. The runtime does not yet evaluate bindings, conditions, loops, component expansion, or transitions. Unsupported nodes return an error instead of silently becoming native controls.

## Start a project with a custom backend

The default path is one `Xui` object and two backend objects. Replace the example backend headers and types with your own implementations of `xui::Renderer` and `xui::Platform`:

```cpp
#include <xui/xui.hpp>
#include "my_renderer.hpp"  // implements xui::Renderer
#include "my_platform.hpp"  // implements xui::Platform
#include <iostream>

int main() {
  MyRenderer renderer;
  MyPlatform platform;
  xui::Xui ui;
  if (!ui.loadFont("font.ttf", "Inter", 400) ||
      !ui.loadXui("app.xui") ||
      !ui.loadXss("app.xss")) {
    std::cerr << ui.lastError() << '\n';
    return 1;
  }

  if (!ui.attach(renderer, platform)) {
    std::cerr << ui.lastError() << '\n';
    return 1;
  }
  return ui.run();
}
```

The entry point can be reduced further when the app does not need custom settings or callbacks:

```cpp
int main() {
  MyRenderer renderer;
  MyPlatform platform;
  xui::Xui ui;
  if (!ui.loadFont("font.ttf") || !ui.loadXui("app.xui") || !ui.loadXss("app.xss")) return 1;
  if (!ui.attach(renderer, platform)) return 1;
  return ui.run();
}
```

### Before and after

| Previous application path | Facade path |
| --- | --- |
| Own `StringInterner`, `UiAssetBundle`, and error handling | `Xui` owns asset parsing and framework state |
| Read files, call `loadUiAssets`, load/register fonts | `loadFont`, `loadXui`, and `loadXss` |
| Build `UiRuntime`, initialize atlas, construct `Painter` | `attach(renderer, platform)` |
| Implement event draining, layout, paint, submit, present, and resize loop | `run()` |

The backend contracts are unchanged. `Platform` owns the native window and translates input into normalized events with top-left-origin logical coordinates; it reports logical client size, physical framebuffer size, and DPI, and waits instead of busy-polling when idle. `Renderer` owns the drawing device/surface, texture lifecycle, and execution of the backend-neutral `DrawList`; it implements frame begin/submit/end/present and reports truthful capabilities. Neither interface parses `.xui` or `.xss`. `Xui` accepts both as references, so applications can still provide custom implementations without making OS calls in shared application code.

`XuiOptions` customizes app/window names, initial and minimum size, resizability, visibility, OpenGL-context request, vsync, and glyph atlas size. Set `glContext = false` for a non-GL backend. `loadFont(path, family, weight)` loads a TrueType face before attach; `.xss` can then select the family and weight. `Xui::runtime()` and `Xui::textSystem()` expose lower-level operations for advanced applications. The facade is a one-window, one-run convenience lifecycle; use the lower-level APIs for multiple windows or a host-owned event loop. See [`architecture.md`](architecture.md) for the interface contracts and data flow.

## Build and checks

```sh
cmake -S . -B build/debug -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=ON
cmake --build build/debug --config Debug
ctest --test-dir build/debug -C Debug --output-on-failure
```

See [tracking.md](tracking.md) for audit findings, design choices, status and next steps.

## Documentation

The [`doc/`](doc/README.md) folder contains focused guides for getting started, `.xui` structure, `.xss` styling, supported UI objects and style properties, backend architecture, performance, and troubleshooting. Start with the [documentation index](doc/README.md); advanced renderer/platform authors can go directly to [Architecture and backend development](doc/architecture.md). The root [architecture.md](architecture.md) remains available as the project's architecture notes and measurement history.
