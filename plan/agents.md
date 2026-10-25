# Agent operating rules

Read this file before making changes. The repository's root [`AGENTS.md`](../AGENTS.md) points here.

## Core invariants

- `.xui` defines structure, content, and layout intent. `.xss` defines visual styling and state appearance. Keep those responsibilities separate; do not move demo layout or theme decisions into C++ as a shortcut.
- Shared application/runtime code stays backend-neutral. `Renderer` consumes the backend-neutral draw list; `Platform` owns the native window, normalized input, timing, and presentation hooks. Keep operating-system and graphics-API calls inside concrete backend implementations of those contracts.
- The same authored `.xui` and `.xss` should work across supported backends. A backend must not become the source of widget, layout, theme, or interaction semantics.
- Preserve the public facade and lower-level extension path. `xui::Xui` is the recommended entry point; custom renderers and platforms remain supported through their interfaces.
- Unsupported syntax or widgets must produce a clear diagnostic. Do not silently turn unsupported nodes into native controls or claim cross-platform support without verification.

See [`conventions.md`](conventions.md) for file-specific implementation patterns and [`architecture.md`](architecture.md) for the agent source map. The public interface contracts and data flow are in [`../docs/architecture.md`](../docs/architecture.md).

## Anti-regression checklist

The following issues have been reported or fixed during this project's development. Treat each as a regression guard when changing adjacent code; do not assume a prior fix is still present without checking current source and tests.

- Text renders as readable glyphs, not white placeholder boxes; wrapping and clipping respect widget bounds.
- Resize updates layout continuously and does not freeze the event loop. Nested widgets respond to live size changes.
- Normal flow layout does not accidentally overlap siblings or collapse content-sized containers. Intentional overlay/stack layouts remain intentional.
- Hit testing follows visible paint order; inactive or clipped content cannot steal input.
- Tooltips appear on their target with the expected delay and do not block pointer input.
- Dropdown hover, open state, selection, and popup interaction remain responsive.
- Slider values update continuously during pointer drag.
- Color picker fields and popup remain editable and report the selected color.
- TextBox and TextArea support deletion, cursor movement, and editable/read-only behavior correctly.
- Editable-state indicators, caret visibility/blinking, and TextArea tab/whitespace behavior remain correct; control characters must not be drawn as visible glyphs.
- Cursor icons match the control under the pointer and its enabled/editable state.
- Values changed in the UI are reflected through the value API and application callbacks/readouts where wired.
- Themes and `.xss` rules apply coherently to all supported widgets and relevant states.

For a change in a listed area, add or extend a focused regression test where practical, and report any behavior that remains only partially verified.

## Working rules

1. Audit the current code, tests, documentation, and current [`plan.md`](plan.md) before editing. Distinguish runtime-supported features from parser/schema-only features.
2. Fix the root cause. Do not patch a demo to hide a runtime/layout/theme/backend defect.
3. Keep changes focused. Do not add features while a known regression or stability issue blocks the current priority, unless the roadmap explicitly orders that work.
4. Verify incrementally: compile affected targets, run focused tests, then run the broader relevant suite. For rendering/input claims, use a visible backend smoke test when available; headless tests and hidden-loop profiles do not prove visual correctness or input latency.
5. Maintain the performance acceptance target: more than 60 FPS in normal interactive use, responsive input, live resize, and no freezes. Record what was actually measured and the conditions; do not infer user-visible performance from an uncapped hidden loop alone.
6. Never claim an untested backend, OS, visual state, or feature as verified. State blockers and partial fixes plainly.
7. Preserve user changes already present in the working tree. Inspect `git status` before edits and do not discard or overwrite unrelated work.

## Update after each change

- Update [`tracking.md`](tracking.md) with cause/findings, changes, decisions and trade-offs, verification evidence, status, and next steps.
- Update [`plan.md`](plan.md) when verified progress, risks, milestones, priorities, or open questions change. Move an item to “works” only after the relevant verification.
- Update user-facing guides under [`../docs/`](../docs/README.md), the root README, and examples when public behavior, syntax, setup, or feature coverage changes.
- Keep these agent rules and [`conventions.md`](conventions.md) current when project-wide practices or invariants change. The root `AGENTS.md` should remain a short pointer, not a second copy of these rules.
