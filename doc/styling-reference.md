# `.xss` property reference

This reference lists every property accepted by the current stylesheet compiler, including aliases. “Runtime support” describes the standard `UiRuntime`; a low-level client can read compiled values and implement additional behavior.

## Values, units, and shorthand

Colors accept `#rgb`, `#rgba`, `#rrggbb`, and `#rrggbbaa`, or a color token. Strings use double quotes. Bare identifiers represent enums, names, or token references.

The parser recognizes numeric suffixes `px`, `%`, `em`, `rem`, `vw`, `vh`, `dp`, `sp`, `ms`, `s`, and `deg`. The standard runtime performs layout in logical pixels: `size` and min/max size properties support px and `%` (unitless values and `dp` are logical pixels); percentages resolve against the parent's content box after border and padding, then min/max clamp the border-box result. Padding, margin, typography, and paint dimensions use logical values. Prefer `px` for lengths and unitless values for opacity, weights, and ratios. Full CSS unit conversion is not implemented.

Side shorthands use one to four values: one applies to all sides; two means vertical/horizontal; three means top/horizontal/bottom; four means top/right/bottom/left. Pair shorthands use one value for both axes or two values for x/y.

## Canonical properties

| Property | Accepted values | Runtime behavior / fallback |
| --- | --- | --- |
| `bg` (`background`) | One color or color token | Paints background; default transparent. |
| `fg` (`color`) | One color or color token | Text, splitter, slider/progress fill tint; default `#f5f5f5`. |
| `border` | 1–3 values: width, optional style identifier, optional color | Paints when a background is painted; default none. Runtime recognizes `dashed`; other style names currently render as solid. |
| `radius` | 1–4 numbers | Rounded background corners; default 0. |
| `pad` (`padding`) | 1–4 numbers/identifiers | Content inset and text clip inset; default 0. |
| `margin` | 1–4 numbers/identifiers | Outer layout spacing; default 0. |
| `gap` | 1–2 numbers | Row/column spacing; default 0. |
| `size` | 1–2 numbers or identifiers | Fixed width/height; one value applies to both; default auto/intrinsic. `%` is resolved against available layout size. |
| `min-size` | 1–2 lengths | Minimum width/height; default 0. Supports px and percentages. |
| `max-size` | 1–2 lengths | Maximum width/height; default unbounded. Supports px and percentages. |
| `min-width`, `max-width` | One length | Width constraints; use px or `%`; applied after width resolution. |
| `min-height`, `max-height` | One length | Height constraints; use px or `%`; applied after height resolution. |
| `opacity` | One number | Multiplies paint alpha, clamped to 0–1; default 1. |
| `shadow` | 1–6 numeric/color/identifier values | Runtime reads x, y, blur, spread, and color; default no shadow. The optional sixth value is currently unused. |
| `glow` | `blur spread color` | Draws a separate outer halo using the SDF shadow primitive; e.g. `glow: 10px 1px #5b8cff80`. It does not replace or modify `shadow`. |
| `gradient` | `linear-gradient(angle, stops...)`, `radial-gradient(stops...)`, or `conic-gradient(angle, stops...)` | Fills a widget background with 2–4 evenly spaced colors. Angles use degrees; custom stop positions and gradient borders/text are not supported. |
| `blur` | One number | Accepted and compiled; standard runtime does not apply a blur effect. |
| `transform` | Up to 6 values using `translate(x, y)`, `scale(x[, y])`, and `rotate(angle)` | Applies a paint transform to the widget and its descendants; hit testing uses the inverse transform. Translation values use logical pixels or percentages of the widget size; rotation uses degrees. |
| `font` (`font-family`) | One string or identifier | Selects a loaded family; default is the first `loadFont()` face, or no glyph rendering if none was loaded. |
| `font-size` | One number | Logical font size, default 16; actual rasterization scales with DPI. |
| `font-weight` | One number or identifier | Numeric 100–900 selects nearest registered face weight; default 400. Identifier keywords compile but do not map to weights in the runtime. |
| `letter-spacing` | One number | Extra spacing between glyph positions; default 0. |
| `align` | 1–2 identifiers | `Start`, `Center`, `End`, or `Stretch`; affects container child alignment and text alignment on text-bearing leaves. Default is layout stretch for containers and start for text. The second accepted value is currently unused. |
| `justify` | One identifier | `Start`, `Center`, `End`, `SpaceBetween`, `SpaceAround`, `SpaceEvenly`; main-axis distribution for flex containers. Default `Start`. |
| `grow` | One number | Flex grow factor; default 0. |
| `shrink` | One number | Flex shrink factor; default 1. |
| `cursor` | `default`, `pointer`, `text`, `crosshair`, `resize-ew`, `resize-ns`, `resize-nwse`, `resize-nesw`, `move`, `not-allowed`, `grab`, `grabbing`, `hidden` | Explicit `.xss` value overrides widget cursor defaults. Backends may map unsupported shapes to a nearby cursor. |
| `z-index` | One number | Accepted and compiled but not applied. Runtime stacking follows tree order; tab headers and popups use fixed overlay layers. |
| `clip` | One identifier or number | Accepted and compiled; runtime clips text to widget bounds and popups to their boxes, but does not apply this property. |
| `overflow` | 1–2 identifiers | Accepted and compiled; not interpreted by the standard runtime. |
| `animation` | `name duration [iterations|infinite|paused] [easing] [cancel(stop|finish)]` | Named keyframes start automatically unless `paused` is present. `cancel(...)` overrides the widget's `animation-cancel` rule for this animation. Duration accepts `ms` or `s`; default is one iteration. |
| `animation-cancel` | `stop` or `finish` | Per-widget cancellation policy. `stop` is the default and applies the newest state immediately. `finish` completes the current transition target or current keyframe iteration before applying the newest state. |
| `value` | One number, inside keyframes | Animates the normalized value of `ProgressBar` and other runtime controls that expose a numeric value; the ordinary visual style rule does not set control state. |
| `scroll-track` | One color or color token | Scrollbar track tint for `ScrollView`; default translucent dark track. |
| `scroll-thumb` | One color or color token | Scrollbar thumb tint; participates in `hover` and `pressed` state resolution. |
| `scrollbar-size` | One number | Scrollbar thickness in logical pixels, clamped to 4–24; default 10. |
| `scrollbar-visibility` | `auto`, `always`, or `never` | `auto` shows while the view is hovered/focused; default `auto`. |
| `scroll-behavior` | `instant` or `smooth` | Wheel/programmatic animation default for a view; default `instant`. `.xui` `scrollBehavior` can override it. |
| `scroll-speed` | One number | Wheel pixels per notch, clamped to 1–400; default 42. `.xui` `scrollSpeed` can override it. |

Aliases normalize to canonical names: `background` → `bg`, `color` → `fg`, `padding` → `pad`, and `font-family` → `font`.

Tooltip timing and activation are `.xui` behavior properties rather than `.xss` visual properties: use `tooltipShowDelay` / `tooltipHideDelay` (milliseconds), `tooltipTrigger` (`hover`, `focus`, `both`, or `none`), `tooltipHideOnClick`, and `tooltipDisabled`. Style the shared tooltip bubble with a `Tooltip` selector and the properties in this table. `FpsCounter[showFrameTime: false]` selects its compact FPS-only label; the default also displays average milliseconds per frame.

In the GLFW showcase, F4 adds the previous completed frame's `Painter::Stats.drawCalls` batch count to the FPS counter. It is a backend-neutral draw-list metric, not a GPU submission count. F4 does not enable continuous rendering; it refreshes when another event or animation causes a frame. F3 enables continuous FPS sampling.

Border example:

```xss
Panel.card { resting: { border: 1px solid #344563 } }
```

## Selectors and states

Selectors can combine a widget type, classes, an ID, and schema-level property filters. Current runtime matching supports type/class/ID. State blocks include `resting`, `enabled`, `readonly`, `hover`, `focus`, `focus-visible`, `selected`, `checked`, `open`, `closed`, `pressed`, `dragging`, `loading`, `error`, `disabled`, and `placeholder`; `active` aliases `pressed`. The runtime activates `placeholder` for an empty editable field that has placeholder text and `readonly` for `TextBox`, `TextArea`, `NumberField`, and `SearchField` in read-only mode. `Switch` and `ToggleButton` activate `checked`; `CheckItem`/`CheckBox` activate `checked` and `selected`.

`resting` provides the base values. Matching rules are applied in deterministic selector score then source order. Active states are merged in fixed enum order. The current runtime activates only the states described in the [`.xss` guide](xss-guide.md); other states can be compiled without becoming active.

Popup controls keep interaction configuration in `.xui` and appearance in `.xss`. Dropdown option rows are virtual targets and currently use a row-local highlight derived from the control foreground; the header's `hover`, `focus`, and `open` states only style the header. Color picker surfaces use the matched control's `bg`, `fg`, border, radius, size, and state values. The hue spectrum is generated by the picker and does not yet expose separate XSS properties for its sub-controls. See the [widget guide](widgets.md#popup-controls) for supported `.xui` color formats and current customization limits.

Composite widget chrome is audited against the same style table: `CheckBox`/`CheckItem` use `fg` for the checked fill and border, `radius` for the indicator, and `bg`/`fg` for their surface/text; `Switch` uses `fg` for its on track and a dimmed `fg` for the off track; `RadioGroup`, `SegmentedControl`, and `Rating` derive selected, hover, and outline tints from `fg`; editable-field selection highlights also derive from `fg`; and an `Image` placeholder uses `fg`, `border`, and `radius`. The switch knob and checkbox tick remain fixed contrast marks. Dropdown popup rows use a row-local tint derived from the control `fg`; per-option selectors are not supported. The color picker spectrum and channel geometry are generated from the chosen color and retain semantic hues rather than inheriting a single theme color. Tab header height and equal-width distribution, spinner cadence, and the SearchField clear glyph geometry are built in. These are widget-specific behaviors, not unhandled `.xss` properties; the shared surface, typography, border, radius, padding, opacity, and applicable state properties still resolve from `.xss`.

### Widget styling audit: current limitations

Every runtime-created authored node receives a matched `StyleSetId` and resolves `.xss` declarations for the active state and selected palette. Internal composite pieces reuse their owner's resolved values where listed above. A small number of subparts are deliberately not independent style targets:

| Widget / subpart | What remains fixed or limited | Reason |
| --- | --- | --- |
| `Tabs` headers | Header is 34 logical pixels high and pages use equal-width headers; there are no per-header dimensions | Header geometry is owned by the tab control; `Tab` selectors still style background, foreground, font, border, radius, opacity, and states. |
| `Dropdown` options | No per-option selector or option-specific style; row hover/selection tint derives from owner foreground | Options are virtual rows rather than `.xui` nodes. |
| `ColorPicker` | Hue/saturation/value/alpha surfaces retain semantic color ramps; no per-channel selectors | The ramps represent color values. The popup frame, typography, border, padding, opacity and interaction states use the matched `.xss` rule. |
| `DatePicker` day cells | Calendar cell geometry and some semantic selection/disabled marks are internal | Cells are virtual options, not individual runtime nodes. The owner supplies its surface and text palette. |
| `CheckBox`, `CheckItem`, `Switch` | Check tick and switch knob use fixed contrast colors | These marks remain legible over the accent fill; their indicator fill/border/track and labels use owner style values. |
| `Spinner` | Eight-tick geometry and 80 ms step cadence | Animation geometry is a control behavior; tick color and opacity inherit the resolved foreground/disabled state. |
| `SearchField` | Clear affordance geometry is fixed | It is a built-in hit target; field surface, text, border, radius, opacity and states remain style-driven. |
| `TitleBar` caption zones | Minimize/maximize glyph shapes are fixed painter marks; zones are equal-width (46 px) at the bar's right end | Zone geometry is owned by the window-chrome layer so children can never underlap it. The `TitleButton` selector styles zone `fg`, `bg`, `radius`, `opacity`, and `hover`/`pressed` states; the bar itself is a normal `TitleBar` style target. |
| `NumberField`/`Stepper` step buttons | −/+ glyph geometry is fixed painter marks in a reserved right zone | The `NumberButton` selector styles zone `fg`, `bg`, `radius`, and `hover`/`pressed` states; the field itself keeps the shared surface/typography/border states plus the `error` state while its text does not parse. |
| `Menu` popup rows | Popup surface geometry (row heights, insets, flip rules) is internal; rows are virtual targets, not laid-out children | Per-item selectors are not available. The `Menu` rule styles the popup surface; the `MenuItem` rule styles every row with `resting`/`hover`/`checked`/`disabled` states. |
| Theme-driven layout metrics | Changing palette re-resolves paint/text but does not reconstruct the merged `.xui`/`.xss` `LayoutStyle` | Explicit `.xui` geometry precedence must be preserved during a theme swap; keep `pad`, `gap`, `size`, min/max, and margins theme-invariant for now. |

No widget type is wholly unstyleable in the standard runtime. `blur`, `z-index`, and generic `overflow` are accepted by the compiler but are not applied by the runtime. Transformed clips use axis-aligned scissor bounds, and overlay popups remain in window coordinates.

## Alpha, gradients, and glow

`bg` colors can include alpha (`#rrggbbaa`); `opacity` multiplies the authored widget's own paint, including its own text and composite marks. Child nodes resolve opacity independently. Both affect paint only. A transparent widget remains eligible for hit testing according to its widget type and layout bounds.

Gradients are authored as a single `gradient` value. Linear/conic forms take an angle first; radial does not. The runtime evenly spaces two to four stops. They fill the rounded widget background and preserve stop alpha. Stop locations, gradient borders, and gradient text are not implemented.

`glow` is a separate outer halo; `shadow` keeps its offset drop-shadow behavior. Glow uses the existing SDF shadow draw primitive with zero offset, so each glowing widget adds render work and overlapping halos can be costly. The runtime does not cache pre-blurred glow textures. Keep blur radii modest and avoid animating many large glows at once.

## Transitions and keyframes

Transitions animate changed paint values when a control enters the declared destination state. `all` selects all supported paint properties; otherwise list property names:

```xss
Button.action {
  resting: { bg: #2459a6, glow: 0px 0px #559cff00 }
  hover: { bg: #397dd1, glow: 8px 0px #559cff99 }
  transition: resting -> hover 160ms ease-out bg glow
  transition: hover -> resting 160ms ease-out bg glow
}
```

Named keyframes use `animation: name duration [count|infinite|paused] [easing] [cancel(stop|finish)]`:

```xss
Panel.pulse { resting: { animation: pulse 900ms infinite ease-in-out cancel(finish) } }
keyframes pulse {
  from { opacity: 0.5 }
  50% { opacity: 1 }
  to { opacity: 0.5 }
}
```

Numbers with matching units, colors, and compatible transform function sequences animate. Layout and font face/weight are not interpolated. `animation: name duration paused ...` holds the first keyframe until `Xui::playAnimation(id, mode)` is called. `Once` plays one cycle and leaves the last sample applied; `Loop` repeats until `cancelAnimation(id)`, which always stops immediately and holds its current sample. A later play request overrides and restarts playback. Visible animations schedule frames on a 16 ms timer; static and paused screens return to the platform event wait. GLFW reports pointer exit and window visibility, so hover-out animations run and hidden/minimized windows pause animation clocks. See [the motion showcase](../examples/showcase.xui) and [stylesheet](../examples/showcase.xss).

## Cursor values

Use `cursor` on a widget selector or state block. If unset, the runtime chooses a widget default (for example, `Button` uses a hand and editable fields use an I-beam). GLFW maps `resize-nwse`, `resize-nesw`, `move`, `grab`, and `grabbing` to a hand cursor where no matching standard shape is available; `not-allowed` falls back to the arrow. Hidden cursor mode is supported by GLFW. Alpha and cursor styling do not change hit testing.

## Token expressions

`tokens { name = value }` declares reusable values. Dotted names such as `color.primary.hover` are ordinary token names. The compiler supports arithmetic on numbers with compatible units and color methods `.mix()`, `.darken()`, `.lighten()`, `.alpha()`, `.saturate()`, and `.desaturate()`. Select a palette at runtime with `Xui::setTheme("dark")` after `loadXss()` or before `attach()`. Palette colors and text properties resolve from the selected theme across authored widgets and popup/editor overlays. The current runtime does not reapply theme-dependent layout declarations (`pad`, `gap`, `size`, etc.) after the initial `.xui`/`.xss` merge; keep geometry tokens theme-invariant for now. Transitions and visible keyframes animate supported paint values and transforms; see [Transitions and keyframes](#transitions-and-keyframes).

## Text inheritance

The standard runtime inherits `font`, `font-size`, `fg`, `align`, and `letter-spacing` from parent styles when a child has no explicit value. Child declarations override inherited defaults. Background, border, padding, and layout properties do not inherit.
