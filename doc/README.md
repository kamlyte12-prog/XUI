# Xui documentation index

Use the focused guide for the topic you need. `.xui` defines structure and layout; `.xss` defines visual rules; `xui::Renderer` and `xui::Platform` keep rendering and window/input backends replaceable.

## Project documentation

| File | Purpose |
|---|---|
| [`README.md`](README.md) | This index and current documentation scope. |
| [`getting-started.md`](getting-started.md) | Prerequisites, build/link/setup, minimal app, assets/fonts, configuration, and custom backends. |
| [`xui-guide.md`](xui-guide.md) | `.xui` syntax, layout, sizing, structure, events, imports, and runtime support. |
| [`xss-guide.md`](xss-guide.md) | `.xss` rule syntax, selectors, cascade, themes, state styles, and layout/style boundaries. |
| [`widgets.md`](widgets.md) | Runtime widget behavior, authored properties, values/events, and unsupported objects. |
| [`docking.md`](docking.md) | DockHost/DockPanel authoring, recursive splits, tabs, floating overlays, events, values, and styles. |
| [`styling-reference.md`](styling-reference.md) | Accepted `.xss` properties, value shapes, units, states, defaults, and limits. |
| [`animations-fonts-transforms.md`](animations-fonts-transforms.md) | Animation control, transform behavior, font-format support, and cursor fallback matrix. |
| [`architecture.md`](architecture.md) | Framework data flow, interface contracts, lifecycle, and how to add a backend. |
| [`performance.md`](performance.md) | Performance measurements, optimization guidance, and resize/redraw behavior. |
| [`troubleshooting.md`](troubleshooting.md) | Diagnosing setup, text, styling, layout, popup, and responsiveness problems. |

The root [`README.md`](../README.md) is a short project entry point. Agent rules and roadmap/history are indexed separately in [`plan/README.md`](../plan/README.md). The canonical change history is [`plan/tracking.md`](../plan/tracking.md).

## Examples and implementation scope

The [showcase](../examples/showcase.xui) and [stylesheet](../examples/showcase.xss) are the default interactive reference. The event-driven [game UI example](../examples/game/README.md) demonstrates a turn-based encounter without a separate game renderer. Focused [cross-platform](../tests/fixtures/cross_platform.xui) and [controls](../tests/fixtures/controls.xui) fixtures are shared by tests and these guides. GLFW backend implementations live in [`backends/glfw`](../backends/glfw/glfw_platform.hpp) and [`backends/opengl`](../backends/opengl/gl_drawlist_renderer.hpp); the showcase application remains in [`examples/glfw_demo.cpp`](../examples/glfw_demo.cpp).

Parser/schema acceptance does not guarantee runtime support. The [widget reference](widgets.md) labels objects that are not materialized by `UiRuntime`; only runtime-backed objects should be treated as interactive controls.
