# Docking panels

`DockHost` organizes direct-child `DockPanel` nodes into recursive split regions, center tab groups, and floating overlays inside the same window. Docking behavior lives in `UiRuntime`; rendering and input continue through the existing `Painter`, `Renderer`, and `Platform` contracts.

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

`DockSpace` is accepted as an alias for `DockHost`. Host attributes are `gutter` and `headerHeight`. Panel attributes are `title`, `region`, `target`, `floating`, `closable`, `icon`, and `preferredSize`; normal layout attributes such as `minSize`, `maxSize`, and `.xss` `min-size`/`max-size` also apply. Percent min/max constraints resolve against the current host content box. Split allocation respects panel floors and ceilings; when a host is smaller than the sum of its minimums, the panels keep their minimums and can extend beyond the host bounds.

Panel children use the ordinary layout system. The runtime reserves the host's header height above each DockPanel's content. The `.xui` file owns initial placement and child structure; `.xss` owns all chrome appearance.

## Interaction

- Click a tab to activate it. Drag a tab or title header to rearrange a panel.
- Drop near an active group's left/right/top/bottom edge to create a nested split; drop in the center to add the panel to that group's tabs.
- During a valid drag, a `.xss`-styled preview shows the target area. The pointer-up location decides the result: releasing outside the host creates an in-app floating panel, even if the last pointer-move still showed an in-host preview. Releasing on an invalid area inside the host or pressing Escape cancels the move.
- Drag split gutters to resize the two adjacent regions. Panel min/max constraints clamp the available ratio.
- Closable panels are removed from the tree when their close button is pressed. There is no built-in reopen command; applications can hide the close button or manage panel visibility outside the current docking value API.

Floating panels stay inside the application's existing native window. They are positioned relative to the DockHost and share its event loop and backend. Separate native windows are not created.

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
DockHeader { resting: { bg: #202f46, fg: #edf3fb, pad: 8px, radius: 4px }
  hover: { bg: #293d59 } }
DockTab { resting: { bg: #101827, fg: #a6b5c9, pad: 8px }
  hover: { bg: #293d59, fg: #edf3fb }
  selected: { bg: #202f46, fg: #69a7ff } }
DockCloseButton { resting: { fg: #a6b5c9 }
  hover: { bg: #9c3e52, fg: #ffffff } }
DockSplitter { resting: { bg: #33465f }
  hover: { bg: #69a7ff } pressed: { bg: #91bdff } }
DockPreview { resting: { bg: #69a7ff55, border: 1px solid #69a7ff, radius: 5px } }
```

Dock chrome uses the same XSS palette/theme resolution as the rest of the runtime. Color, foreground, border, radius, padding, and font properties apply to the corresponding subpart. Layout rules such as `min-size` belong to the host/panel selector, not the synthetic chrome subparts.

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
- The runtime does not provide keyboard docking commands, drag payloads between separate hosts, floating-window chrome outside the host overlay, persistence/serialization, or restoration of closed panels.
- Automated tests cover tree topology, min/max clamping, nested splits, tabs, floating, hit interactions, and API restoration. Visible drag preview, cursor, native resize coexistence, and >60 FPS during user-driven docking still require a desktop smoke test; do not infer those claims from unit tests alone.
