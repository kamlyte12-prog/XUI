# Implementation conventions

This document captures repository-specific practices for agents. For the full contributor workflow and current build commands, also read [`../CONTRIBUTING.md`](../CONTRIBUTING.md). For public file-format behavior, use the guides under [`../docs/`](../docs/README.md) as the source of truth.

## C++ style and structure

- Use C++23, `namespace xui`, four-space indentation, and same-line opening braces, matching the existing sources. Use project aliases (`u32`, `f64`, etc.) and `std::string_view` for non-owning string inputs where appropriate.
- Public APIs belong under `include/xui/`; implementations belong in the corresponding `src/` module. Keep parsing/style compilation in `xui_assets`, layout in `xui_layout`, platform/render boundaries in their modules, and retained widget behavior in `src/ui/`.
- Follow neighboring code for includes, naming, error handling, and ownership. Report failures through existing return/diagnostic/`lastError()` paths; do not silently ignore malformed input or backend failures.
- Keep shared runtime code free of OS and graphics-API calls. Put such code in a concrete backend adapter such as the GLFW platform or GL renderer.

## `.xui` and `.xss` authoring

- Use stable, descriptive authored IDs for stateful controls and callbacks. Keep widget structure, text/content, layout relationships, and behavior attributes in `.xui`.
- Put colors, typography, borders, spacing, and visual state rules in `.xss`. Prefer shared selectors/tokens and consistent state blocks over one-off inline/demo colors.
- Keep examples small enough to teach one pattern, with clear IDs and comments only where they explain behavior. The complete interactive reference is `examples/showcase.xui` plus `examples/showcase.xss`.
- Check the current guides before extending syntax: [`xui-guide.md`](../docs/xui-guide.md), [`xss-guide.md`](../docs/xss-guide.md), and [`styling-reference.md`](../docs/styling-reference.md). Parser acceptance alone does not mean the runtime implements an object or property.

## Adding a widget end to end

Follow the existing runtime path and contribution checklist in [`../CONTRIBUTING.md`](../CONTRIBUTING.md). In practical terms:

1. Define or update the schema/parser contract only when the authored syntax needs it; validate attributes and surface source-located diagnostics.
2. Add runtime construction, intrinsic measurement/layout behavior, paint, hit testing, pointer/key/text events, focus/disabled/read-only behavior, and teardown as applicable. Do not put widget policy in the backend.
3. Resolve its appearance through the `.xss` style system, including applicable normal/hover/focus/active/checked/disabled states. Avoid fixed colors or sizes when these are meant to be theme-controlled.
4. For stateful controls, implement typed `value(id)` / `setValue(id, value)` behavior and change notifications consistently with existing controls.
5. Add focused parser/layout/runtime/render tests for behavior, including interaction and invalidation cases. Add it to the showcase only after the runtime supports it.
6. Update the widget guide, styling reference if needed, architecture docs if a boundary changes, roadmap status, and tracking history.
7. Verify the common anti-regression cases in [`agents.md`](agents.md); test the GLFW/OpenGL example and any other affected backend where available.

## Adding a style property

1. Trace an existing property end to end through style declarations/types, parser/compiler validation, cascade/merge/resolution, runtime consumption, paint/layout, and docs/tests. A property is not supported merely because its name parses.
2. Define accepted values, units, defaults, inheritance/cascade behavior, and precedence against `.xui` layout/behavior attributes. Keep visual styling in `.xss`; use `.xui` for authored structure and layout intent unless the documented property is explicitly shared.
3. Add valid/invalid parser tests, cascade/state tests, and a runtime or rendering test proving the property changes output. Avoid per-frame allocations or repeated style compilation/resolution in hot paths.
4. Update [`../docs/styling-reference.md`](../docs/styling-reference.md), the relevant guide, and canonical [`plan.md`](plan.md) / [`tracking.md`](tracking.md) where status or decisions change.

## Testing and verification

- Add tests for bug fixes and new behavior; run the focused target first, then `xui_tests`/`ctest` as appropriate. Run compiler warnings as errors using the repository's configured build.
- For layout, text, styling, rendering, and events, test bounds, clipping, invalidation, and state transitions—not just successful parsing.
- For performance-sensitive changes, use the relevant benchmarks and compare before/after. A `--profile` loop reports loop throughput; it does not prove visible presentation cadence, smooth resize, or input latency.
- When GUI access exists, visibly verify text glyphs/wrapping, hover/focus/click/typing, resize while active, and normal close. Record backend, environment, test method, and results. If GUI access is unavailable, state that clearly and do not call the visual behavior verified.
- For every change, follow the documentation and status update rules in [`agents.md`](agents.md).
