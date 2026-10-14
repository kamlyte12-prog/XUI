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
```

The runtime currently matches type, class, and ID. Property filters parse and compile, but the current `UiRuntime` does not provide a property lookup callback, so filtered selectors should not be relied on in runtime UI.

Matching rules use a simple deterministic score: ID selectors outrank classes, classes outrank filters, and filters outrank type selectors; later source order wins when scores tie. This is not full CSS: there are no descendant/child combinators, pseudo-classes, selector inheritance, or browser-style specificity rules.

## States and cascade

Every matched rule contributes a resting block and any active state blocks. Active blocks are merged in the framework's fixed state order; later states in that order override earlier declarations for the same property. The current runtime can set hover, focus, checked/selected, open, pressed, and disabled states on supported controls.

Supported state names are `resting`, `hover`, `focus`, `focus-visible`, `selected`, `checked`, `open`, `closed`, `pressed`, `dragging`, `loading`, `error`, and `disabled`. `active` is an alias for `pressed`. Only states that the runtime actually activates can affect current widgets. A stylesheet may compile other state blocks without any control reaching those states.

Text defaults `font`, `font-size`, `fg`, `align`, and `letter-spacing` cascade from a parent when a child has no matching explicit value. An explicit child style wins. Layout values and backgrounds do not inherit.

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

The parser/compiler supports `@theme` token sets and `StyleSheet` can resolve by theme index. The current `UiRuntime` always resolves theme index zero and does not expose a theme-switching method. Theme syntax is currently useful to low-level clients.

## Styling versus layout

The `.xss` language carries both visual paint rules and layout inputs. In the runtime, `size`, `min-size`, `max-size`, `pad`, `margin`, `gap`, `grow`, `shrink`, `align`, and `justify` feed `LayoutTree`. Background, foreground, border, radius, shadow, opacity, and text properties feed painting. Layout stays in logical pixels; the painter and text atlas handle output DPI.

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

## Transitions and keyframes

The compiler accepts `transition` declarations and named `keyframes`:

```xss
Button.primary {
  resting: { bg: #2563eb }
  hover: { bg: #3b82f6 }
  transition: resting -> hover 120ms ease-out bg
}

keyframes pulse {
  from { opacity: 0.7 }
  to { opacity: 1 }
}
```

Transition/keyframe data is retained in `StyleSheet` for low-level consumers. `UiRuntime` currently does not advance transitions or apply animation keyframes. Do not expect animation from the standard `Xui` entry point yet.

## Best practices

- Put reusable colors and spacing in tokens rather than repeating literals.
- Use type rules for defaults and class/ID rules for exceptions.
- Add an explicit resting rule before state rules so controls remain styled when idle.
- Use `.xui` for widget hierarchy and event names; avoid turning `.xss` into layout structure.
- Keep selectors simple because combinators and runtime property filters are not implemented.
- See [the property reference](styling-reference.md) for exact parser arities and runtime support.
