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

The parser recognizes `press`, `release`, `click`, `doubleClick`, `change`, `submit`, `focus`, `blur`, `hover`, `scroll`, `drag`, `drop`, `key`, `resize`, and `close`. The current runtime wires `press` on buttons and `change` on slider/dropdown/color-picker controls to the same callback mechanism. It passes the handler name, not a value payload. Slider changes are reported immediately for each distinct pointer or keyboard value. Read a dropdown or color value through `UiRuntime::value(id)`, `controlText(id)`, or `controlColor(id)` inside the callback; color picker format/options are described in the [widget guide](widgets.md#popup-controls). Other parser event names are not dispatched by this runtime.

The common validator accepts generic names including `id`, `class`, `key`, `slot`, `grow`, `shrink`, `visible`, `disabled`, `tooltip`, `tooltipShowDelay`, `tooltipHideDelay`, `tooltipTrigger`, `tooltipHideOnClick`, `tooltipDisabled`, `align`, `justify`, `pad`, `margin`, `size`, `minSize`, `maxSize`, `width`, `height`, and `opacity`. Tooltip timing/trigger are behavior properties in `.xui`; bubble appearance belongs to `.xss`. The runtime applies structural geometry properties `grow`, `shrink`, `align`, `justify`, `pad`, `margin`, `size`, `minSize`, `maxSize`, `width`, and `height` from `.xui`; Row/Column also accept a named `gap` constructor argument, and CheckList accepts `gap` as a property. `.xui` values override the same `.xss` layout defaults. `visible` and `.xui opacity` are still not implemented as runtime state/paint behavior; use `.xss opacity` for visual opacity.

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

The layout engine also has lower-level `Grid`, `Stack`, `Scroll`, and absolute-layout capabilities, but the runtime does not currently materialize the corresponding `.xui` widgets. They are not available to the ordinary document path yet.

### Sizing and alignment

The backend reports the available window client area in logical pixels. The runtime lays out the `Window` tree in that area, then the painter scales drawing to the physical framebuffer using DPI.

- Put structure and geometry in `.xui`: use `[width: 100%, height: 44px]`, `[grow: 1]`, `[pad: 12px]`, and `[margin: 0x8]`. `size: 800x500` is a unitless width/height pair; `minSize` and `maxSize` use the same pair form. A scalar `pad` or `margin` applies to all edges; a pair such as `8x4` means 8 logical pixels horizontally and 4 vertically.
- Use `Column(gap: 10)` or `Row(gap: 10)` for flow spacing. `CheckList[gap: 8]` sets its vertical item gap. `.xui justify: SpaceBetween` controls main-axis distribution (`Start`, `Center`, `End`, `SpaceBetween`, `SpaceAround`, `SpaceEvenly`).
- Use `.xui align: Center` on a child to override its parent's cross-axis alignment. On a container, `.xui align` sets the default alignment for its children.
- `.xss size`, `min-size`, `max-size`, `grow`, `shrink`, `gap`, `pad`, `margin`, `align`, and `justify` remain supported as layout defaults for compatibility; an authored `.xui` layout value takes precedence.

`px`, `%`, and `dp` are supported for `.xui` width/height values; unitless values and `px`/`dp` are logical pixels. Percentages are resolved once against the containing block. For padding and margin, use a scalar logical-pixel value or a unitless horizontal-by-vertical pair. The stylesheet parser accepts more units, but runtime layout does not convert `em`, `rem`, `vw`, `vh`, or `sp`. DPI conversion occurs in text rasterization and painter output, not in authored logical layout sizes.

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
