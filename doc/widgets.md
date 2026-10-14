# UI object reference

This catalog describes objects that `UiRuntime` currently materializes and paints. The `.xui` parser/schema recognizes additional names, but those names are not working runtime controls yet; the unsupported list at the end prevents confusing validation with implementation.

Common bracket properties available through the schema include `id`, `class`, `disabled`, and event names. Styling comes from `.xss` selectors matching the type, class, or id.

## Containers and layout

### `Window`

The single required root. It fills the logical client area and lays children out vertically.

```xui
Window(title: "Tasks", size: 900x600) {
  Column[class: "page"] { Text("Content") }
}
```

`title` and `size` are retained by the runtime document, but `XuiOptions` configures the native window created by the facade. The schema accepts `resizable`; native resize behavior also comes from `XuiOptions`. Style `Window` for background and text defaults.

### `Column` and `Row`

Vertical and horizontal flex-like flow containers. The schema accepts `gap` and `wrap` arguments/properties, but current arrangement uses `.xss gap` and does not wrap the flow. Use `.xss pad`, `align`, and `justify` to control arrangement; put `grow` on children that should take remaining main-axis space.

```xui
Row[class: "toolbar"] {
  Button(label: "Open")[class: "tool"]
  Button(label: "Save")[class: "tool"]
}
```

### `Panel`

A vertical container commonly used as a styled surface. It is not independently draggable, dockable, or collapsible in the current runtime. The schema accepts `title`, `collapsed`, and `collapsible`, but the runtime does not implement those behaviors or paint the title argument; add a `Text` child for a visible heading.

```xui
Panel[class: "card"] {
  Text("Details")[class: "panel-title"]
  Text("Panel content")[class: "body"]
}
```

### `CheckList`

A vertical container for `CheckItem` children. The schema accepts a `gap` property; set spacing through `.xss gap` for current runtime behavior.

```xui
CheckList[class: "checklist"] {
  CheckItem(label: "Parse layout", checked: true)[class: "item"]
  CheckItem(label: "Apply stylesheet")[class: "item"]
}
```

### `Split`

A layout container with a pointer-draggable gutter. `direction` is `Horizontal` (side-by-side) or `Vertical` (stacked). `ratio` sets the first pane's initial share and `gutter` is logical pixels. The schema accepts `collapsible`, but the runtime does not collapse panes. It is the framework's current resizable-panel primitive.

```xui
Split(direction: Horizontal)[ratio: 0.35, gutter: 8] {
  Panel[class: "sidebar"] { Text("Navigation") }
  Panel[class: "main"] { Text("Workspace") }
}
```

## Text and action controls

### `Text`

Displays literal text, wraps at the available content width, advances each line vertically, and clips glyphs to its box. It accepts `wrap` and `maxLines` in the schema, but these options are not currently evaluated; wrapping follows available width. Use a width constraint and a font loaded through `Xui::loadFont()`.

```xui
Text("A long paragraph wraps inside the content column.")[class: "body"]
```

### `Button`

Displays a label, supports hover/pressed/focus/disabled styling, pointer activation, and keyboard activation when focused. Use `press: handlerName` for a callback.

```xui
Button(label: "Save", press: onSave)[class: "primary", align: Center]
```

The runtime does not implement icon, loading, or variant rendering even though those names pass schema validation.

### `CheckItem`

A checkbox row with a label and retained checked state. Clicking or pressing Space/Enter toggles it. The runtime exposes visual checked state but does not yet expose a general checklist value/binding API.

```xui
CheckItem(label: "Release is ready", checked: false)[class: "check-item"]
```

### `CheckBox`

A labeled single checkbox. `checked` sets its initial value; the authored `id` makes it readable and writable through `UiRuntime::value()` / `setValue()`. Pointer click, Space, and Enter toggle it and invoke `change`.

```xui
CheckBox(label: "Enable notifications")[id: "notifications", checked: true, change: onNotifications, class: "checkbox"]
```

Style it through `CheckBox` selectors and the `hover`, `focus`, `checked`, and `disabled` states. It has no indeterminate state or native accessibility bridge yet.

### `Switch`

An on/off toggle rendered as a track with a sliding knob and an optional label to its right. `checked` sets the initial state; the authored `id` exposes a `bool` through `value()` / `setValue()`. Clicking toggles on release, Space/Enter toggle when focused, and both invoke `change`.

```xui
Switch[label: "Wi-Fi", checked: true, change: onWifi][id: "wifi", class: "switch"]
```

The on-state track takes the control's `fg`; the unchecked track and the knob are fixed colors. The label uses the normal text properties and is offset clear of the track automatically. Without an explicit size and label the widget measures 44x24; with a label the text adds to the intrinsic width. There is no indeterminate state.

```xss
Switch.switch { resting: { size: 64px 32px, fg: #2563eb, font-size: 14px } hover: { fg: #3b82f6 } focus: { border: 1px solid #70a7ff } checked: { fg: #2563eb } disabled: { opacity: 0.45 } }
```

### `ToggleButton`

A push button with a persistent checked state, for on/off toolbar actions. `ToggleButton("Mute")` or `ToggleButton[label: "Mute", checked: true, change: onMute]`. Clicking toggles `checked` on release and invokes `change`; Space/Enter toggle when focused. Text is centered like `Button`, and the `pressed`, `checked`, `focus`, and `disabled` states all style it.

```xss
ToggleButton.tool { resting: { size: 40px 32px, bg: #1d293b, fg: #dce6f8, radius: 6px } hover: { bg: #2b3d58 } checked: { bg: #2563eb, fg: #ffffff } disabled: { opacity: 0.45 } }
```

The schema accepts `icon`, but the runtime does not render icons yet.

### `RadioGroup`

A single-select group of options rendered as radio rows inside one control. Supply pipe-separated `options`, an optional initial `value` (an option string), and `change: handler`. Clicking a row selects it; Up/Down/Left/Right move the selection when the group is focused. Hovering highlights the row under the pointer with a row-local tint, like dropdown rows. The authored `id` reads and writes the selected option string.

```xui
RadioGroup[id: "layout", options: "Compact|Comfortable|Spacious", value: "Comfortable", change: onLayout][class: "radio"]
```

Rows are painted by the control rather than authored as `.xui` children, so per-row selectors are not available. `fg` colors the ring, the selection dot, the row hover tint, and the option labels; the selected ring draws at two pixels.

```xss
RadioGroup.radio { resting: { size: 240px 108px, fg: #60a5fa, font-size: 14px, pad: 6px } hover: { fg: #93c5fd } focus: { border: 1px solid #70a7ff } disabled: { opacity: 0.45 } }
```

### `TextBox`

A single-line editable UTF-8 field. It supports initial `value`, `placeholder`, `maxLength` (Unicode code points), `readOnly`, `password`, `disabled`, `tooltip`, and `change`. Clicking focuses it; text input inserts UTF-8 text, including spaces. Backspace/Delete erase the previous/next code point, Shift+Left/Right/Home/End select text, and Ctrl+A/C/X/V select all, copy, cut, and paste through the platform clipboard interface. Typing or deleting while a selection exists replaces/removes that selection. Tab always moves keyboard focus and never inserts a tab character into a field; multi-line entry belongs in `TextArea`. A focused editable field shows a blinking caret: it blinks while the field is idle, stays solid while typing or moving the caret, and is hidden for read-only or disabled fields. An empty field activates the `.xss` `placeholder` state. Its authored `id` reads/writes a `std::string` through `value(id)` / `setValue(id, value)`.

For debugging text layout, `UiRuntime::setWhitespaceVisible(true)` renders spaces as a middle dot (·) and tabs as a right arrow (→) inside editable fields; caret and selection geometry are measured through the same substitution, so positions stay aligned with the drawn glyphs. The showcase demo exposes this as a checkbox.

```xui
TextBox[id: "email", value: "", placeholder: "name@example.com", maxLength: 120,
       tooltip: "Use your work email", change: onEmailChanged, class: "field"]
```

Style the field's size, padding, font, foreground/background, border, radius, and `hover`, `focus`, `disabled`, or `placeholder` state in `.xss`. Text and selection are clipped to the content box; the caret scrolls horizontally when needed. User edits invoke `change`; programmatic `setValue` is silent unless `notify=true`. Mouse caret placement, drag selection, word navigation, undo, IME preedit display, validation, and native accessibility are not implemented. Password mode masks the display only; it does not encrypt stored text.

```xss
TextBox.field {
  resting: { size: 280px 40px, pad: 9px, bg: #172033, fg: #f5f7ff, border: 1px solid #40506b, radius: 6px, font-size: 14px }
  hover: { border: 1px solid #7189b4 }
  focus: { border: 2px solid #70a7ff }
  disabled: { opacity: 0.45 }
  placeholder: { fg: #929db1 }
}
```

### `TextArea`

A multiline editable field with the same `.xui` values and state styles as `TextBox`. Text wraps to its styled content width; Enter inserts a newline, and Backspace at the beginning of a line merges it with the previous line. Keyboard selection and clipboard editing follow `TextBox`. Its `id` exposes a `std::string`; user edits invoke `change`. This is a basic text area rather than a rich text editor.

```xui
TextArea[id: "notes", placeholder: "Add notes…", maxLength: 2000,
        tooltip: "Keep the summary concise", change: onNotesChanged, class: "notes"]
```

Style size, padding, font, foreground/background, border, radius, and shared control states in `.xss`; text and selection are clipped to the field. Undo, IME preedit display, drag selection, and mouse caret positioning remain future work.

```xss
TextArea.notes { resting: { size: 420px 140px, pad: 10px, bg: #172033, fg: #f5f7ff, border: 1px solid #40506b, radius: 6px, font-size: 14px } focus: { border: 2px solid #70a7ff } placeholder: { fg: #929db1 } }
```

### `NumberField`

A single-line numeric editor built on the `TextBox` editing path (caret, selection, clipboard, `readOnly`, `disabled`) with numeric behavior. `value`, `min`, and `max` set the initial and clamped number; `step` (default 1) is the Up/Down increment; `precision` (0–6, default 0 = shortest round-trip formatting) formats stepped and programmatic values; `unit` is appended to formatted text; `change: handler` fires whenever the committed value changes.

```xui
NumberField[id: "delay", value: 250, min: 0, max: 2000, step: 50, unit: "ms", change: onDelay][class: "number"]
```

Typing is filtered to numeric characters (`0-9`, `.`, `-`, `+`, `e`, `E`, spaces). Every valid edit commits immediately and fires `change`; a partially typed value (for example a lone `-`) stays in the editor without changing the value. Up/Down step by `step` and reformat the text clamped to `min`/`max` when provided. `value(id)` returns the committed `f64`; `setValue(id, f64)` clamps, reformats with the unit, and is silent unless `notify=true`; `setValue(id, std::string)` parses and commits like typed input. Invalid entries never change the value, and the editor keeps what you typed until a step or programmatic set reformats it.

```xss
NumberField.number { resting: { size: 140px 36px, pad: 8px, bg: #172033, fg: #f5f7ff, border: 1px solid #40506b, radius: 6px, font-size: 14px } focus: { border: 2px solid #70a7ff } disabled: { opacity: 0.45 } readonly: { fg: #929db1 } }
```

### `SearchField`

A single-line query field: `TextBox` semantics (placeholder, UTF-8 input, selection, clipboard, `maxLength`, `readOnly`) plus a clear affordance. When the field has text, an `×` glyph is drawn at the right edge; clicking it, or pressing Escape while focused, clears the field and fires `change`. Every edit fires `change` like `TextBox`.

```xui
SearchField[id: "query", placeholder: "Search widgets…", change: onQuery][class: "search"]
```

Style it with `SearchField` selectors; the reserved clear region (about 18 logical pixels) is carved out of the text area automatically, and the caret never runs under the glyph.

```xss
SearchField.search { resting: { size: 280px 36px, pad: 9px, bg: #172033, fg: #f5f7ff, border: 1px solid #40506b, radius: 18px, font-size: 14px } focus: { border: 2px solid #70a7ff } placeholder: { fg: #929db1 } }
```

### `Tabs` and `Tab`

`Tabs` accepts an initial zero-based `selected` index, `disabled`, `id`, and `change`; its only children are `Tab` pages. `Tab` requires a title and contains the page subtree. Headers have equal width. Click a header or focus Tabs and press Left/Right to switch pages. Use the Tabs `id` to read or set a `u32` selected index.

```xui
Tabs[id: "settings", selected: 0, change: onTabChanged, class: "tabs"] {
  Tab(title: "General") { Text("General settings") }
  Tab(title: "Advanced") { Text("Advanced settings") }
}
```

Style Tabs and Tab headers with the shared `hover`, `focus`, `disabled`, and `selected` states. Inactive page descendants do not paint, hover, receive focus, or receive pointer input. Headers do not scroll, reorder, close, or lazy-load pages.

```xss
Tabs.settings { resting: { size: 520px 340px, bg: #111827, radius: 8px } focus: { border: 1px solid #70a7ff } }
Tab { resting: { bg: #1d293b, fg: #dce6f8, font-size: 14px } hover: { bg: #2b3d58 } selected: { bg: #344b6b, fg: #ffffff } disabled: { opacity: 0.45 } }
```

### Tooltips

Add a string `tooltip: "…"` value to a runtime node. Tooltip behavior is configured on that `.xui` target because trigger and delay are interaction behavior; the bubble's colors, font, border, padding, and radius remain in `.xss`. Defaults are a 500 ms show delay, 120 ms hide delay, hover and focus triggers, hide-on-click enabled, and per-element enablement.

```xui
Button(label: "Publish")[tooltip: "Publish the current draft", tooltipShowDelay: 300,
  tooltipHideDelay: 150, tooltipTrigger: "hover", tooltipHideOnClick: true,
  tooltipDisabled: false, press: onPublish, class: "primary"]
```

```xss
Tooltip { resting: { bg: #202a40, fg: #f2f6ff, font-size: 13px, pad: 7px, radius: 5px } }
```

`tooltipShowDelay` and `tooltipHideDelay` are milliseconds, clamped to 0–60,000. `tooltipTrigger` accepts `"hover"`, `"focus"`, `"both"`, or `"none"`. `tooltipHideOnClick` and `tooltipDisabled` are booleans. Setting `tooltipDisabled: true` prevents this target's tooltip from showing. A click hides a visible tooltip immediately when hide-on-click is enabled; the current target stays suppressed until it loses hover and focus. Popups suppress tooltip painting so an open dropdown/color picker stays unobscured. Tooltips are backend-neutral pointer-transparent overlays whose placement is clamped to the window; they are not standalone child nodes. Rich content and per-tooltip selectors are not available yet.

## Display controls

### `Label`

A static text leaf, equivalent to `Text` under a different semantic name: `Label("Username")` or `Label[text: "Username"]`. It wraps at the available content width, clips to its box, supports the shared text properties, and blocks pointer input like `Text`. Its authored `id` reads and writes the string through `value()` / `setValue()` / `controlText()`.

### `Badge`

A short text leaf for counts and status words: `Badge("New")` or `Badge[text: "3"]`. It renders exactly like other text leaves; give it `bg`, `fg`, `radius`, `pad`, and `align: Center` in `.xss` for the pill look. It participates in layout as an ordinary leaf and exposes its text through the value API.

```xss
Badge.tag { resting: { bg: #2563eb, fg: #ffffff, size: auto 22px, pad: 7px 2px, radius: 11px, font-size: 12px, align: Center } }
```

### `Separator` and `Divider`

Aliases for the same control: a thin line drawn in the control's `fg`. The intrinsic size is 12x1; inside a `Column` the default cross-axis stretch spans the container width, and inside a `Row` give it an explicit `size` (for example `size: 1px 32px` draws a vertical divider). Orientation follows the painted aspect: a region wider than tall draws horizontally.

```xss
Separator.rule { resting: { fg: #33415560, margin: 8px 0px } }
```

### `Image`

Paints a PNG file: `Image("assets/logo.png")` or `Image[source: "assets/logo.png", fit: Cover]`. The file is read once at first paint, uploaded as an RGBA8 texture through the `Renderer` interface, drawn aspect-correct per `fit` (`Fill` stretches, `Contain` — the default — letterboxes, `Cover` fills and center-crops), and clipped to the widget with optional `radius`. Missing sources, unreadable files, or headless clients without a renderer draw an outlined placeholder box instead of failing.

> **Decoder limit:** `Image` currently reads only the uncompressed PNG variant this project writes (`xui::writePng`: 8-bit RGBA, filters 0/2, stored or fixed-Huffman deflate blocks). Compressed PNGs from other tools fall back to the placeholder until a general decoder lands. Do not point `Image` at arbitrary assets yet. The showcase demo generates its own image with `writePng` at startup (see `examples/glfw_demo.cpp`) and reads it back through this path.

Without an explicit size, a source-less image measures 96x64 and a loaded image takes its natural pixel size as logical pixels.

### `Spinner`

An activity indicator: eight radial ticks in the control's `fg` with a fading tail, rotating one step per 80 ms timer tick. It animates only while enabled and visible and uses the runtime's idle-event timer, so it never forces continuous redraws. The intrinsic size is 18x18; scale it with `size`. A disabled spinner stops animating and picks up the `disabled` state styles.

```xss
Spinner.busy { resting: { size: 24px 24px, fg: #60a5fa } }
```

## Stacking and hit testing

Normal widgets paint in `.xui` tree order: a parent paints before its children, and later siblings paint above earlier siblings. Hit testing walks that order in reverse, stops at the first visible painted hit surface, and updates at most one target's hover, focus, press, or activation state. Visible controls, text, and panels block widgets behind them, including disabled controls. Transparent `Row` and `Column` containers pass pointer input through. Inactive tabs are excluded.

Open dropdown and color-picker popups sit above the tree and tab headers and receive pointer input inside their popup bounds. Clicking outside closes an open popup, then the exposed target receives that click. Tab headers sit above ordinary tree content. Tooltips are pointer-transparent and hidden while a popup is open. `Label`, `Badge`, `Spinner`, and `Image` nodes block pointer input like text and panels. RadioGroup rows and the SearchField clear region are part of their control: a click inside the group's bounds selects the row under the pointer, and a click on the clear region clears the field. Arbitrary `.xss z-index` and general container clipping are not implemented; pointer coordinates are clipped to the window, and inactive tabs are filtered consistently with painting.

## Reading and changing values

Stateful controls with an authored `id` expose a typed `std::optional<UiValue>` from `ui.runtime().value(id)`. The alternatives are `bool` for `CheckItem` / `CheckBox` / `Switch` / `ToggleButton`, `f64` for `Slider` / `NumberField`, `std::string` for `Dropdown` / `RadioGroup` / `SearchField` / `TextBox` / `TextArea` / `Label` / `Badge` / identified `Text` nodes, `Color` for `ColorPicker`, and `u32` for the selected `Tabs` index. `setValue(id, value)` updates and redraws a control; `notify` defaults to `false`. User interaction invokes the authored `change` callback, where the application can read the current value.

```cpp
if (const auto value = ui.runtime().value("email"); value && std::holds_alternative<std::string>(*value)) {
  const std::string& email = std::get<std::string>(*value);
}
ui.runtime().setValue("notifications", true); // silent programmatic update
```

Use unique authored IDs; duplicate IDs are not a supported lookup pattern.

### `Slider`

A single-value range control. It supports pointer dragging, Tab focus, Left/Right keyboard steps, `value`, `min`, `max`, `disabled`, and `change: handler`. `step` passes schema validation, but keyboard movement currently uses a 5% range step. A change handler fires on pointer-down if the value changes, once for every distinct logical value reached while dragging, and once for each keyboard step. Pointer-up does not send a duplicate final change event. The value API always returns the current `f64`.

```xui
Slider[value: 0.4, min: 0, max: 1, change: onVolume][class: "slider"]
```

The host callback can inspect `ui.runtime().value("volume")` for the current value. To show a live readout without adding data binding, give a `Text` node an ID and update it from the callback:

```xui
Slider[id: "volume", value: 0.4, min: 0, max: 1, change: onVolume][class: "slider"]
Text("Volume: 40%")[id: "volume-label"]
```

```cpp
if (const auto value = ui.runtime().value("volume")) {
  const auto amount = std::get<xui::f64>(*value);
  ui.runtime().setValue("volume-label", std::to_string(int(amount * 100.0 + 0.5)) + "%");
}
```

### `ProgressBar`

A display-only range bar with `value`, `min`, and `max`. It renders a track and proportional fill. The `indeterminate` schema property is not implemented; it does not animate or bind to another control.

```xui
ProgressBar[value: 0.72, min: 0, max: 1][class: "progress"]
```

### `FpsCounter`

A text object that reports frame rate and average frame time over the same sample window. It displays `FPS: 120 | 8.3 ms` by default. Set `showFrameTime: false` for FPS-only display. Press **F3** to toggle the counters; while enabled, the label samples and refreshes every 250 ms. In an otherwise idle event-driven app, this schedules at most four overlay frames per second. Input and resize events still trigger prompt redraws. Continuous profile mode does not add an FPS timer wait or force redraws on every loop iteration.

```xui
FpsCounter[class: "fps"]
FpsCounter[showFrameTime: false, class: "fps-compact"]
```

## Popup controls

### `Dropdown`

A single-select dropdown. Supply pipe-separated static choices through `options`, an optional initial `value`, and `change: handler`. Click or Enter/Space opens it; click selects; Up/Down changes a focused selection; Escape closes it. Header hover applies only while the pointer is over the header. Popup rows have an independent row-local hover highlight; they do not inherit the header's `.xss` hover style. The header's resting border remains, and its `open` / `focus` styles can keep an outline visible while the pointer moves over options. Popup rows are virtual runtime targets, not `.xui` child nodes, so per-option selectors and option-specific styling are not available. The schema accepts `placeholder`, but the runtime does not display it.

```xui
Dropdown[id: "density", options: "Compact|Comfortable|Spacious", value: "Comfortable", change: onDensity][class: "dropdown"]
```

Read the selected string with `ui.runtime().value("density")` (or the older `controlText("density")`) in a facade event callback. The list is static and pipe characters cannot be part of an option value. There is no data binding or scrolling for long option lists yet.

```xss
Dropdown.dropdown { resting: { size: 240px 38px, pad: 8px, bg: #172033, fg: #f5f7ff, border: 1px solid #40506b, radius: 6px } hover: { bg: #202d43 } focus: { border: 2px solid #70a7ff } open: { border: 2px solid #70a7ff } disabled: { opacity: 0.45 } }
```

### `ColorPicker`

An in-app HSV popup with a saturation/value field, hue strip, optional alpha strip, active-format display, format switcher, and editable color string. It is backend-neutral painted UI and does not invoke a native color dialog. Click/drag changes values immediately; Escape closes the popup. Clicking the format label cycles the configured formats. Click the value row and type, edit, copy, cut, or paste a supported color string; valid complete values apply immediately, while invalid/incomplete strings stay in the editor without changing the current color.

```xui
ColorPicker[id: "accent", value: #5b8cff, format: "hex", formats: "hex|rgb|hsl|hsv",
  popupSize: 240x220, precision: 2, alpha: true, showAlpha: true, showHue: true, showSaturationValue: true,
  showInputs: true, showFormatSwitcher: true,
  change: onAccent][class: "color-picker"]
```

`format` selects the initially displayed format (`hex`, `rgb`, `hsl`, or `hsv`); `formats` is a pipe-separated list used by the switcher and its order. `popupSize: 240x220` requests the popup dimensions (the runtime enforces a minimum of 196x204 for its built-in controls). `precision` rounds decimal components to 0–4 places. `alpha` defaults to true and forces alpha to 1 when false; `showAlpha` controls whether the alpha strip is visible. `showHue` and `showSaturationValue` hide their respective HSV surfaces; `showInputs` and `showFormatSwitcher` control the typed value row and format label. Hex input accepts `#rgb`, `#rgba`, `#rrggbb`, and `#rrggbbaa`; functional input accepts comma-separated `rgb(a)`, `hsl(a)`, and `hsv(a)` values, including percentage components for saturation/lightness/value. Named colors, CSS space-separated syntax, an inline layout, separate numeric fields, custom labels/placeholders, and individually themed hue/alpha sub-controls are not implemented yet. `.xss` controls the widget/popup surface and text through normal `size`, `bg`, `fg`, `border`, `radius`, `pad`, and state declarations; the hue strip remains the standard spectrum.

Read the canonical backend-neutral `Color` (RGBA float components) with `ui.runtime().value("accent")` or `controlColor("accent")`. `controlText("accent")` returns the active formatted string. `setValue("accent", Color{...})` and `setValue("accent", std::string("hsl(210, 60%, 50%)"))` are supported; invalid strings return `false` and leave the color unchanged. The optional `change` callback is fired for live pointer changes and live valid typed changes. Clipboard copy/paste uses the platform interface.

```xss
ColorPicker.color-picker { resting: { size: 240px 38px, pad: 8px, bg: #172033, fg: #f5f7ff, border: 1px solid #40506b, radius: 6px } hover: { bg: #202d43 } focus: { border: 2px solid #70a7ff } open: { border: 2px solid #70a7ff } disabled: { opacity: 0.45 } }
```

## Shared style example

```xss
Panel.card { resting: { bg: #172033, pad: 18px, border: 1px solid #344563, radius: 10px } }
Button.primary {
  resting: { bg: #2563eb, fg: #fff, size: 180px 42px, radius: 7px }
  hover: { bg: #3b82f6 }
  pressed: { opacity: 0.85 }
  focus: { border: 2px solid #93c5fd }
  disabled: { opacity: 0.45 }
}
CheckItem.check-item { resting: { fg: #f8fafc, size: 260px 36px } hover: { bg: #263752 } checked: { fg: #60a5fa } }
```

The full property/value table is in the [styling reference](styling-reference.md). The default [showcase](../examples/showcase.xui) and [stylesheet](../examples/showcase.xss) exercise the complete runtime-supported object set: the Input tab covers `TextBox`/`TextArea`/`CheckBox` plus `SearchField`/`NumberField`, the Controls tab covers `Button`/`Slider`/`ProgressBar`/`Dropdown`/`ColorPicker` plus `Switch`/`ToggleButton`/`RadioGroup`, and the Layout tab covers `Split`/`CheckList` plus a display section with `Separator`, `Image`, `Label`, `Badge`, and `Spinner`. The earlier [cross-platform sample](../examples/cross_platform.xui) and focused [controls sample](../examples/controls.xui) remain available as smaller references.

## Schema-recognized, runtime-unsupported objects

The current core schema also recognizes names that are not materialized by `UiRuntime::build()`: `Grid`, `Stack`, `ScrollView`, `Toolbar`, `MenuBar`, `StatusBar`, `Menu`, `MenuItem`, `Popover`, `Modal`, `ContextMenu`, `DockSpace`, `IconButton`, `RangeSlider`, `ComboBox`, `DatePicker`, `KeybindField`, `Icon`, `TreeView`, `ListView`, `Table`, `Canvas`, `HUD`, `Crosshair`, `RadialMenu`, `HealthBar`, and `DamageNumber`.

Use only the supported object list above in a document passed to `Xui::attach()`. Parser/schema entries are retained for future implementation and language validation; they are not native controls or hidden fallbacks.
