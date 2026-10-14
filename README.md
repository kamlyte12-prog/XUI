# Xui

> An experimental C++23 GUI framework: describe your UI in `.xui`, style it in `.xss`, and plug in any rendering or windowing backend.

<!-- 📍 Badge suggestions (add only the ones that apply):
[![Build](https://img.shields.io/badge/build-TODO-lightgrey)](TODO)
[![Version](https://img.shields.io/badge/version-TODO-lightgrey)](TODO)
[![License](https://img.shields.io/badge/license-TODO-lightgrey)](TODO)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)](TODO)
-->


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
- **Supported widgets:** `Window`, `Column`, `Row`, `Panel`, `Split`, `CheckList`, `CheckItem`, `CheckBox`, `TextBox`, `TextArea`, `Tabs` / `Tab`, `Text`, `Button`, `Slider`, `ProgressBar`, `Dropdown`, `ColorPicker`, `FpsCounter`, plus tooltips via the `tooltip` property.
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
- Unsupported nodes return an error instead of silently becoming native controls.
- The `Xui` facade is a one-window, one-run convenience lifecycle. For multiple windows or a host-owned event loop, use the lower-level APIs.

---

## 🖼️ Demo

![Screenshot](doc/showcase.png)

<!-- 📍 Suggested visuals:
  1. Screenshot of the showcase window, one per tab.
  2. GIF of dragging a Split divider and using the dropdown/color picker.
  3. Diagram: .xui + .xss -> Xui -> DrawList -> Renderer, with Platform feeding events in. -->

The `showcase.xui` / `showcase.xss` example demonstrates editable fields, checkbox, tabs, delayed tooltips, dropdown, color picker, checklist, resizable panels, slider, progress bar, and an FPS counter.

Press **F3** in either example to toggle the `.xss`-styled FPS counter. It is visible on first launch.

---

## 📋 Prerequisites

- CMake **3.25+**
- A **C++23** compiler
- **OpenGL 2.1** support
- Linux/macOS: the system OpenGL development package (the GLFW source is bundled)

---

## 📦 Installation

Xui is built from source with CMake. [TODO: Document how to consume Xui from another project (e.g., `add_subdirectory`, `find_package`, or package manager), since the existing README does not specify this.]

```sh
git clone https://github.com/kamlyte12-prog/XUI.git
cd XUI
```

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

Run `xui_glfw_demo` from any working directory. It loads the copied `showcase.xui` and `showcase.xss` beside its executable. [TODO: Add the exact output path of `xui_glfw_demo` per platform.]

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

Neither interface parses `.xui` or `.xss`. `Xui` accepts both as references, so applications can provide custom implementations without OS calls in shared application code. See [`architecture.md`](architecture.md) for interface contracts and data flow.

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

`XuiOptions` customizes the following. [TODO: List exact field names and defaults from the header.]

| Option | Purpose | Default |
| --- | --- | --- |
| App/window names | Application and window naming | xui-app |
| Initial and minimum size | Starting and minimum window size | 720x440 |
| Resizability | Whether the window can be resized | true |
| Visibility | Whether the window is shown | true |
| OpenGL-context request (`glContext`) | Request a GL context; set `glContext = false` for a non-GL backend | true |
| Vsync | Vertical sync | false |
| Glyph atlas size | Size of the glyph texture atlas | GlyphAtlas::kMinSize |

### CMake options

| Option | Purpose | Default |
| --- | --- | --- |
| `XUI_BUILD_TESTS` | Build tests | ON |
| `XUI_BUILD_BENCHMARKS` | Build benchmarks | ON |
| `XUI_BUILD_GLFW_EXAMPLE` | Build the cross-platform GLFW demo (`xui_glfw_demo`) | ON |

### Environment variables and runtime flags

| Name | Applies to | Purpose | Default |
| --- | --- | --- | --- |
| `--profile` | `xui_hello`, `xui_glfw_demo` | Run a hidden, bounded, continuously redrawn five-second profile loop | ON |
| `F3` (key) | Both demos | Toggle the FPS counter | Visible on first launch |

---

## 🗂️ Project structure

> [TODO: Verify and complete this tree against the repository; only the items below are mentioned in the existing README.]

```text
.
├── doc/             # Focused guides (see doc/README.md)
│   ├── README.md    # Documentation index
│   ├── widgets.md   # Supported widgets, options, limitations
│   └── architecture.md  # Architecture and backend development
├── examples/        # xui_hello and xui_glfw_demo; showcase.xui / showcase.xss
├── architecture.md  # Project architecture notes and measurement history
├── tracking.md      # Audit findings, design choices, status, next steps
└── CMakeLists.txt   # [TODO: confirm]
```

---

## 🧪 Build and test

```sh
cmake -S . -B build/debug -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=ON
cmake --build build/debug --config Debug
ctest --test-dir build/debug -C Debug --output-on-failure
```

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

Start with the [documentation index](doc/README.md). Renderer/platform authors can go directly to [Architecture and backend development](doc/architecture.md). The root [`architecture.md`](architecture.md) remains available as the project's architecture notes and measurement history.

---

## 🤝 Contributing

[TODO: Add contribution guidelines (issue reporting, pull request process, code style).]

Before submitting changes, please run the [build and test](#-build-and-test) commands above.

---

## 📄 License

[TODO: Add license name and link to the `LICENSE` file.]

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
See the troubleshooting guide in [`doc/`](doc/README.md). [TODO: Link the exact troubleshooting page.]