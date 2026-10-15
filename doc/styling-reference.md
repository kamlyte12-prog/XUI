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
| `blur` | One number | Accepted and compiled; standard runtime does not apply a blur effect. |
| `transform` | 1–6 number/identifier/function values | `translate`, `scale`, and `rotate`-like syntax can compile; standard runtime does not transform widgets. |
| `font` (`font-family`) | One string or identifier | Selects a loaded family; default is the first `loadFont()` face, or no glyph rendering if none was loaded. |
| `font-size` | One number | Logical font size, default 16; actual rasterization scales with DPI. |
| `font-weight` | One number or identifier | Numeric 100–900 selects nearest registered face weight; default 400. Identifier keywords compile but do not map to weights in the runtime. |
| `letter-spacing` | One number | Extra spacing between glyph positions; default 0. |
| `align` | 1–2 identifiers | `Start`, `Center`, `End`, or `Stretch`; affects container child alignment and text alignment on text-bearing leaves. Default is layout stretch for containers and start for text. The second accepted value is currently unused. |
| `justify` | One identifier | `Start`, `Center`, `End`, `SpaceBetween`, `SpaceAround`, `SpaceEvenly`; main-axis distribution for flex containers. Default `Start`. |
| `grow` | One number | Flex grow factor; default 0. |
| `shrink` | One number | Flex shrink factor; default 1. |
| `cursor` | One identifier | Accepted and compiled; runtime does not set a cursor from `.xss`. |
| `z-index` | One number | Accepted and compiled but not applied. Runtime stacking follows tree order; tab headers and popups use fixed overlay layers. |
| `clip` | One identifier or number | Accepted and compiled; runtime clips text to widget bounds and popups to their boxes, but does not apply this property. |
| `overflow` | 1–2 identifiers | Accepted and compiled; not interpreted by the standard runtime. |
| `animation` | 1–6 identifiers/numbers/easing curves | Keyframe name is validated and retained; runtime does not animate it. |
| `scroll-track` | One color or color token | Scrollbar track tint for `ScrollView`; default translucent dark track. |
| `scroll-thumb` | One color or color token | Scrollbar thumb tint; participates in `hover` and `pressed` state resolution. |
| `scrollbar-size` | One number | Scrollbar thickness in logical pixels, clamped to 4–24; default 10. |
| `scrollbar-visibility` | `auto`, `always`, or `never` | `auto` shows while the view is hovered/focused; default `auto`. |
| `scroll-behavior` | `instant` or `smooth` | Wheel/programmatic animation default for a view; default `instant`. `.xui` `scrollBehavior` can override it. |
| `scroll-speed` | One number | Wheel pixels per notch, clamped to 1–400; default 42. `.xui` `scrollSpeed` can override it. |

Aliases normalize to canonical names: `background` → `bg`, `color` → `fg`, `padding` → `pad`, and `font-family` → `font`.

Tooltip timing and activation are `.xui` behavior properties rather than `.xss` visual properties: use `tooltipShowDelay` / `tooltipHideDelay` (milliseconds), `tooltipTrigger` (`hover`, `focus`, `both`, or `none`), `tooltipHideOnClick`, and `tooltipDisabled`. Style the shared tooltip bubble with a `Tooltip` selector and the properties in this table. `FpsCounter[showFrameTime: false]` selects its compact FPS-only label; the default also displays average milliseconds per frame.

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
| Theme-driven layout metrics | Changing palette re-resolves paint/text but does not reconstruct the merged `.xui`/`.xss` `LayoutStyle` | Explicit `.xui` geometry precedence must be preserved during a theme swap; keep `pad`, `gap`, `size`, min/max, and margins theme-invariant for now. |

No widget type is wholly unstyleable in the standard runtime. Unsupported property behavior is recorded in the property table above (`blur`, `transform`, `z-index`, generic `overflow`, cursor, and animation playback are accepted by the compiler but not applied by the runtime).

## Token expressions

`tokens { name = value }` declares reusable values. Dotted names such as `color.primary.hover` are ordinary token names. The compiler supports arithmetic on numbers with compatible units and color methods `.mix()`, `.darken()`, `.lighten()`, `.alpha()`, `.saturate()`, and `.desaturate()`. Select a palette at runtime with `Xui::setTheme("dark")` after `loadXss()` or before `attach()`. Palette colors and text properties resolve from the selected theme across authored widgets and popup/editor overlays. The current runtime does not reapply theme-dependent layout declarations (`pad`, `gap`, `size`, etc.) after the initial `.xui`/`.xss` merge; keep geometry tokens theme-invariant for now. Keyframes/transitions compile but are not animated.

## Text inheritance

The standard runtime inherits `font`, `font-size`, `fg`, `align`, and `letter-spacing` from parent styles when a child has no explicit value. Child declarations override inherited defaults. Background, border, padding, and layout properties do not inherit.
