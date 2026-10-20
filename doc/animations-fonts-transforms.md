# Animation, transforms, fonts, and cursors

This page records the behavior of these features in the standard `UiRuntime` and GLFW backend. It does not imply support from a custom backend beyond the draw-list and platform contracts.

## Animation playback

State transitions start automatically when the runtime enters the destination state. Keyframes start when their styled node is first painted unless the declaration includes `paused`:

```xss
ProgressBar.meter { resting: { animation: fill 500ms paused ease-out } }
keyframes fill {
  from { value: 0 }
  to { value: 1 }
}
```

Start and stop authored keyframes with the facade:

```cpp
ui.playAnimation("meter", xui::AnimationMode::Once);
ui.playAnimation("spinner", xui::AnimationMode::Loop);
ui.cancelAnimation("spinner");
```

For an event-triggered value animation, give a `ProgressBar` a paused `value` keyframe in `.xss`, then call `playAnimation(id, Once)` from the button/event callback. Use `Loop` for a repeating animation and `cancelAnimation(id)` to stop at the current value. The GLFW showcase's Motion & Style → Animation tab demonstrates all three controls.

`Once` restarts from the first keyframe, plays one duration, and holds the final sample. `Loop` repeats until cancelled. A second play request replaces the current playback and starts over. Cancellation leaves the current sample painted. Application conditions are handled by calling `cancelAnimation(id)` from the application's event logic; the stylesheet does not contain application-specific predicates. Only visible, active animations schedule timer-driven frames. Paused and completed animations do not keep the idle loop awake. Visible-window background behavior was tested through the timer path, but minimized-window pausing is not currently reported by `Platform` and is not guaranteed.

Numbers with matching units, colors, and compatible transform function sequences interpolate. The `value` keyframe property currently updates numeric progress controls such as `ProgressBar`. Keyframes do not alter layout sizes, font face/weight, or arbitrary application values.

## Widget transforms

The runtime accepts these functions in `.xss`:

```xss
Button.moved { resting: { transform: translate(18px, -4px) scale(1.04) } }
Button.tilted { resting: { transform: rotate(4deg) } }
```

Transforms are paint transforms composed from the widget center outward and inherited by descendant paint. The same matrix is inverted for node hit testing, so a translated/rotated widget's interactive bounds follow its painted geometry. Transform translations use logical pixels; percentages are relative to the widget's width/height. Rotation values use degrees. Compatible transform values can animate through transitions and keyframes.

Limitations: transformed clips use an axis-aligned scissor bounding box, so a rotated clip can admit pixels in the corners outside the exact rotated shape. Popups and other overlay passes remain in window coordinates. Composite virtual hit targets such as scrollbars and tab headers are not independently transformed. Complex nested transforms therefore need a visible smoke test before production use.

## Font file and glyph support

`Xui::loadFont(path, family, weight)` registers every loaded face; the first loaded face is the default. Numeric `.xss font-weight` picks the nearest registered weight in that family, without synthetic bold. The loader accepts TrueType `glyf` outlines in TTF files and OpenType containers with TrueType outlines. It reads `cmap` format 4 and 12, composite glyphs, and format-0 kerning.

| Capability | Current behavior |
| --- | --- |
| Multiple files/families/weights | Supported and registered before `attach()`. |
| Missing requested weight | Nearest registered weight; no synthetic bold. |
| TTF and TrueType-outline OTF | Supported. |
| CFF/CFF2 OTF outlines | Not rasterized; the file does not provide supported `glyf` outlines. |
| Variable font axis deltas | Not applied. |
| Unicode cmap | Format 4 and 12 are read; language-specific shaping is not implemented. |
| Missing glyph fallback | There is no automatic cross-family fallback or system font fallback. Use a face that contains the needed glyphs. |
| Atlas capacity | Shared growable atlas starts at the configured size and grows to 4096×4096, with LRU eviction. |
| Complex shaping | No script shaping, ligatures, bidi, or full OpenType shaping; text layout uses supported cmap/metrics/kerning paths. |

Color emoji and CFF-only OTF files are unsupported. No user-visible fallback font is selected automatically, so unsupported characters may render as the font's missing glyph or empty space depending on the face.

## Cursor style values

Use `cursor` in the widget's `.xss` rule. Explicit style values override the runtime's sensible default cursor for that widget.

| `.xss` value | Runtime shape | GLFW behavior |
| --- | --- | --- |
| `default` | Arrow | Arrow |
| `pointer` | Hand | Hand |
| `text` | I-beam | I-beam |
| `crosshair` | Crosshair | Crosshair |
| `resize-ew`, `resize-ns` | Horizontal/vertical resize | Matching standard cursor |
| `resize-nwse`, `resize-nesw` | Diagonal resize | Hand fallback |
| `move` | All-direction resize/move | Hand fallback |
| `grab`, `grabbing` | Grab states | Hand fallback |
| `not-allowed` | Not allowed | Arrow fallback |
| `hidden` | Hidden | GLFW hidden cursor mode |

```xss
Button.drag-handle { resting: { cursor: grab } pressed: { cursor: grabbing } }
TextBox.field { resting: { cursor: text } }
```

Other platform implementations may map unsupported cursors differently; cursor style does not change pointer hit testing.
