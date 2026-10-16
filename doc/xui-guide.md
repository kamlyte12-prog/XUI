# `.xui` layout and structure guide

`.xui` is a small declarative tree language. It describes widgets, their parent/child structure, literal content, layout intent, identifiers, and event handler names. Visual rules belong in `.xss`.

## File structure and syntax

A file contains top-level declarations. A runtime document must have exactly one `Window` root. A widget has this form:

```xui
WidgetName(positionalValue, named: value)[property: value, event: handler] {
  ChildWidget(...)
}
```

Whitespace and newlines separate sibling widgets. Commas separate constructor arguments and bracket properties. Constructor arguments may be positional or named. Use quoted strings for text and paths; identifiers such as `Horizontal`, `Center`, and `onClick` are bare names.

```xui
Window(title: "Tasks", size: 900x620) {
  Column[class: "page"] {
    Text("Today")[class: "heading"]
    Button(label: "Add task", press: onAdd)[id: "add", class: "primary"]
  }
}
```

Comments support `// line comments` and `/* block comments */`. Strings support interpolation (`"Hello {user}"`) and escapes including `\n`, `\t`, and escaped braces. Interpolation is parsed, but this runtime does not evaluate dynamic expressions.

## IDs, classes, properties, and events

- `id: "name"` identifies one node; it is useful for `.xss` selectors and reading dropdown/color-picker state.
- `class: "primary compact"` adds space-separated class names.
- Bracket properties are written as `[name: value]`; the parser validates them against the schema.
- Event properties use `eventName: handlerName`, for example `press: onSave` or `change: onColorChange`.

The parser recognizes `press`, `release`, `click`, `doubleClick`, `change`, `submit`, `focus`, `blur`, `hover`, `scroll`, `drag`, `drop`, `key`, `resize`, and `close`. The current runtime wires `press` on buttons and `change` on slider/dropdown/color-picker/scroll-view controls to the same callback mechanism. It passes the handler name, not a value payload. Slider changes are reported immediately for each distinct pointer or keyboard value. Read a ScrollView's offset as a `Vec2` through `UiRuntime::value(id)` or `scrollOffset(id)` inside its callback. Read a dropdown or color value through `value(id)`, `controlText(id)`, or `controlColor(id)`; color picker format/options are described in the [widget guide](widgets.md#popup-controls). Other parser event names are not dispatched by this runtime.

### Scrollable containers

Use a `ScrollView` to bound and clip overflowing content. `vertical` defaults to `true`, `horizontal` to `false`; set both to `true` to permit overflow on both axes. `scrollSpeed` and `scrollBehavior` set the default wheel step and animation; `scrollbar` accepts `"auto"`, `"always"`, or `"never"`.

```xui
ScrollView[id: "results", vertical: true, horizontal: false, scrollSpeed: 42,
          scrollBehavior: "smooth", scrollbar: "auto", width: 360px, height: 280px,
          change: onScroll][class: "results"] {
  Column(gap: 6) {
    Text("A result")
    Text("Another result")
    Button(label: "Jump target")[id: "target"]
  }
}
```

Use `ui.runtime().scrollTo("results", {0, 120})` for direct positioning or `scrollIntoView("target", true)` to reveal a descendant. See the [ScrollView widget reference](widgets.md#scrollview) for nested input routing, clipping, keyboard behavior, and current limitations.

The common validator accepts generic names including `id`, `class`, `key`, `slot`, `grow`, `shrink`, `visible`, `disabled`, `tooltip`, `tooltipShowDelay`, `tooltipHideDelay`, `tooltipTrigger`, `tooltipHideOnClick`, `tooltipDisabled`, `align`, `justify`, `pad`, `margin`, `size`, `minSize`, `maxSize`, `width`, `height`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`, and `opacity`. Tooltip timing/trigger are behavior properties in `.xui`; bubble appearance belongs to `.xss`. The runtime applies structural geometry properties from `.xui`; Row/Column also accept a named `gap` constructor argument, and CheckList accepts `gap` as a property. `.xui` values override the same `.xss` layout defaults. `visible` and `.xui opacity` are still not implemented as runtime state/paint behavior; use `.xss opacity` for visual opacity.

## Supported layout containers

The current `UiRuntime` implements these containers:

| Object | Layout behavior |
| --- | --- |
| `Window` | Required root. Behaves as a vertical column filling the logical client area. |
| `Column` | Flex-like vertical flow. Use `.xss gap`, `pad`, and child `grow` to arrange content. |
| `Row` | Flex-like horizontal flow. |
| `Panel` | Styled vertical container. It is not independently dockable or draggable. |
| `CheckList` | Vertical list container for `CheckItem` children. |
| `Split` | Two or more panes with a draggable gutter. `direction: Horizontal` puts panes side by side; `Vertical` stacks them. |
| `ScrollView` | Bounded scrolling container with viewport clipping, nested wheel routing, keyboard scrolling, and scroll-into-view support. See the [widget reference](widgets.md#scrollview). |

The layout library also has lower-level `Grid` and `Stack` solvers, but the ordinary `.xui` runtime does not yet materialize them. Absolute positioning is available on runtime nodes through `.xui position: Absolute` and `left`/`top`/`right`/`bottom` insets. Consult the [widget reference](widgets.md#schema-recognized-runtime-unsupported-objects) before relying on a parser-recognized object.

### Sizing and alignment

The backend reports the available window client area in logical pixels. The runtime lays out the `Window` tree in that area, then the painter scales drawing to the physical framebuffer using DPI.

- Put structure and geometry in `.xui`: use `[width: 100%, height: 44px]`, `[grow: 1]`, `[pad: 12px]`, and `[margin: 0x8]`. `size: 800x500` is a unitless width/height pair; `minSize` and `maxSize` use the same pixel pair form. `minWidth`/`maxWidth`/`minHeight`/`maxHeight` also accept percentages. A scalar `pad` or `margin` applies to all edges; a pair such as `8x4` means 8 logical pixels horizontally and 4 vertically.
- Use `Column(gap: 10)` or `Row(gap: 10)` for flow spacing. `CheckList[gap: 8]` sets its vertical item gap. `.xui justify: SpaceBetween` controls main-axis distribution (`Start`, `Center`, `End`, `SpaceBetween`, `SpaceAround`, `SpaceEvenly`).
- Use `.xui align: Center` on a child to override its parent's cross-axis alignment. On a container, `.xui align` sets the default alignment for its children.
- `.xss size`, `min-size`, `max-size`, `min-width`, `max-width`, `min-height`, `max-height`, `grow`, `shrink`, `gap`, `pad`, `margin`, `align`, and `justify` remain supported as layout defaults for compatibility; an authored `.xui` layout value takes precedence. Explicit min constraints apply to leaf widgets and containers alike. On `Split`, pane minima constrain gutter dragging; size the Split itself to at least the sum of its pane floors and gutters to preserve all floors simultaneously.

`px`, `%`, and `dp` are supported for `.xui` width/height/min/max values; unitless values and `px`/`dp` are logical pixels. Percentages resolve against the parent's usable content box after its border and padding. Min/max constraints clamp the computed border-box size after percentage resolution, so combinations such as `width: 80%`, `minWidth: 50%`, and an `.xss max-width: 60%` work together. Use camelCase names in `.xui`; `.xss` supports `size`, `min-size`, `max-size`, and the longhands `min-width`, `max-width`, `min-height`, and `max-height`. For padding and margin, use a scalar logical-pixel value or a unitless horizontal-by-vertical pair. The stylesheet parser accepts more units, but runtime layout does not convert `em`, `rem`, `vw`, `vh`, or `sp`. DPI conversion occurs in text rasterization and painter output, not in authored logical layout sizes.

Auto-sized widgets use measured content and widget-specific preferred sizes for empty controls; `.xui` and `.xss` padding/borders are added by the layout engine. See [preferred sizes for auto-sized widgets](widgets.md#preferred-sizes-for-auto-sized-widgets) for default content-box dimensions and how explicit constraints interact with them.

```xui
Column[class: "content", pad: 20px] {
  Button(label: "Responsive action")[class: "responsive-action", width: 80%, minWidth: 220px, maxWidth: 520px, height: 44px]
}
```

```xss
Column.content { resting: { border: 2px solid #334155 } }
Button.responsive-action { resting: { min-width: 40%, max-width: 600px } }
```

Here the button's percentage width and minimum use the column's content width after the 20 px padding and 2 px border on both sides. If minimum and maximum conflict, the minimum wins, matching the layout solver's clamp rule. A percentage cannot resolve against an indefinite auto-sized parent during intrinsic measurement; it behaves as auto until the parent supplies a definite content size.

### Positioning

Flow is the default. Use `position: Absolute` to remove a child from row/column flow and anchor it to its parent with insets. Insets accept logical pixels or percentages relative to the parent border box; explicit width/height and min/max still constrain the result. Absolute children are intended for deliberate overlays or anchored decorations, so keep their bounds clear of flow content unless overlap is intentional.

```xui
Panel[class: "card", width: 360px, height: 96px, pad: 10px] {
  Text("Flow content")[width: 65%]
  Badge("TOP-RIGHT")[position: Absolute, top: 12px, right: 12px, width: 96px, height: 26px]
}
```

The showcase's Layout tab also demonstrates fixed and percentage sizes, clamped constraints, both XUI and XSS minimum floors, row/column flow, alignment, justification, padding, margins, nesting, and horizontal/vertical `Split` behavior where applicable.

The native window's title and starting size are configured with `XuiOptions`. `Window(title: ..., size: ...)` is parsed and retained as document metadata, but the `Xui` facade currently creates the native window from `XuiOptions`; set both explicitly if you want matching metadata and window configuration.

## Nesting patterns

Use a column for a page, rows for side-by-side controls, and `Split` for resizable panes. Keep spacing in a shared class or tokenized stylesheet:

```xui
Window(title: "Workspace", size: 1100x720) {
  Column[class: "page"] {
    Text("Workspace")[class: "title"]
    Split(direction: Horizontal)[ratio: 0.3, gutter: 8] {
      Panel[class: "sidebar"] { CheckList { CheckItem(label: "Inbox") } }
      Panel[class: "main"] { Text("Select a task")[class: "body"] }
    }
  }
}
```

The first two split children are weighted using `ratio`; further children receive a default share. `gutter` is in logical pixels. Dragging the gutter updates the retained split weights without rebuilding the tree.

## Imports and components

The parser and import loader support relative imports and component declarations:

```xui
@import "shared.xui"

component PageTitle(text) {
  Text(text)[class: "title"]
}
```

`xui::Xui::loadXui(path)` resolves imports and schema-validates the loaded program. However, `UiRuntime::build()` currently materializes only the entry document's supported widget nodes; it does not expand component instances, evaluate bindings/interpolation, or execute `if` / `for`. Use literal widget trees for a working runtime UI. Component and control-flow syntax is parser-level functionality, not a promise of rendered output yet.

## Best practices

- Give every repeated visual pattern a `.xss` class instead of encoding colors or sizes in widget code.
- Keep structure and app geometry in `.xui`; use `.xss` for reusable visual rules. `.xss` layout defaults remain available for legacy styles, but explicit `.xui` values take precedence.
- Add stable IDs only when app code needs to read a control value or target a unique style.
- Keep long body copy in `Text`; it wraps to its measured width and clips to its widget bounds. Give its parent a width constraint and enough vertical space.
- Check widget support in the [object reference](widgets.md) before relying on a parser-accepted name.
- Start from the actual [cross-platform sample](../examples/cross_platform.xui) and [stylesheet](../examples/cross_platform.xss).
