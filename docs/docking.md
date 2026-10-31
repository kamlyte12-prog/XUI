# Docking panels

`DockHost` organizes direct-child `DockPanel` nodes into recursive split regions, center tab groups, and floating panels. Docking behavior lives in `UiRuntime`; rendering and input continue through the `Painter`, `Renderer`, and `Platform` contracts.

## Authoring

Every panel belongs directly to one host. Give each panel a stable `id` and a title. Use `region` for its initial position (`Left`, `Right`, `Top`, `Bottom`, or `Center`) and `target` to choose the panel/group it relates to. The first non-floating panel becomes the root. Later regions nest around their target; `Center` joins its target's tab group. `floating: true` starts a panel as an in-window overlay.

```xui
Window(title: "Workspace", size: 900x600) {
  DockHost[id: "workspace", class: "workspace", width: 100%, height: 100%,
          gutter: 7px, headerHeight: 30px, minSize: 520x360] {
    DockPanel(title: "Editor")[id: "editor", region: "Center", minSize: 240x180] {
      Text("Editor content")
    }
    DockPanel(title: "Inspector")[id: "inspector", region: "Right", target: "editor",
              minSize: 160x180, closable: true] {
      Text("Inspector content")
    }
    DockPanel(title: "Preview")[id: "preview", region: "Center", target: "editor",
              preferredSize: 320x240, closable: true] {
      Text("Preview is a tab beside Editor")
    }
  }
}
```

`DockSpace` is accepted as an alias for `DockHost`. Host attributes are `gutter` and `headerHeight`. Panel attributes are `title`, `region`, `target`, `floating`, `floatMode`, `nativeTitleBar`, `closable`, `icon`, and `preferredSize`; normal layout attributes such as `minSize`, `maxSize`, and `.xss` `min-size`/`max-size` also apply. `floatMode` is either `"in-app"` (the default) or `"os-window"`. `nativeTitleBar` defaults to `true`; set it to `false` to request an undecorated native tear-off window. When that option is false, the DockPanel header remains the drag handle and displays a close affordance even if `closable` is false. On GLFW/Windows the close affordance closes the panel, and the header moves the native window. Platforms that cannot remove decorations keep the native title bar as a fallback. These are behavioral `.xui` properties, not styles. Percent min/max constraints resolve against the current host content box. Split allocation respects panel floors and ceilings; when a host is smaller than the sum of their minimums, panels keep their minimums and can extend beyond host bounds.

Panel children use the ordinary layout system. The runtime reserves the host's header height inside the DockPanel's top inset; it does not add that height on top of the theme's top padding. The header therefore meets the content area without an extra strip of empty padding. The `.xui` file owns initial placement and child structure; `.xss` owns all chrome appearance.

## Interaction

- Click a tab to activate it. Drag a tab or title header to rearrange a panel.
- Drop near an active group's left/right/top/bottom edge to create a nested split; drop in the center to add the panel to that group's tabs.
- During a valid drag, a `.xss`-styled preview shows the target area. Releasing outside the host floats the panel; `floatMode` selects in-app or native-window hosting. Releasing on an invalid area inside the host or pressing Escape cancels the move.
- Drag split gutters to resize the two adjacent regions. Panel min/max constraints clamp the available ratio.
- Closable panels are removed from the tree when their close button is pressed. There is no built-in reopen command; applications can hide the close button or manage panel visibility outside the current docking value API.

In-app floating panels stay inside the main window. Their title bar follows pointer movement continuously, and the panel is clamped to the DockHost content area so its title bar remains reachable. A floating panel remains a DockHost child in the retained ownership tree so its authored IDs, values, and event handlers stay stable, but its layout position is absolute and DockTree-managed. It does not take part in Row/Column/Stack flow. During paint, its complete subtree is emitted in a window-level overlay pass after ordinary content. Hit testing visits the same floating panels from front to back, so the visible top panel receives input.

With `floatMode: "os-window"`, releasing a panel outside its host creates a separate native window and render surface that shares the primary context. The panel keeps the same retained nodes, authored IDs, values, event handlers, and styles. Its native window can be moved and resized; drag the in-client DockPanel header back over a dock group to redock it. While the pointer crosses the host, the main window paints the current styled `DockPreview`; drop outside a valid host target leaves the panel floating. Closing the native window returns the panel to an in-app floating panel. The host must remain in the same `Xui` instance. A custom backend must implement `Platform::createWindow` with `WindowDesc.shareContextWith`, screen/client coordinate methods, and the optional Renderer surface methods; if it does not provide a usable surface, Xui leaves the panel in-app.

## Stacking and default appearance

Docked panels remain in the normal retained-tree paint and clipping path. In-app floating panel subtrees are removed from that normal paint sequence and emitted in a dedicated global overlay pass, sorted by DockTree z-order from back to front. This prevents later siblings or ancestor ScrollView clips from cutting through a floating panel. The overlay clips to the window viewport, the floating panel bounds, and ScrollViews inside that panel; outer layout/scroll ancestors do not clip it. Input follows the same rule: the topmost floating panel is tested first, with its own bounds and internal scroll clips, and can receive clicks above the docked content below. Dock headers, tab strips, splitter handles, and drag previews paint after panel content; popup controls retain their existing overlay priority.

The built-in Fluent 2-inspired defaults give DockPanel a neutral outline and an elevated shadow only while it is floating. The header has a pale blue surface and accent text; DockTabs use the same surface, with stronger blue text and an accent outline for the selected tab. Selected-state defaults do not replace an application's resting header/tab background, so a blue or dark `.xss` theme remains intact. The top corners follow the panel radius and the bottom corners are square, joining the header to the panel body. Splitters use a neutral gutter that accents on hover/drag, and close buttons turn red on hover. These are normal `.xss` rules (`DockPanel`, `DockHeader`, `DockTab`, `DockCloseButton`, `DockSplitter`, `DockPreview`) and can be overridden by application styles. Header and tab rules resolve background, foreground, border, radius, opacity, padding, font family/size/weight, and text alignment through the regular cascade. `DockHeader:floating` is supplied for in-app floating panels; it changes title weight without forcing a separate light background. Hover, selected, pressed, disabled, and dragging states are supplied where applicable. Close affordances and the drop preview have their own selectors. `DockHeader:selected` applies to an active single-panel header; `DockTab:selected` applies to the active tab in a tab group.

The current benchmark target reports CPU microseconds for idle layout, drag preview, splitter movement, painting, tab switching, undock/re-dock value operations, and close input on prebuilt runtimes. These values isolate runtime work; they do not represent displayed FPS or GPU/presentation cost. Visible drag, resize, and interaction frame pacing still needs a desktop smoke run.

## Styling

The runtime resolves each dock subpart through ordinary type selectors and the current theme. A subpart selector can also use the source DockPanel's ID/classes. Supported parts:

| Selector type | Applied to |
|---|---|
| `DockHeader` | Single-panel title bar and floating-panel title bar |
| `DockTab` | Each tab in a multi-panel group; uses `:hover`, `:selected`, `:pressed`, and `:disabled` |
| `DockCloseButton` | Close affordance; uses `:hover`, `:pressed`, and `:disabled` |
| `DockSplitter` | Split gutter; uses `:hover`, `:pressed`, and `:dragging` |
| `DockPreview` | Live drop target; uses `:dragging` |

Use regular `DockHost` and `DockPanel` selectors for the host surface and panel contents.

```xss
DockHost.workspace { resting: { bg: #172235, pad: 4px, border: 1px solid #33465f, radius: 7px } }
DockPanel { resting: { bg: #172235, fg: #edf3fb, pad: 10px, font-size: 13px } }
DockHeader { resting: { bg: #202f46, fg: #edf3fb, pad: 8px, radius: 6px 6px 0px 0px }
  hover: { bg: #293d59 } }
DockTab { resting: { bg: #101827, fg: #a6b5c9, pad: 8px, radius: 6px 6px 0px 0px }
  hover: { bg: #293d59, fg: #edf3fb }
  selected: { bg: #202f46, fg: #69a7ff } }
DockCloseButton { resting: { fg: #a6b5c9 }
  hover: { bg: #9c3e52, fg: #ffffff } }
DockSplitter { resting: { bg: #33465f }
  hover: { bg: #69a7ff } pressed: { bg: #91bdff } }
DockPreview { resting: { bg: #69a7ff55, border: 1px solid #69a7ff, radius: 5px } }
```

Dock chrome uses the same XSS palette/theme resolution as the rest of the runtime. `align` controls the title label's horizontal alignment; `font-family`, `font-size`, and `font-weight` control its face; `fg`, `bg`, `border`, `radius`, `opacity`, and `pad` control the subpart appearance. Radius accepts the usual one-to-four value shorthand; for an integrated panel header, match the panel's top corners and set the bottom radii to zero, for example `radius: 6px 6px 0px 0px`. Layout rules such as `min-size` belong to the host/panel selector, not the synthetic chrome subparts.

Native tear-off rendering paints only the selected DockPanel subtree and its DockHeader on the separate surface. It uses the same `.xss` style resolution as docked chrome, clips to the panel bounds and its internal ScrollViews, and ignores clips from the former outer layout ancestors. Other main-window widgets are excluded from the tear-off draw list.

## Values and events

`UiRuntime::value(panelId)` returns a `DockPanelValue` for an open panel:

- `region`: current root-level region (`Center`, `Left`, `Right`, `Top`, `Bottom`, or `Floating`). Nested panels report the side of the first split below the host root.
- `floating`, `bounds`, `order`, and `active`.
- `host` ID and `activeTab` ID for the containing dock group.

Bounds use logical pixels relative to the DockHost content box. `setValue(panelId, DockPanelValue)` can float/move a panel, dock it into `targetPanel` using `region`, reorder a group member, or activate it. A closed panel currently has no value and cannot be reopened through `setValue`.

Panel event properties are `dock`, `undock`, `resize`, `tabChange`, and `close`. Handlers receive the authored event name through the standard `UiEventFn`; query `value(id)` in the callback to read the updated state. Splitter resize callbacks currently notify each panel in the host because the event API does not include a source ID.

## Limits and performance notes

- A DockHost currently accepts only DockPanel as a direct child. Nesting comes from recursive split regions, not nested DockHost widgets.
- Pointer movement updates the docking tree and relayouts only that host subtree; it does not rebuild the authored document. Dragging and splitter movement avoid allocations in the model hot path.
- The runtime does not provide keyboard docking commands, drag payloads between separate hosts, persistence/serialization, or restoration of closed panels. Native tear-off is implemented for the GLFW/OpenGL adapters. The Windows Release GLFW demo launched its five-second docking profile at 357.573 FPS, and headless tests cover surface-scoped painting and redocking state; native tear-off, movement, resize, close, and redock interactions have not been visually exercised. The screenshot's 4 FPS remains unreproduced. Linux/macOS native behavior and mixed-DPI monitor dragging have not been verified. A backend without surface support falls back to in-app floating.
- Automated tests cover tree topology, min/max clamping, nested splits, tabs, floating, hit interactions, and API restoration. Visible drag preview, cursor, native resize coexistence, and >60 FPS during user-driven docking still require a desktop smoke test; do not infer those claims from unit tests alone.
