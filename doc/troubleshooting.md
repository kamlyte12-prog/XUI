# Troubleshooting

Start with the actual error string from `Xui::lastError()`, then confirm the runtime supports each object. The parser may accept a schema name that the runtime does not yet materialize.

## Window does not appear

1. Check each load call and print `ui.lastError()` immediately on failure.
2. Confirm `.xui`, `.xss`, and font paths are relative to the process working directory or use absolute paths during diagnosis.
3. Confirm `attach()` succeeds and the backend reports a valid window and nonzero framebuffer size.
4. Set `XuiOptions::visible = true`; check `glContext` matches the backend needs.
5. Verify the renderer's required system graphics libraries/context are present.

`Xui` creates the native title/size from `XuiOptions`. The `Window(title, size)` values in `.xui` are runtime document metadata and currently do not override those native window settings.

## Text is blank, missing, or shows boxes

- Load a real supported TrueType font before `attach()`. If no valid default font is loaded, text paint is skipped.
- Confirm the `.xss` family name exactly matches `loadFont(path, family, weight)`. If it does not match, the runtime falls back to the first loaded face.
- Confirm `attach()` initializes the renderer and glyph atlas successfully. Glyph coverage uses `TextureFormat::A8`; a renderer that treats it as RGBA or ignores texture alpha will draw incorrect glyphs.
- Check that the draw-list backend samples the atlas's alpha/coverage channel and preserves the documented texture lifecycle.
- If only long strings fail, inspect their available width and `.xss size`; the standard `Text` widget wraps to constrained width and clips to its bounds.
- Complex-script shaping and automatic fallback fonts are not yet supported; start with a short Latin string to isolate font/atlas problems.

## UI freezes or responds slowly

- Use the direct GPU renderer for interactive windows. Full-frame CPU `SoftRenderer` rasterization is for reference/testing and can fall below 60 FPS.
- Ensure `Platform::waitEvents()` blocks when the queue is empty and `pumpEvents()` drains queued events promptly. A busy loop wastes CPU; a blocking event pump that ignores input can stall interaction.
- Confirm pointer coordinates are logical client coordinates with a top-left origin and that physical framebuffer size is not being passed to `UiRuntime::layout()`.
- Keep the UI loop on the thread that owns the platform/graphics context unless the backend explicitly documents thread-safe operations.
- Disable continuous redraw in normal mode. `Xui::run()` redraws when state changes and sleeps while idle, except when the FPS counter is active.
- Compare `--profile` results to a normal visible run, accounting for vsync and GPU load.

## Text overlaps or escapes a panel

- Give the text node a width constraint through its parent flow or a `.xss size` rule.
- Use `Column`/`Row` flow instead of overlapping absolute assumptions; each wrapped line contributes to intrinsic text height.
- Give fixed-height labels enough height for expected lines. Fixed boxes clip overflow; automatic text sizing needs constrained width from the parent.
- Check that the renderer honors scissor/clip commands. The UI runtime clips glyph runs to each text widget's content box.
- Keep padding/spacing in `.xss`; `.xui` is for tree/layout intent.

## Button or control styles do not change

- Check that the class is a quoted string, e.g. `[class: "primary"]`, and the selector matches the exact widget type/class/ID.
- Put base values under `resting`; use runtime-activated states such as `hover`, `pressed`, `focus`, `checked`, `open`, and `disabled`.
- `active` aliases `pressed`. Some compiled states (for example `loading`) are not activated by current controls.
- A disabled control uses `[disabled: true]`; `disabled: false` keeps it interactive.
- Note that `.xss` selectors do not support combinators and the runtime does not currently provide values for property-filter selectors.

## Splitter or resize behavior is wrong

- Use `Split(direction: Horizontal)` for side-by-side panes and `Vertical` for stacked panes.
- Confirm it has at least two children and a visible `gutter` size.
- Layout input dimensions must be logical client size; renderer resize uses physical framebuffer size plus DPI.
- Check backend resize handling does not block while waiting for a GPU or perform software rasterization on the UI thread.

## Dropdown/color picker issues

- Dropdown options use a pipe-separated string such as `"Small|Medium|Large"`; option values cannot include a pipe.
- Dropdown is single-select. Click to open/select, use Escape to close, and use arrow keys when focused.
- The color picker is an in-app HSV popup, not a native dialog. Alpha is enabled by default; set `alpha: false` to force full opacity, and use `showAlpha` to control the alpha strip.
- Read the current selected text/color from an event callback using `ui.runtime().controlText(id)` or `controlColor(id)`.
- Dropdown header hover is scoped to the header; popup option hover is a row-local virtual target. Use an `open` or `focus` selector to keep a stable header outline while the popup is open. Per-option selectors are not currently supported.
- Slider `change` callbacks fire for each distinct value during a drag; update a `Text[id: ...]` node from the callback with `setValue` to keep an authored readout live.
- A color value row accepts HEX, comma-separated RGB(A), HSL(A), and HSV(A). The editor applies complete valid input as it is typed; an incomplete or invalid draft stays visible and does not overwrite the last valid RGBA `Color`. Clipboard support depends on the host platform implementation.
- The color picker has popup layout only. Named CSS colors, inline mode, standalone numeric fields, and individually styled sub-controls are not currently supported.

## Stylesheet parsing errors

- Separate tokens with `=` and rule properties with `:`:

  ```xss
  tokens { color.panel = #172033 }
  Panel.card { resting: { bg: color.panel, pad: 16px } }
  ```

- Check that each property uses the value kinds/arities in [the style reference](styling-reference.md).
- Arithmetic requires compatible units; for example `8px + 4px` is valid while `8px + 4%` is not.
- `Xui::setTheme("name")` selects a named `@theme` palette after `.xss` loading. Palette-backed visual properties update at runtime; layout-affecting tokens are not fully re-applied yet. Transitions and visible keyframes animate supported paint values and transforms. See the [animation guide](animations-fonts-transforms.md).

## Still stuck?

Record the exact backend, OS, build configuration, `.xui`/`.xss` snippet, font path/family, window size/DPI, and whether the failure occurs with `SoftRenderer` or OpenGL. Reduce the document to a root `Window`, one `Text`, and one style rule, then add controls back until the failure returns.

For apparent overlap, first distinguish authored layering from flow: explicit `Position::Absolute`, `Stack`, tab-page stacking, and explicit grid cells can overlap by design. `Row`/`Column` children are placed sequentially; if they overflow their parent, inspect width/height, margins, `%`, and min/max constraints. Content outside a `ScrollView` viewport is intentionally clipped. A visible FPS overlay runs continuous redraw and reports actual frame cadence; disable it with F3 for event-driven idle behavior.
