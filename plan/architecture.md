# Agent architecture map

This file helps agents locate implementation layers without duplicating the public contract guide. Read [`../docs/architecture.md`](../docs/architecture.md) for the authoritative data flow, lifecycle, renderer/platform contracts, and runtime limits. Project-wide invariants are in [`agents.md`](agents.md).

## Source map

| CMake target / layer | Main implementation | Responsibility |
|---|---|---|
| `xui_widgets` — public facade | `include/xui/xui.hpp`, `include/xui/ui/xui.hpp`, `src/ui/xui.cpp` | Asset loading, backend attachment, event/layout/render loop. |
| `xui_assets` — `.xui` language | `include/xui/lang/`, `src/lang/` | Parsing, imports, schema validation, diagnostics. |
| `xui_assets` — `.xss` styling | `include/xui/style/`, `src/style/` | Style values, parser/compiler, selectors, cascade and themes. |
| `xui_layout` — layout | `include/xui/layout/`, `src/layout/` | Retained layout tree, measurements, constraints and arrangement. |
| `xui_renderer` — rendering | `include/xui/render/`, `src/render/` | Backend-neutral paint operations, draw list, painter and reference renderers. |
| `xui_platform` — platform | `include/xui/platform/`, `src/platform/` | Window/event contract and null platform. |
| `xui_text` — text | `include/xui/text/`, `src/text/` | Font loading, shaping/layout, glyph rasterization and atlas. |
| `xui_widgets` — widget runtime | `include/xui/ui/runtime.hpp`, `src/ui/runtime.cpp` | Materialized widgets, state, layout integration, events and paint generation. |
| Optional backend targets | `backends/glfw/*`, `backends/opengl/*` | GLFW window/input adapter and OpenGL draw-list renderer. They implement the public interfaces and link to framework targets; they are not part of the core libraries. |
| `xui_tests` | `tests/` | Unit, runtime, layout, render, text, parser and golden-image checks. |

## Change boundaries

- A format change usually touches parser/schema or style compilation, runtime consumption, tests, and the corresponding user guide. Parser acceptance alone is not runtime support.
- A widget change belongs primarily in the runtime and should use shared style/layout/value/event paths. Do not implement app-level widget policy in GLFW or renderer code.
- A backend change belongs behind `Renderer` or `Platform`; shared `.xui`, `.xss`, and runtime code must remain independent of OS/graphics APIs.
- A layout or paint change should preserve retained invalidation and cache behavior; add a minimized regression case before changing solver behavior.

The end-to-end widget and style-property checklists are in [`conventions.md`](conventions.md). Verification history is in [`tracking.md`](tracking.md), and current priorities are in [`plan.md`](plan.md).
