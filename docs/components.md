# Components

Components let an `.xui` file compose existing widgets into a reusable structure. The loader resolves `@import` paths relative to the importing file, detects file cycles, and registers each declared component by its declared name.

```xui
// card.xui
component Card(title, subtitle: "") {
  Panel[class: "card"] {
    Text(title)[id: "title", part: "title"]
    Text(subtitle)[id: "subtitle"]
    Slot()
    Slot(name: "footer") { Text("No actions") }
  }
}
```

```xui
// main.xui
@import "card.xui"

Window {
  Card("Welcome", subtitle: "A reusable section")[id: "welcome", class: "featured"] {
    Text("Caller-owned content")[id: "body"]
    Button(label: "Continue")[slot: "footer", id: "continue"]
  }
}
```

Component names come from `component Name`, not the filename. Parameters can be positional or named and may have literal defaults. The first/default slot receives unmarked caller children. `[slot: "name"]` routes a caller child to `Slot(name: "name")`. A slot may contain fallback children, used when the caller provides no matching child.

The component call's `id` and `class` are applied to its root widget. IDs authored inside the component are prefixed with the call ID: `welcome.title`. IDs authored by the caller inside a slot keep their original names. If the component call has no ID, an internal generated prefix prevents collisions, but that generated name is not a stable application API.

### Styling component parts

Component internals remain ordinary widgets. Mark the internals that consumers should be able to style, then select those parts by the component's declared name:

```xui
component Card(title) {
  Panel[part: "surface"] {
    Text(title)[part: "title"]
    Slot()
  }
}
```

```xss
Card::surface { resting: { bg: #ffffff, pad: 16px, radius: 8px } }
Card::title { resting: { font-size: 22px, font-weight: 600, fg: #172033 } }
```

`Card::title` is shorthand for an internal marker class applied during expansion. It uses normal class matching and cascade, so it can style any widget type marked as that part. A part selector applies to every instance of that declared component; there is no per-instance part selector or descendant/combinator syntax. Use regular `.xss` type/class selectors for broadly shared styles, and expose only stable, intentional parts to avoid coupling consumers to every internal node.

The `part` property is consumed during expansion and does not become a widget attribute. Parts must be declared inside a component definition, using a literal string or name. Widgets supplied by a caller through a slot are owned by that caller and do not automatically inherit the receiving component's part scope.

Event parameters can pass handler names into event properties:

```xui
component ActionButton(label, handler) {
  Button(label)[press: handler]
}
```

```xui
ActionButton("Save", onSave)
```

The expanded `Button` reports the same authored handler name through the existing `UiEventFn` callback. Values and events on internal controls use their namespaced IDs. The runtime does not evaluate arbitrary expressions, bindings, interpolation, `if`, or `for` as part of component expansion. Component parameters must resolve to literal values or handler names, and positional arguments must precede named arguments. Unknown, duplicate, missing, and invalid arguments are rejected during schema validation; the expansion layer also reports a specific error if called without prior validation. Recursive component expansion is rejected with a diagnostic; imported-file cycles are rejected by the existing loader.

The expansion runs before normal runtime construction. Once expanded, layout, `.xss` matching, value APIs, event dispatch, rendering, and backend behavior use the same paths as a hand-authored widget tree.

See [`examples/composites/`](../examples/composites/README.md) for a complete `Card` example. See the [`.xui` guide](xui-guide.md#imports-and-components) for the file grammar and parser limits.
