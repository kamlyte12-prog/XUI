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

`Once` restarts from the first keyframe, plays one duration, and holds the final sample. `Loop` repeats until cancelled. A second play request replaces the current playback and starts over. The imperative `cancelAnimation(id)` stops immediately and leaves the current sample painted. Only visible, active animations schedule timer-driven frames. Paused and completed animations do not keep the idle loop awake. The GLFW platform pauses animation clocks while hidden or minimized, then resumes with elapsed animation time preserved; it continues while visible but unfocused. Custom platforms default to visible and should override `Platform::windowVisible()` if they support hiding/minimizing.

Numbers with matching units, colors, and compatible transform function sequences interpolate. The `value` keyframe property currently updates numeric progress controls such as `ProgressBar`. Keyframes do not alter layout sizes, font face/weight, or arbitrary application values.

### Animations without input, and `animation-keep`

Active visible animations and state-transition tweens run on a 16 ms timer path: the event loop wakes, samples, damages the animated nodes, and returns to waiting when nothing animates. Mouse movement is not required. GLFW sends cursor-enter/leave events so hover transitions return to resting after the pointer exits the client area. A completed transition paints its exact final value before the timer retires. The event loop blocks between frames, so an idle window does not spin.

By default a mid-flight transition is cleared when the state exits early. The legacy per-widget property `animation-keep: until-done` retargets it to the new state's values. For a widget that should finish the current target before applying a newer state, use `animation-cancel: finish`; `stop` (the default) applies the latest state immediately. This applies to both transitions and keyframe animation. On an infinite keyframe, `finish` completes the current iteration, then applies the latest state.

```xss
Button.keep { resting: { bg: #222222 } hover: { bg: #3A5FA8 }
  transition: resting -> hover 160ms ease-out
  transition: hover -> resting 160ms ease-out
  animation-keep: until-done }
```

The per-animation `cancel(finish)` modifier overrides the widget default. `cancel(stop)` overrides it in the other direction.

```xss
Panel.pulse { resting: { animation: pulse 900ms infinite ease-in-out cancel(finish) } }
Button.exit { resting: { animation-cancel: finish } hover: { bg: #3A5FA8 }
  transition: resting -> hover 160ms ease-out
  transition: hover -> resting 160ms ease-out }
```

`animation-keep` retains its older retargeting behavior; `animation-cancel` is the finish-current-target policy. Keyframes use their duration, iteration count, and `infinite` marker for one-shot/looping behavior. Application calls to `cancelAnimation(id)` remain immediate and do not use stylesheet cancellation policy. Animations pause while their node is hidden in the UI tree (docked-away or on an unselected tab) and while the host window is minimized/hidden; they continue while the visible window is unfocused.

### Draw-batch counter

Press F4 in the GLFW showcase to display the previous completed frame's `Painter::Stats.drawCalls` beside the FPS readout. This counts backend-neutral draw-list batches, not GPU draw submissions. It updates when the app renders for another reason and does not force continuous rendering while idle. F3 still enables the FPS readout's continuous sampling mode.

## Widget transforms

The runtime accepts these functions in `.xss`:

```xss
Button.moved { resting: { transform: translate(18px, -4px) scale(1.04) } }
Button.tilted { resting: { transform: rotate(4deg) } }
```

Transforms are paint transforms composed from the widget center outward and inherited by descendant paint. The same matrix is inverted for node hit testing, so a translated/rotated widget's interactive bounds follow its painted geometry. Transform translations use logical pixels; percentages are relative to the widget's width/height. Rotation values use degrees. Compatible transform values can animate through transitions and keyframes.

Limitations: transformed clips use an axis-aligned scissor bounding box, so a rotated clip can admit pixels in the corners outside the exact rotated shape. Popups and other overlay passes remain in window coordinates. Composite virtual hit targets such as scrollbars and tab headers are not independently transformed. Complex nested transforms therefore need a visible smoke test before production use.

## Font file and glyph support

`Xui::loadFont(path, family, weight)` registers every loaded face; the first loaded face is the default fallback. If `.xss` omits the `font` family property, the runtime selects from faces registered in the `default` family. For a variable TrueType font with a `wght` axis, numeric `.xss font-weight` selects a cached font instance at that design coordinate, including fractional numeric values. The requested coordinate is clamped to the font axis minimum and maximum; FreeType applies the variation data before measurement and outline extraction. When a family has both variable and static faces, a variable face is preferred when its clamped axis coordinate is at least as close to the request as the static face weight. This prevents a separately registered bold face from masking an in-range variable axis. Outside the variable axis range, normal nearest-effective-weight selection applies; ties prefer the variable face. The glyph atlas keys entries by unique font-instance identity, glyph index, and pixel size, so two weights at the same glyph and size remain distinct in one frame.

Text layout passes each left-to-right, whitespace-delimited run through bundled HarfBuzz 15.0.0. HarfBuzz applies OpenType GPOS pair adjustments and variation deltas using the same `wght` coordinate as the FreeType face. The framework keeps explicit whitespace separate for its existing wrapping rules. `liga` and `clig` substitutions are disabled so a text field retains a straightforward relationship between input characters and glyph positions. The shaping output supplies advances and offsets to layout; FreeType's glyph metrics and outline path use `FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM`. Atlas glyph advances use that same unhinted metric path, and outlines are rasterized by Xui's software rasterizer. There is no 26.6 pixel rounding in layout: HarfBuzz positions are kept in font units as integers, scaled to floating-point logical pixels, and accumulated without rounding each glyph to a pixel. Glyph bitmap size is rounded separately to an integer pixel size for atlas lookup. This keeps measurement and drawing on consistent unhinted geometry without changing the existing global rounding or hinting policy.

`font-weight` is inherited by descendants that do not specify their own value; a child `.xss` rule overrides the inherited weight. This applies to text inside buttons and editable fields as well as ordinary `Text` nodes. Static fonts and variable fonts without a `wght` axis use nearest registered face selection. If there is only one face, all requested weights use that same face; the runtime does not synthesize bold or use system fallback. `font-weight` accepts numeric values; named identifiers parse but are not mapped to numeric weights. TTF and OTF containers with TrueType `glyf` outlines are supported. CFF/CFF2-only outlines and color glyph tables remain unsupported. Only `wght` is controlled by `.xss`; all other variable axes remain at their font defaults.

Shaping is currently constrained to left-to-right layout. Bidirectional and right-to-left paragraph layout, ligature substitutions, and complex-script editing/caret mapping are not supported. The implementation depends on the bundled HarfBuzz 15.0.0 and FreeType 2.15.0 source trees; FreeType's optional HarfBuzz integration is disabled because Xui links HarfBuzz directly for shaping.

To trace an end-to-end weight request while diagnosing a font, set `XUI_TRACE_FONT_WEIGHT=1` before launching the app. The trace reports each distinct FreeType axis setup and matching HarfBuzz `wght` coordinate, runtime face selection, glyph-load coordinate, and glyph-atlas key once. It is disabled by default.

| Capability | Current behavior |
| --- | --- |
| Multiple files/families/weights | Supported and registered before `attach()`. |
| Missing requested weight | Variable `wght` axis is used when present; otherwise nearest registered weight, with no synthetic bold. |
| TTF and TrueType-outline OTF | Supported. |
| CFF/CFF2 OTF outlines | Not rasterized; the file does not provide supported `glyf` outlines. |
| Variable font `wght` axis | Applied through FreeType for variable `glyf` fonts; coordinates are clamped to axis bounds. |
| Text shaping | HarfBuzz OpenType shaping and GPOS are applied to left-to-right runs; ligatures and bidi are disabled. |
| Missing glyph fallback | There is no automatic cross-family fallback or system font fallback. Use a face that contains the needed glyphs. |
| Atlas capacity | Shared growable atlas starts at the configured size and grows to 4096×4096, with LRU eviction. |
| Complex shaping | No RTL/bidi layout or complex-script editing/caret model; ligature substitutions are disabled. |

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
