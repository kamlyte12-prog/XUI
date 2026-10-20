# Getting started

This guide creates a small app with one `.xui` layout and one `.xss` stylesheet. The app code chooses concrete backend implementations, while the shared UI stays independent of GLFW, Win32, or OpenGL.

## Build Xui

Xui uses CMake 3.25+ and a C++23 compiler. The bundled GLFW example requires OpenGL 2.1 support. On Linux, install OpenGL, X11, and Wayland development packages (for Ubuntu: `libgl-dev xorg-dev libwayland-dev libxkbcommon-dev`). The GLFW source is bundled; macOS uses its system Cocoa/OpenGL frameworks.

Configure, build, and test from a checkout:

```sh
cmake -S . -B build -DXUI_BUILD_EXAMPLES=ON -DXUI_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For GCC/Clang sanitizer checks, configure another build tree with `-DXUI_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug`, then build and run `ctest` there.

Xui builds from source and does not currently export an installed CMake package. To consume it from another CMake project in the same build, add the source tree and link the facade target:

```cmake
add_subdirectory(path/to/xui)
target_link_libraries(my_app PRIVATE xui_widgets)
```

`xui_widgets` publicly links the core, asset parser, layout, rendering, and text libraries used by the facade.

The cross-platform demo target is `xui_glfw_demo`. It uses the bundled GLFW source and requires system OpenGL development files. The GLFW platform adapter and OpenGL renderer are separate optional targets in `backends/glfw/` and `backends/opengl/`; the demo links both without adding either implementation to the core libraries. The default launch loads `showcase.xui` and `showcase.xss`. Press F3 to toggle the FPS counter.

To build the GLFW demo explicitly:

```sh
cmake -S . -B build/glfw -DXUI_BUILD_GLFW_EXAMPLE=ON -DXUI_BUILD_TESTS=OFF -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/glfw --target xui_glfw_demo
```

The executable loads the copied showcase pair beside itself, independent of the working directory. Set `XUI_DEMO_FONT` to a TrueType font path if the demo's font search paths do not find one. A screenshot and the full example assets are linked from the [documentation index](README.md#examples-and-implementation-scope).

## Minimal application

Implement `xui::Renderer` and `xui::Platform` in your backend files, then the application startup stays small:

```cpp
#include <xui/xui.hpp>
#include "my_renderer.hpp"  // MyRenderer implements xui::Renderer
#include "my_platform.hpp"  // MyPlatform implements xui::Platform
#include <iostream>

int main() {
    MyRenderer renderer;
    MyPlatform platform;
    xui::Xui ui;

    if (!ui.loadFont("assets/Inter-Regular.ttf", "Inter", 400) ||
        !ui.loadXui("ui/main.xui") ||
        !ui.loadXss("ui/main.xss")) {
        std::cerr << ui.lastError() << '\n';
        return 1;
    }
    // Optional: activate an @theme declared in main.xss.
    // if (!ui.setTheme("dark")) { std::cerr << ui.lastError() << '\n'; return 1; }
    if (!ui.attach(renderer, platform)) {
        std::cerr << ui.lastError() << '\n';
        return 1;
    }
    return ui.run();
}
```

The file paths are interpreted by the process working directory. Bundle or locate the font and UI assets as part of your app's packaging. Call all three load methods before `attach()`. The first loaded font is the default; `.xss` can select a registered family and weight using `font` and `font-weight`.

## Fonts

The built-in loader supports TTF and OTF containers that use TrueType `glyf` outlines. Load each family/weight before `attach()`; multiple loaded files are registered, and a requested missing weight uses the nearest registered face. The framework does not synthesize bold or select system fallback fonts. CFF/CFF2-only OTF outlines, variable font deltas, emoji color tables, complex script shaping, ligatures, and bidi layout are not supported. See [font, transform, cursor, and animation details](animations-fonts-transforms.md).

To customize the window, construct `Xui` with `XuiOptions`:

```cpp
xui::XuiOptions options;
options.appName = "inventory-tool";
options.windowTitle = "Inventory";
options.width = 1100;
options.height = 720;
options.minWidth = 600;
options.minHeight = 400;
options.resizable = true;
options.glContext = true; // set false for a backend that does not use OpenGL
options.vsync = true;

xui::Xui ui(options);
```

The renderer and platform objects stay application-owned and must outlive the attached `Xui` object. The facade shuts down their initialized resources when `shutdown()` is called or when `Xui` is destroyed. The facade currently manages one window and one run lifecycle.

## First `.xui` and `.xss` files

`ui/main.xui`:

```xui
Window(title: "Starter", size: 760x480) {
  Column[class: "page"] {
    Text("Welcome to Xui")[class: "title"]
    Text("Layout is authored here; colors and typography live in the stylesheet.")[class: "body"]
    Button(label: "Continue", press: onContinue)[class: "primary", align: Center]
  }
}
```

`ui/main.xss`:

```xss
tokens {
  color.canvas = #111827
  color.text = #f9fafb
  color.muted = #cbd5e1
  color.primary = #2563eb
  color.primary.hover = #3b82f6
}

Window { resting: { bg: color.canvas } }
Column.page { resting: { pad: 28px, gap: 14px } }
Text.title { resting: { fg: color.text, font-size: 26px, font-weight: 700 } }
Text.body { resting: { fg: color.muted, font-size: 15px } }
Button.primary {
  resting: { bg: color.primary, fg: color.text, size: 220px 44px, radius: 8px }
  hover: { bg: color.primary.hover }
  pressed: { opacity: 0.85 }
  disabled: { opacity: 0.45 }
}
```

The event callback can be installed with `ui.setEventHandler(handler, context)` before attaching. Runtime event handlers receive the authored handler name (here, `onContinue`); the application decides what that name means.

## Implementing your own backend

The public interfaces are declared in:

- [`renderer.hpp`](../include/xui/render/renderer.hpp) (`xui::Renderer`)
- [`platform.hpp`](../include/xui/platform/platform.hpp) (`xui::Platform`)

`Renderer` owns the graphics resources, texture storage, and execution of the backend-neutral `DrawList`. `Platform` owns native windows, normalized events, dimensions, DPI, waiting, and context/presentation hooks. Implement every pure virtual method and keep platform calls inside these implementations. The shared app then attaches them through `Xui`.

For a non-OpenGL renderer, implement `makeGlCurrent`, `nativeGlContext`, and `swapBuffers` as suitable no-ops and set `XuiOptions::glContext` to `false`. The facade calls `makeGlCurrent()` before renderer initialization and calls `swapBuffers()` after renderer presentation. A renderer that presents through `present()` may leave platform swapping as a no-op.

Read [Architecture and backends](architecture.md) for the complete method contract and lifecycle, and [Troubleshooting](troubleshooting.md) when the first window does not appear or render as expected.

## Advanced / lower-level path

Use `UiRuntime`, `TextSystem`, `Painter`, and `loadUiAssets()` directly when embedding into an existing event loop, managing multiple windows, or customizing frame scheduling. The low-level path makes the host responsible for file reads, parsing, font-atlas setup, input dispatch, logical-size layout, draw-list submission, and cleanup. It does not change the `.xui` / `.xss` separation.

Compared with the lower-level setup, the facade replaces caller-owned `StringInterner`/asset-bundle/error plumbing and manual parser/style compilation with `loadFont()`, `loadXui()`, and `loadXss()`; `attach()` builds and wires the runtime; and `run()` owns the standard event/layout/paint/presentation loop. `runtime()` and `textSystem()` remain available for advanced access.

## Configuration

Pass `XuiOptions` to the `Xui` constructor to configure the app/window and glyph atlas:

| Field | Purpose | Default |
|---|---|---|
| `appName` | Application name | `"xui-app"` |
| `windowTitle` | Native window title | `"Xui"` |
| `width`, `height` | Initial client size | 720 × 440 |
| `minWidth`, `minHeight` | Minimum client size | 128 × 96 |
| `resizable`, `visible` | Window behavior | `true`, `true` |
| `glContext` | Request a GL context; set false for non-GL backends | `true` |
| `vsync` | Request vertical synchronization | `false` |
| `atlasSize` | Glyph texture atlas dimension | `GlyphAtlas::kMinSize` |

`Xui::run()` accepts `XuiRunOptions`: `maxSeconds` bounds a run when nonzero, and `redrawContinuously` opts into continuous drawing (default false). The demos use continuous mode for their hidden `--profile` run; it measures loop throughput, not visible presentation or input latency.

| CMake option | Purpose | Default |
|---|---|---|
| `XUI_BUILD_TESTS` | Build `xui_tests` | ON |
| `XUI_BUILD_BENCHMARKS` | Build benchmarks and fuzz harnesses | ON |
| `XUI_BUILD_EXAMPLES` | Build runnable examples | ON |
| `XUI_BUILD_GLFW_EXAMPLE` | Build GLFW/OpenGL demo (`xui_glfw_demo`, and `xui_hello` on Windows) | ON |
| `XUI_WERROR` | Treat warnings as errors | ON |
| `XUI_SANITIZE` | Enable ASan/UBSan on GCC/Clang | OFF |

Demo controls: `--profile` runs a hidden bounded loop; F3 toggles the visible FPS overlay; `XUI_DEMO_FONT` adds a TrueType file to the demo's font search list.
