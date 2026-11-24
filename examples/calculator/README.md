# Windows Calculator demo

A runnable Xui calculator with a dark Windows 11 inspired surface. The grid, custom title bar, mode drawer, history/memory panel, and visual states are authored in [calculator.xui](calculator.xui) and [calculator.xss](calculator.xss); the demo SVG icons live in [assets](assets/). [calculator.cpp](calculator.cpp) implements arithmetic, memory, history, and keyboard input through the Xui event APIs. On Windows the custom title bar replaces the native title bar; platforms that cannot host undecorated windows retain native chrome and render the authored bar as documented by the framework.

## Build and run

Configure with the GLFW example enabled, then build:

```powershell
cmake -S . -B build -DXUI_BUILD_EXAMPLES=ON -DXUI_BUILD_GLFW_EXAMPLE=ON
cmake --build build --target xui_calculator_demo --config Release
```

Run `build/examples/calculator/Release/xui_calculator_demo.exe` for a Visual Studio multi-config build, or `build/examples/calculator/xui_calculator_demo.exe` for a single-config generator. The demo resolves its UI, font, and SVG files beside the executable, regardless of the launch working directory.

## Use

Click the number/operator keys or use the keyboard: digits, `+`, `-`, `*`, `/`, decimal point, Enter, Backspace, Delete, Escape, and C. Memory keys support clear, recall, add, subtract, and store. The calculator supports chaining, percentage, reciprocal, square, square root, sign toggle, and divide-by-zero reporting.

The Windows Release build and headless asset parse/layout regression pass. A Windows visual smoke test confirmed the layout and the `7 + 3 = 10` button path. The native title bar and glyph icons differ from the reference image, and other platforms were not visually checked.
