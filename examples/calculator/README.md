# Windows Calculator demo

A runnable Xui reproduction of the Windows 11 Calculator in Standard mode. The
layout, custom title bar, mode drawer, keypad, and history/memory panels are
authored in [calculator.xui](calculator.xui) and [calculator.xss](calculator.xss);
the demo SVG icons live in [assets](assets/). [calculator.cpp](calculator.cpp)
implements arithmetic, percent, memory, history, clipboard, and keyboard input
through the Xui event and value APIs.

The visual reference is the XAML/theme source in
[`microsoft/calculator`](https://github.com/microsoft/calculator), so the dark
palette, 320x500 client size, 32px title bar, key colors, and 4px key radii track
the real app rather than a generic dark theme.

## Build and run

Configure with the GLFW example enabled, then build:

```powershell
cmake -S . -B build -DXUI_BUILD_EXAMPLES=ON -DXUI_BUILD_GLFW_EXAMPLE=ON
cmake --build build --target xui_calculator_demo --config Release
```

Run `build/examples/calculator/Release/xui_calculator_demo.exe` for a Visual
Studio multi-config build, or `build/examples/calculator/xui_calculator_demo.exe`
for a single-config generator. The demo resolves its UI, font, and SVG files
beside the executable, regardless of the launch working directory.

On Windows the authored title bar replaces the native one, and `radius: 8px` on
the root lets the GLFW backend request DWM rounded corners. The facade also
passes the root `bg` to the platform, which repaints DWM's 1px frame border in
that colour; otherwise the OS's light border steps along the corner arc at
fractional DPI and the corners read as pixelated. Platforms that cannot host
undecorated windows keep native chrome and render the authored bar as documented
by the framework.

## Scope

Standard mode is implemented end to end. The drawer still lists Scientific,
Graphing, Programmer, Date Calculation, the converters, and Settings so the
navigation matches the real app, but selecting any of them shows a "Not
available in this demo" notice instead of switching modes.

## Layout

The client area is the real 320x500 logical size. A 32px title bar sits above a
468px stage:

| Region | Height |
| --- | --- |
| Toolbar (history, mode, title) | 48px |
| Expression line | 23px |
| Result display | 44px |
| Memory toolbar | 33px |
| Keypad | 320px (grows with the window) |

The keypad is a 4x6 `Grid` with `pad: 3` and `gap: 2`, so keys are
77 x 50.67 at the default size. The memory toolbar is a six-column `1fr` `Grid`
with `pad: 0 4px`, giving each of MC, MR, M+, M−, MS, and the M-dropdown an exact
52px track. The dropdown is composed from `Text("M")` and `chevron-down.svg`.

## Behavior

- **Arithmetic** — chaining, repeated `=`, sign toggle, reciprocal, square, square
  root, percent, and backspace/clear-entry/clear.
- **Percent** — follows the Windows operand rules: with `+` or `−` the percent
  applies to the accumulator (`accumulator * value / 100`); with `*` or `/`, and
  for a lone value, it is `value / 100`.
- **Errors** — division by zero reports `Cannot divide by zero`, `0 / 0` reports
  `Result is undefined`, a negative square root reports `Invalid input`, and
  overflow reports `Overflow`. The next digit starts a fresh calculation.
- **History** — every completed `=` appends an entry, newest at the bottom, capped
  at six rows. Clicking a row recalls its result. `Ctrl+Shift+D` clears it.
- **Memory** — newest first, capped at six entries. `MS` inserts at the front,
  `M+`/`M−` adjust the top entry, and `MC` clears all. Each row has its own
  MC/M+/M−, and clicking a value recalls it and closes the panel.
- **Clipboard** — `Ctrl+C` copies the display, `Ctrl+V` pastes a parseable number.

History and memory open as full-stage overlays rather than anchored flyouts
because the facade has no runtime window-resize API for anchored popups.

## Keyboard map

| Key | Action |
| --- | --- |
| `0`–`9`, `.` | Digits and decimal separator |
| `+` `-` `*` `/` | Operators (`Shift+8` for `*`, `Shift+=` for `+`) |
| `Enter`, `=`, keypad `Enter`/`=` | Equals |
| `Backspace` | Backspace |
| `Delete` | Clear entry |
| `Escape` | Close an open panel, otherwise clear |
| `F9` | Negate |
| `R` / `Q` | Reciprocal / square |
| `Shift+2` / `Shift+5` | Square root / percent |
| `Ctrl+C` / `Ctrl+V` | Copy / paste |
| `Ctrl+H` | Toggle history |
| `Ctrl+M` / `Ctrl+P` / `Ctrl+Q` | Memory store / add / subtract |
| `Ctrl+R` / `Ctrl+L` | Memory recall / clear all |
| `Ctrl+Shift+D` | Clear history |

Keypad digits and operators are mapped too. The memory panel opens from the
M-dropdown button because `Ctrl+M` is already "store in memory".

## Known deviations

- History and memory are full-stage overlays with a compact 48px header (back,
  title, trash) instead of the real bottom-trash flyout, because the facade has
  no anchored-popup or runtime window-resize API.
- The toolbar "always on top" pin is omitted; no runtime API exposes
  `WindowDesc::alwaysOnTop`.
- History and memory cap at six entries.
- Arithmetic uses `double` (about 16 significant digits), so
  `99999999 × 99999999` shows `9,999,999,800,000,000` where the real app's
  32-digit engine shows `...001`.
- The result display fits 16-digit grouped values at 26px but has no font
  auto-shrink like the real app.
- Settings and the other modes are navigation placeholders, as described above.

## Verification

`tests/test_runtime.cpp::calculator_example_assets_parse_validate_and_layout`
parses and validates both documents, then checks the 320x500 geometry, the
77 x 50.67 keypad keys, the 44px display, and the 52px memory columns. The demo
was also driven on Windows through keyboard and pointer input for arithmetic,
percent, repeated equals, keypad clicks, history recall, memory store/add, the
drawer and notice, divide-by-zero, reciprocal, clipboard round-trips, Escape,
empty panels, and long results.
