# The Ashen Gate

This is a small, event-driven turn-based encounter built with the Xui GLFW/OpenGL example backend. It is a game-UI sample, not a graphics-engine sample: the menu, combat page, pause page, settings, inventory, and results are all authored in [`game.xui`](game.xui) and styled in [`game.xss`](game.xss). `game.cpp` owns the combat state and updates authored control values from UI events.

## Build and run

From the repository root:

```powershell
cmake -S . -B build -DXUI_BUILD_EXAMPLES=ON -DXUI_BUILD_GLFW_EXAMPLE=ON
cmake --build build --target xui_game_demo --config Release
```

Run `build/examples/game/Release/xui_game_demo.exe` with a Visual Studio multi-config build, or `build/examples/game/xui_game_demo` with a single-config generator. CMake copies `game.xui`, `game.xss`, and the shared `Roboto.ttf` next to the executable. This example uses the existing GLFW and OpenGL backend targets and requires the platform's OpenGL SDK/runtime.

## Controls and game state

- **Begin encounter** starts a fresh fight. Strike and Heavy strike damage the sentinel; Use draught restores health; Guard reduces the next counterattack. Health, score, turn, and remaining draughts update after each action. Defeat the sentinel or lose all health, then restart from Results.
- **Settings** changes difficulty, strike power, and combat hints. **Inventory** toggles a rune blade and iron shield; equipped items affect combat damage.
- The page tabs expose Main Menu, Encounter, Paused, Settings, Inventory, and Results. Buttons use the `press` event names in `.xui`; stateful controls use IDs and typed `value()` reads; the C++ event handler updates labels and progress bars with `setValue()`.
- Tooltips appear on the controls that explain a combat action or setting. Event messages use the existing XSS animation system. F3 toggles continuous FPS sampling; F4 toggles the draw-batch count.

The loop is event-driven. It waits for input while idle, so no per-frame game simulation runs when paused. Gameplay and UI updates happen on the same UI event thread. No GLFW calls or widget construction appear in the game logic.

## Scope and limitations

The pause screen is an authored tab page, not a modal with focus trapping. There is no separate game canvas or sprite renderer. `Xui::run()` does not currently expose a per-frame application callback, so this sample intentionally avoids adding a parallel game-rendering path. The framework provides the UI and backend; the C++ file only manages the encounter rules and values.

The checked-in automated test parses, validates, and builds the authored game document and checks its stateful IDs. Visible mouse/keyboard playthrough, live resize, and visible FPS measurements still require a desktop run; the test does not claim those have been verified.
