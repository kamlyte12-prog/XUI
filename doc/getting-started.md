# Getting started

This guide creates a small app with one `.xui` layout and one `.xss` stylesheet. The app code chooses concrete backend implementations, while the shared UI stays independent of GLFW, Win32, or OpenGL.

## Build Xui

Xui uses CMake 3.25+ and C++23. In a checkout, configure and build the libraries and example:

```sh
cmake -S . -B build -DXUI_BUILD_EXAMPLES=ON -DXUI_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The cross-platform demo target is `xui_glfw_demo`. It uses the bundled GLFW source and requires system OpenGL development files. On Windows the same source is also built as `xui_hello`. The default launch loads `showcase.xui` and `showcase.xss`, with Input, Controls, Layout, Widgets, and Scrolling tabs covering every currently runtime-supported widget, including toolbar/status bar container aliases and the divider alias. Press F3 to toggle the FPS counter.

For a project in the same CMake tree, add Xui as a subdirectory and link its application facade target:

```cmake
add_subdirectory(path/to/xui)
add_executable(my_app main.cpp my_platform.cpp my_renderer.cpp)
target_link_libraries(my_app PRIVATE xui_widgets)
```

`xui_widgets` publicly links the core, asset parsers, layout, rendering, and text libraries it uses. A custom platform/renderer implementation supplies its own platform and graphics dependencies. The repository currently builds from source through CMake; it does not define an installed package/export target yet.

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
