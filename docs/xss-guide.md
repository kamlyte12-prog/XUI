# `.xss` styling guide

`.xss` is Xui's stylesheet language. It selects nodes by type, class, ID, and optional property filters, then supplies paint/text values and interaction-state variants. It is intentionally a separate file from `.xui` structure.

## Rule syntax

```xss
Button.primary {
  resting: { bg: #2563eb, fg: #ffffff, size: 200px 44px, radius: 8px }
  hover: { bg: #3b82f6 }
  pressed: { opacity: 0.85 }
  disabled: { opacity: 0.45 }
}
```

A rule is a selector followed by braces. Declarations are `property: value`; commas, semicolons, or line boundaries can separate declarations. A declaration outside a named state block belongs to `resting`. Multiple selectors can share one rule with commas. Comments use `//` and `/* ... */`.

## Selectors and precedence

Supported selector forms are:

```xss
Button { ... }                     // widget type
.primary { ... }                   // any node with a class
#save { ... }                       // exact id
Button.primary#save { ... }        // combination
* { ... }                           // universal
Button[variant: primary] { ... }   // property filter syntax
Card::title { ... }                // marked part inside every Card component instance
```

The runtime currently matches type, class, ID, and component-part markers. A `Component::part` selector is compiled to the same class-matching path as other selectors; components must mark the internal widget with `[part: "part"]` in their definition. Property filters parse and compile, but the current `UiRuntime` does not provide a property lookup callback, so filtered selectors should not be relied on in runtime UI.

Matching rules use a simple deterministic score: ID selectors outrank classes, classes outrank filters, and filters outrank type selectors; later source order wins when scores tie. This is not full CSS: there are no descendant/child combinators, pseudo-classes, selector inheritance, or browser-style specificity rules.

## States and cascade

Every matched rule contributes a resting block and any active state blocks. Active blocks are merged in the framework's fixed state order; later states in that order override earlier declarations for the same property. The current runtime can set hover, focus, checked/selected, open, pressed, and disabled states on supported controls.

Supported state names are `resting`, `hover`, `focus`, `focus-visible`, `selected`, `checked`, `open`, `closed`, `pressed`, `dragging`, `loading`, `error`, and `disabled`. `active` is an alias for `pressed`. Only states that the runtime actually activates can affect current widgets. A stylesheet may compile other state blocks without any control reaching those states.

Text defaults `font`, `font-size`, `fg`, `align`, and `letter-spacing` cascade from a parent when a child has no matching explicit value. An explicit child style wins. Layout values and backgrounds do not inherit.

The compiler starts with a Fluent 2-inspired light baseline for standard runtime widgets, then applies authored rules through the normal cascade. Define your own colors and control rules to replace it. `outline` and `outline-offset` add a paint-only focus ring outside the border box; they do not consume layout space.

## Palette tokens and themes

Declare reusable values with `tokens` and refer to them by name:

```xss
tokens {
  color.surface = #111827
  color.text = #f9fafb
  color.primary = #2563eb
  color.primary.hover = color.primary.lighten(0.12)
  space.page = 24px
  space.gap = 12px
}

Column.page { resting: { bg: color.surface, fg: color.text, pad: space.page, gap: space.gap } }
```

Token expressions support numeric `+`, `-`, `*`, `/` with compatible units; color methods `.mix(color[, amount])`, `.darken(amount)`, `.lighten(amount)`, `.alpha(amount)`, `.saturate(amount)`, and `.desaturate(amount)`; and easing values for transitions. Cycles and unknown token references are compile errors.

The compiler starts with these palette tokens, which the stylesheet can override: `color.on-primary` (`#ffffff`), `color.primary.active` (`#1d4ed8`), `color.muted` (`#52525b`), `color.border` (`#27272a`), and `color.shadow` (`#00000066`).

The parser/compiler supports `@theme` token sets. The standard facade/runtime can select one by name: call `ui.setTheme("dark")` after `loadXss()`; the choice may be made before or after `attach()`. This changes palette-backed paint and text styles, including generated popup/editor nodes. Layout-affecting theme tokens are not reapplied after the `.xui`/`.xss` layout merge yet; use themes for colors and typography while keeping dimensions, padding, margins, and gaps invariant.

## Styling versus layout

The `.xss` language carries both visual paint rules and layout defaults. In the runtime, `size`, `min-size`, `max-size`, `min-width`, `max-width`, `min-height`, `max-height`, `pad`, `margin`, `gap`, `grow`, `shrink`, `align`, and `justify` feed `LayoutTree`. Background, foreground, border, radius, shadow, opacity, and text properties feed painting. Layout stays in logical pixels; the painter and text atlas handle output DPI. Percent dimensions resolve against the parent content box after its border and padding; min/max clamps apply to the resolved size.

For a fixed centered action:

```xui
Column[class: "actions"] {
  Button(label: "Save", press: onSave)[class: "primary", align: Center]
}
```

```xss
Column.actions { resting: { gap: 8px, pad: 16px } }
Button.primary { resting: { size: 200px 42px, bg: #2563eb, fg: #ffffff, radius: 7px, font-size: 15px } }
```

Use `.xui align` when setting item alignment in the structure. `.xss align` also controls text alignment on text-bearing leaves and container child alignment; prefer `.xui align` for structural alignment to make intent obvious.

### Initial window defaults

The root `Window` selector may set `window-title` and `size` for the initial native window:

```xss
Window { resting: { window-title: "My application", size: 1000px 700px, radius: 12px } }
```

These are defaults. Explicit `XuiOptions` values override `.xui Window(title, size)`, which override these `.xss` values. `.xss radius` also styles the client surface; on Windows 11 the GLFW backend requests matching DWM window corners for a custom title bar. The backend excludes the outside corner pixels from hit testing. Other platforms retain their rectangular native window shape. See the [`.xui` guide](xui-guide.md#window-defaults-and-precedence) for exact precedence and how to override a default-valued `XuiOptions` field.

## Transitions and keyframes

The compiler accepts `transition` declarations and named `keyframes`:

```xss
Button.primary {
  resting: { bg: #2563eb, glow: 0px 0px #3b82f600 }
  hover: { bg: #3b82f6, glow: 8px 0px #3b82f688 }
  transition: resting -> hover 120ms ease-out bg glow
  transition: hover -> resting 120ms ease-out bg glow
}

Panel.pulse { resting: { animation: pulse 900ms infinite ease-in-out } }
keyframes pulse {
  from { opacity: 0.45 }
  50% { opacity: 1 }
  to { opacity: 0.45 }
}
```

The standard runtime advances these animations. Transitions start when a state changes into the declared destination state. List the properties to interpolate, or use `all` for all supported paint properties. Keyframe animations accept a duration, an optional finite iteration count or `infinite`, an optional `paused` flag, and optional easing. Supported easing includes linear/ease-in/ease-out/ease-in-out and compiled cubic-bezier or spring curves.

Animations include paint values and widget transforms; layout geometry and font selection do not animate. Automatic keyframes run on visible widgets without requiring pointer or keyboard input, and continue while the window is visible but unfocused. A `paused` keyframe can be controlled by the public facade:

```cpp
ui.playAnimation("meter", xui::AnimationMode::Once); // or Loop
ui.cancelAnimation("meter");                        // keep the current sample
```

`Once` runs one cycle and holds its final frame. `Loop` repeats until cancelled. Calling `playAnimation` again restarts and replaces current playback. Applications can call `cancelAnimation` when their own condition is met. The loop schedules frames only during active animation and then returns to its normal event wait. See the [complete property reference](styling-reference.md#transitions-and-keyframes) for syntax and limits.

## Paint effects

`bg` colors may include alpha, and `opacity` multiplies the authored widget's own paint, including its text and composite marks. Child nodes resolve opacity independently. Alpha affects rendering only; it does not make a widget pointer-transparent. Use a separate widget/layout choice when an overlay should pass pointer input through.

Background gradients are available as `linear-gradient(angle, stops...)`, `radial-gradient(stops...)`, or `conic-gradient(angle, stops...)`. Two to four stops are evenly spaced. Gradients fill rounded backgrounds and preserve color alpha; custom stop positions and gradient borders/text are not supported.

`glow: blur spread color` draws an outer halo and remains separate from the offset `shadow` property. It uses an additional SDF shadow primitive rather than a cached texture, so keep large glow radii and counts modest. See the [paint-property reference](styling-reference.md#alpha-gradients-and-glow).

Font size and numeric font weight are `.xss` properties. For variable TrueType fonts with a `wght` axis, weight sets that axis and uses cached face instances. Static fonts use the nearest registered face; the runtime does not synthesize bold. Load static weights with `loadFont(path, family, weight)` when a design needs them. Font formats and axis fallback behavior are listed in the [getting-started guide](getting-started.md#fonts). `size: auto` uses intrinsic content sizing; min/max still clamp the result, and nested Button content participates in the measurement.

## Best practices

- Put reusable colors and spacing in tokens rather than repeating literals.
- Use type rules for defaults and class/ID rules for exceptions.
- Add an explicit resting rule before state rules so controls remain styled when idle.
- Use `.xui` for widget hierarchy and event names; avoid turning `.xss` into layout structure.
- Keep selectors simple because combinators and runtime property filters are not implemented.
- See [the property reference](styling-reference.md) for exact parser arities and runtime support.
