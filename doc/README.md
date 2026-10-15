# Xui documentation

Xui keeps authored structure, visual rules, and platform rendering separate:

- `.xui` files describe the widget tree and layout intent.
- `.xss` files describe paint, text, and interaction-state styles.
- `xui::Xui` is the recommended application entry point.
- `xui::Renderer` and `xui::Platform` are the backend contracts.

## Guides

1. [Getting started](getting-started.md) — build, link, load assets/fonts, and write a minimal app.
2. [`.xui` language guide](xui-guide.md) — syntax, supported structure/layout, imports, and examples.
3. [`.xss` language guide](xss-guide.md) — selector matching, tokens, state blocks, and style resolution.
4. [UI object reference](widgets.md) — every object implemented by the current runtime, its behavior, and limitations.
5. [Styling reference](styling-reference.md) — every accepted `.xss` property, value shape, units, and runtime support status.
6. [Architecture and backends](architecture.md) — data flow and how to implement a renderer/platform.
7. [Performance guide](performance.md) — redraw/layout guidance and measured performance context.
8. [Troubleshooting](troubleshooting.md) — diagnosing text, layout, rendering, and responsiveness issues.

[`tracking.md`](../tracking.md) records implementation notes, design choices, and status. This documentation guide explains the current public API and runtime behavior; implementation limits are called out rather than implied by parser acceptance.

## Current scope

The parsers recognize more syntax and widget names than the first retained runtime implements. For a widget to display and interact, it must be listed as implemented in the [UI object reference](widgets.md). Parser/schema acceptance alone does not mean that a runtime renderer exists for a widget.

The bundled examples are the practical reference implementation:

- [`cross_platform.xui`](../examples/cross_platform.xui)
- [`cross_platform.xss`](../examples/cross_platform.xss)
- [`showcase.xui`](../examples/showcase.xui) and [`showcase.xss`](../examples/showcase.xss), the default tabbed GLFW demo with the complete runtime-supported widget set
- [`controls.xui`](../examples/controls.xui) and [`controls.xss`](../examples/controls.xss), focused earlier example assets retained for tests and reference
- [`glfw_demo.cpp`](../examples/glfw_demo.cpp)
- [`glfw_platform.hpp`](../examples/glfw_platform.hpp)
- [`gl_drawlist_renderer.hpp`](../examples/gl_drawlist_renderer.hpp)

The showcase's **Scrolling** tab demonstrates the shared `ScrollView` runtime: nested vertical wheel routing, a horizontal row, smooth scrolling, scrollbar drag, a hidden scrollbar, programmatic scroll-into-view, and a live `Vec2` offset readout. See the [ScrollView reference](widgets.md#scrollview) for supported properties and limitations.
