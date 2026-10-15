# Xui

> An experimental C++23 GUI framework: describe your UI in `.xui`, style it in `.xss`, and plug in any rendering or windowing backend.

[![CI](https://github.com/kamlyte12-prog/XUI/actions/workflows/ci.yml/badge.svg)](https://github.com/kamlyte12-prog/XUI/actions/workflows/ci.yml)
![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C)

---

## 📖 Table of Contents

- [What is this?](#-what-is-this)
- [Features](#-features)
- [Demo](#-demo)
- [Prerequisites](#-prerequisites)
- [Installation](#-installation)
- [Quick start](#-quick-start)
- [Usage](#-usage)
  - [Authoring `.xui` and `.xss`](#authoring-xui-and-xss)
  - [Using Xui in your C++ app](#using-xui-in-your-c-app)
  - [Custom backends](#custom-backends)
  - [Before and after the facade](#before-and-after-the-facade)
- [Configuration](#-configuration)
- [Project structure](#-project-structure)
- [Build and test](#-build-and-test)
- [Documentation](#-documentation)
- [Contributing](#-contributing)
- [License](#-license)
- [FAQ / Troubleshooting](#-faq--troubleshooting)

---

## 💡 What is this?

Xui separates **what your UI is** from **how it looks** and **where it runs**:

| Piece | Role |
| --- | --- |
| `.xui` file | Describes the widget tree and layout intent |
| `.xss` file | Describes visual rules (colors, borders, fonts, states) |
| `xui::Xui` facade | Loads the files, builds the runtime, and runs the event/layout/paint loop |
| `Platform` backend | Owns the native window and translates input into normalized events |
| `Renderer` backend | Owns the drawing device and executes the backend-neutral `DrawList` |

> **Jargon check:** A `DrawList` is Xui's backend-neutral list of draw commands. Any `Renderer` can execute it, so shared application code never makes OS or graphics-API calls.

**Why use it?**

- One small entry point: one `Xui` object plus two backend objects.
- Layout and styling live in declarative files, not in C++ code.
- Backends are replaceable. Neither `Platform` nor `Renderer` parses `.xui` or `.xss`.
- Lower-level APIs (`UiRuntime`, `LayoutTree`, `Painter`, text APIs) stay available for custom lifecycles or embedding.

---

## ✨ Features

- **Declarative UI:** `.xui` for structure and layout, `.xss` for visuals.
- **Supported widgets:** `Window`, `Column`, `Row`, `Panel`, `Split`, `ScrollView`, `CheckList`, `CheckItem`, `CheckBox`, `Switch`, `ToggleButton`, `RadioGroup`, `TextBox`, `TextArea`, `NumberField`, `SearchField`, `Tabs` / `Tab`, `Text`, `Label`, `Badge`, `Separator` / `Divider`, `Image`, `Spinner`, `Button`, `Slider`, `ProgressBar`, `Dropdown`, `ColorPicker`, `FpsCounter`, plus tooltips via the `tooltip` property.
- **Styling:** background/foreground, borders, radius, shadow, opacity, font family/size/weight, letter spacing, text alignment, and interaction states (`resting`, `hover`, `focus`, `checked`, `disabled`, `pressed`; `active` is an alias for `pressed`).
- **Layout properties:** `align`, `size`, `pad`, `margin`, `gap`, `grow`, and `justify` feed the retained layout engine.
- **Text:** TrueType glyph atlas rasterized at the current DPI; wrapped text advances line baselines, contributes measured height when auto-sized, and clips to its widget.
- **Cascading:** parent text properties (`font`, `font-size`, `fg`, `align`, `letter-spacing`) act as defaults; explicit child rules override them.
- **Rendering:** shared OpenGL 2.1 renderer drawing SDF (signed distance field) shapes and glyphs directly from `DrawList` buffers.
- **Software reference renderer:** the CPU `SoftRenderer` is a deterministic reference and headless backend.
- **Stateful controls:** read and write values by authored id with `ui.runtime().value(id)` and `setValue(id, value)`.
- **Color picker:** accepts typed HEX/RGB(A)/HSL(A)/HSV(A) input; RGBA `Color` stays the canonical value.
- **Cross-platform demo:** Windows, Linux, and macOS via bundled GLFW 3.5.1 source.

**Current limitations**

- The runtime does not yet evaluate bindings, conditions, loops, component expansion, or transitions.
- There is no data binding between widgets yet (the progress bar is display-only).
- `Image` decodes only the project's own uncompressed PNG writer output; general image formats are not supported yet.
- Unsupported nodes return an error instead of silently becoming native controls.
- The `Xui` facade is a one-window, one-run convenience lifecycle. For multiple windows or a host-owned event loop, use the lower-level APIs.

---

## 🖼️ Demo

![Screenshot](doc/showcase.png)

The `showcase.xui` / `showcase.xss` example demonstrates every runtime-supported widget across its tabs: editable fields, checkbox, search and numeric entry, tabs, delayed tooltips, dropdown, color picker, switch, toggle button, radio group, checklist, resizable split panels, slider, progress bar, static label, badge, separators/dividers, activity spinner, a generated PNG image, toolbar/status-bar containers, and an FPS counter. Its Scrolling tab shows vertical, horizontal, nested, always-visible and hidden scrollbar cases, with wheel, drag, keyboard, scroll-into-view, and a live offset readout. Stateful controls are wired to a live readout so values can be watched through the `value()` API while interacting. Parser-only objects are listed separately in the docs and intentionally aren't shown as working controls.

Press **F3** in either example to toggle the `.xss`-styled FPS counter. It is visible on first launch.

---

## 📋 Prerequisites

- CMake **3.25+**
- A **C++23** compiler (CI builds with GCC 13, Clang 18, Apple Clang on macOS 14, and MSVC 2022)
- **OpenGL 2.1** support
- Linux: OpenGL, X11, and Wayland development packages for the bundled GLFW 3.5 (on Ubuntu: `sudo apt install libgl-dev xorg-dev libwayland-dev libxkbcommon-dev`)
- macOS: nothing extra (GLFW uses the system Cocoa and OpenGL frameworks)

---

## 📦 Installation

Xui builds from source with CMake:

```sh
git clone https://github.com/kamlyte12-prog/XUI.git
cd XUI
```

To use Xui in your own CMake project, add the checkout as a subdirectory and link the application facade target `xui_widgets` (it publicly links the core, asset parser, layout, rendering, and text libraries):

```cmake
add_subdirectory(path/to/xui)
target_link_libraries(my_app PRIVATE xui_widgets)
```

> **Note:** Xui does not yet define an install/export package target, so `find_package` and package managers are not available yet. [TODO: Update this section when install rules or a package are added.]

---

## 🚀 Quick start

### Option A: Windows host (`xui_hello`)

Uses the same GLFW `Platform` and direct OpenGL `Renderer` as the cross-platform example.

```powershell
cmake -S . -B build/hello -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/hello --config Release --target xui_hello
.\build\hello\examples\Release\xui_hello.exe
```

The host loads `showcase.xui` and `showcase.xss` from beside the executable.

### Option B: Cross-platform GLFW demo (Windows, Linux, macOS)

```sh
cmake -S . -B build/glfw -DXUI_BUILD_GLFW_EXAMPLE=ON -DXUI_BUILD_TESTS=OFF -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/glfw --target xui_glfw_demo
```

Run `xui_glfw_demo` from any working directory — it loads the copied `showcase.xui` and `showcase.xss` beside its executable. With the default CMake generator the executable lands in `build/glfw/examples/` (`build\glfw\examples\Release\` with the Visual Studio generator).

> 💡 If the demo can't find a font, set `XUI_DEMO_FONT` to a `.ttf` path.

### Profile mode

```sh
xui_hello.exe --profile
xui_glfw_demo.exe --profile
```

Runs a hidden, continuously redrawn, bounded (five-second) loop and prints frame count and effective FPS.

> **Note:** `--profile` measures loop throughput, not visible presentation or input latency.

---

## 🧰 Usage

### Authoring `.xui` and `.xss`

**`app.xui`** describes structure and layout:

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

**`app.xss`** describes visuals:

```xss
Panel.sidebar, Panel.content { resting: { bg: #172033, border: 1px solid #344563, radius: 10px } }
CheckItem { resting: { fg: #f5f7ff, font-size: 15px }
  hover: { bg: #263752 }
  focus: { border: 1px solid #5b8cff }
  checked: { fg: #82aaff }
  disabled: { opacity: 0.45 }
}
```

**Fonts:** load a font with `Xui::loadFont(path, family, weight)` before attaching backends, then use `font: "Family"` in `.xss`. Low-level users can register faces through `UiRuntime::registerFont()`. If no matching family is registered, Xui falls back to the host's default face.

See the [widget guide](doc/widgets.md) for supported options and limitations.

### Using Xui in your C++ app

The `xui::Xui` facade reads the files, loads fonts, parses and validates `.xui`, compiles `.xss`, builds the runtime, creates the window through the supplied platform, and owns the ordinary event/layout/paint loop.

Minimal entry point (no custom settings or callbacks):

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

Reading and writing stateful controls by authored id:

```cpp
ui.runtime().value(id);
ui.runtime().setValue(id, value);
```

Slider changes are reported as the logical value moves.

### Custom backends

Replace the example backend headers and types with your own implementations of `xui::Renderer` and `xui::Platform`:

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

**Backend contracts**

| Interface | Responsibilities |
| --- | --- |
| `Platform` | Owns the native window. Translates input into normalized events with top-left-origin logical coordinates. Reports logical client size, physical framebuffer size, and DPI. Waits instead of busy-polling when idle. |
| `Renderer` | Owns the drawing device/surface and texture lifecycle. Executes the backend-neutral `DrawList`. Implements frame begin/submit/end/present. Reports truthful capabilities. |

Neither interface parses `.xui` or `.xss`. `Xui` accepts both as references, so applications can provide custom implementations without OS calls in shared application code. See [Architecture and backend development](doc/architecture.md) for interface contracts and data flow.

### Before and after the facade

| Previous application path | Facade path |
| --- | --- |
| Own `StringInterner`, `UiAssetBundle`, and error handling | `Xui` owns asset parsing and framework state |
| Read files, call `loadUiAssets`, load/register fonts | `loadFont`, `loadXui`, and `loadXss` |
| Build `UiRuntime`, initialize atlas, construct `Painter` | `attach(renderer, platform)` |
| Implement event draining, layout, paint, submit, present, and resize loop | `run()` |

`Xui::runtime()` and `Xui::textSystem()` expose lower-level operations for advanced applications.

---

## ⚙️ Configuration

### `XuiOptions` (C++)

Pass `XuiOptions` to the `Xui` constructor (or `setWindowOptions()`) to customize the window and atlas:

| Field | Purpose | Default |
| --- | --- | --- |
| `appName` | Application name | `"xui-app"` |
| `windowTitle` | Window title | `"Xui"` |
| `width`, `height` | Initial window size | 720 × 440 |
| `minWidth`, `minHeight` | Minimum window size | 128 × 96 |
| `resizable` | Whether the window can be resized | `true` |
| `visible` | Whether the window is shown | `true` |
| `glContext` | Request a GL context; set `false` for a non-GL backend | `true` |
| `vsync` | Vertical sync | `false` |
| `atlasSize` | Size of the glyph texture atlas | `GlyphAtlas::kMinSize` |

`Xui::run()` optionally takes `XuiRunOptions` with `maxSeconds` (`0` runs until the platform reports close) and `redrawContinuously` (default `false`). The demos use these to implement `--profile`.

### CMake options

| Option | Purpose | Default |
| --- | --- | --- |
| `XUI_BUILD_TESTS` | Build tests (`xui_tests`) | ON |
| `XUI_BUILD_BENCHMARKS` | Build benchmarks and fuzz harnesses | ON |
| `XUI_BUILD_EXAMPLES` | Build runnable examples | ON |
| `XUI_BUILD_GLFW_EXAMPLE` | Build the cross-platform GLFW + OpenGL demo (`xui_glfw_demo`; also `xui_hello` on Windows) | ON |
| `XUI_WERROR` | Treat compiler warnings as errors | ON |
| `XUI_SANITIZE` | Enable ASan + UBSan (GCC/Clang only) | OFF |

### Environment variables and runtime flags

| Name | Applies to | Purpose | Default |
| --- | --- | --- | --- |
| `--profile` (flag) | `xui_hello`, `xui_glfw_demo` | Run a hidden, bounded, continuously redrawn five-second profile loop and print frames + FPS | Off |
| `F3` (key) | Both demos | Toggle the FPS counter | Visible on first launch |
| `XUI_DEMO_FONT` (env var) | Both demos | Adds a `.ttf` path to the demo's font search list | unset |

---

## 🗂️ Project structure

```text
.
├── .github/workflows/   # CI: Debug build + tests on GCC, Clang, MSVC
├── benchmarks/          # Performance benchmarks (bench_*) and fuzz harnesses (fuzz_*)
├── doc/                 # Focused guides (see doc/README.md for the index)
├── examples/            # xui_glfw_demo and xui_hello (Windows); showcase / controls / cross_platform assets; bundled GLFW 3.5.1
├── include/xui/         # Public headers (<xui/xui.hpp> facade plus core, lang, layout, platform, render, style, text, ui)
├── src/                 # Library sources mirroring the include/xui/ modules
├── tests/               # Unit tests and golden-image baselines (tests/golden)
├── CMakeLists.txt       # Top-level build: xui_core, xui_assets, xui_layout, xui_renderer, xui_platform, xui_text, xui_widgets
├── CONTRIBUTING.md      # Contribution guide
└── tracking.md          # Audit findings, design choices, status, next steps
```

---

## 🧪 Build and test

```sh
cmake -S . -B build/debug -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=ON
cmake --build build/debug --config Debug
ctest --test-dir build/debug -C Debug --output-on-failure
```

The suite is the `xui_tests` target: unit tests for core, layout, render, text, `.xui`/`.xss` parsing, and the runtime, plus golden-image comparisons against the baselines in [`tests/golden/`](tests/golden).

Benchmark and fuzz targets built with `XUI_BUILD_BENCHMARKS=ON`: `bench_style`, `bench_xui`, `bench_layout`, `bench_paint`, `fuzz_parsers`, `fuzz_layout`, `fuzz_paint`.

For a sanitizer build with GCC/Clang, add `-DXUI_SANITIZE=ON` (CI runs this on Ubuntu). CI builds Debug and runs `ctest` with GCC 13, GCC 13 + ASan/UBSan, Clang 18, Apple Clang, and MSVC.

---

## 📚 Documentation

The [`doc/`](doc/README.md) folder contains focused guides on:

- Getting started
- `.xui` structure
- `.xss` styling
- Supported UI objects and style properties
- Backend architecture
- Performance
- Troubleshooting

Start with the [documentation index](doc/README.md). Renderer/platform authors can go directly to [Architecture and backend development](doc/architecture.md). [`tracking.md`](tracking.md) records audit findings, design choices, and known issues.

---

## 🤝 Contributing

Before submitting changes, please run the [build and test](#-build-and-test) commands above. For further info check [`CONTRIBUTING.md`](CONTRIBUTING.md).

---

## 📄 License

This project is under the MIT license. The bundled GLFW 3.5.1 under `examples/lib/` keeps its own license; see [`examples/lib/glfw-3.5.1/LICENSE.md`](examples/lib/glfw-3.5.1/LICENSE.md).

---

## ❓ FAQ / Troubleshooting

**My `.xss` font family isn't applied.**
Load the face first with `Xui::loadFont(path, family, weight)` before attaching backends. Without a matching registered family, Xui falls back to the host's default face.

**I get an error about an unsupported node.**
Unsupported nodes return an error rather than becoming native controls. Check the supported widget list above and the [widget guide](doc/widgets.md).

**Bindings, conditions, loops, components, or transitions don't work.**
The runtime does not evaluate these yet. The progress bar is display-only, and there is no data binding between widgets.

**`--profile` shows a high FPS, but the UI feels laggy.**
Profile mode measures loop throughput, not visible presentation or input latency.

**I need a non-OpenGL backend.**
Set `glContext = false` in `XuiOptions` and provide your own `Renderer` and `Platform`. See [Custom backends](#custom-backends).

**I need multiple windows or my own event loop.**
The `Xui` facade is one-window and one-run. Use the lower-level APIs (`UiRuntime`, `LayoutTree`, `Painter`, text APIs).

**Other problems?**
See the [troubleshooting guide](doc/troubleshooting.md), or [open an issue](https://github.com/kamlyte12-prog/XUI/issues).
