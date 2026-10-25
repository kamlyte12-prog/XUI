# Component example

`main.xui` imports `card.xui` and uses the declared `Card` composition. The title and subtitle are literal parameters. The calling document supplies the default body and a named `footer` slot. Internal IDs are scoped below the call ID, so `welcome.title` and `welcome.surface` are addressable; the caller-owned `body-copy` ID stays unchanged.

The component marks its surface and title as public style parts. `main.xss` styles them with `Card::surface` and `Card::title`, while layout and content remain in the `.xui` files.

Build the runnable GLFW/OpenGL example with `cmake --build build/glfw --target xui_component_demo`, then launch `xui_component_demo` from the build output directory. CMake copies `main.xui`, its imported `card.xui`, and `main.xss` beside the executable. The Continue button demonstrates handler forwarding by printing `onContinue` to the console.
