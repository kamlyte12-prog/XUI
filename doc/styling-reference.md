# `.xss` property reference

This reference lists every property accepted by the current stylesheet compiler, including aliases. “Runtime support” describes the standard `UiRuntime`; a low-level client can read compiled values and implement additional behavior.

## Values, units, and shorthand

Colors accept `#rgb`, `#rgba`, `#rrggbb`, and `#rrggbbaa`, or a color token. Strings use double quotes. Bare identifiers represent enums, names, or token references.

The parser recognizes numeric suffixes `px`, `%`, `em`, `rem`, `vw`, `vh`, `dp`, `sp`, `ms`, `s`, and `deg`. The standard runtime performs layout in logical pixels: `size` converts `%` to a fraction and treats other units as numeric pixels; `min-size`/`max-size`, padding, margin, typography, and paint dimensions currently use the numeric component directly. Prefer `px` for lengths and unitless values for opacity, weights, and ratios. Full CSS unit conversion is not implemented.

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
| `min-size` | 1–2 numbers or identifiers | Minimum width/height; default 0. |
| `max-size` | 1–2 numbers or identifiers | Maximum width/height; default unbounded. |
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

Aliases normalize to canonical names: `background` → `bg`, `color` → `fg`, `padding` → `pad`, and `font-family` → `font`.

Tooltip timing and activation are `.xui` behavior properties rather than `.xss` visual properties: use `tooltipShowDelay` / `tooltipHideDelay` (milliseconds), `tooltipTrigger` (`hover`, `focus`, `both`, or `none`), `tooltipHideOnClick`, and `tooltipDisabled`. Style the shared tooltip bubble with a `Tooltip` selector and the properties in this table. `FpsCounter[showFrameTime: false]` selects its compact FPS-only label; the default also displays average milliseconds per frame.

Border example:

```xss
Panel.card { resting: { border: 1px solid #344563 } }
```

## Selectors and states

Selectors can combine a widget type, classes, an ID, and schema-level property filters. Current runtime matching supports type/class/ID. State blocks include `resting`, `hover`, `focus`, `focus-visible`, `selected`, `checked`, `open`, `closed`, `pressed`, `dragging`, `loading`, `error`, `disabled`, and `placeholder`; `active` aliases `pressed`. The runtime activates `placeholder` for an empty `TextBox` or `TextArea` that has placeholder text.

`resting` provides the base values. Matching rules are applied in deterministic selector score then source order. Active states are merged in fixed enum order. The current runtime activates only the states described in the [`.xss` guide](xss-guide.md); other states can be compiled without becoming active.

Popup controls keep interaction configuration in `.xui` and appearance in `.xss`. Dropdown option rows are virtual targets and currently use a row-local highlight derived from the control foreground; the header's `hover`, `focus`, and `open` states only style the header. Color picker surfaces use the matched control's `bg`, `fg`, border, radius, size, and state values. The hue spectrum is generated by the picker and does not yet expose separate XSS properties for its sub-controls. See the [widget guide](widgets.md#popup-controls) for supported `.xui` color formats and current customization limits.

## Token expressions

`tokens { name = value }` declares reusable values. Dotted names such as `color.primary.hover` are ordinary token names. The compiler supports arithmetic on numbers with compatible units and color methods `.mix()`, `.darken()`, `.lighten()`, `.alpha()`, `.saturate()`, and `.desaturate()`. `@theme` and keyframes/transitions compile, but the runtime currently uses only the default theme and does not animate.

## Text inheritance

The standard runtime inherits `font`, `font-size`, `fg`, `align`, and `letter-spacing` from parent styles when a child has no explicit value. Child declarations override inherited defaults. Background, border, padding, and layout properties do not inherit.
