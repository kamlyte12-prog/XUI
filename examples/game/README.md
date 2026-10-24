# Tetris

This example is a playable Tetris game built with the GLFW/OpenGL backend. The board, menus, HUD, controls, and settings are authored in [game.xui](game.xui); [game.xss](game.xss) supplies the theme and animations. [game.cpp](game.cpp) owns the game rules and sends state changes through the runtime value API.

## Build and run

From the repository root:

```powershell
cmake -S . -B build -DXUI_BUILD_EXAMPLES=ON -DXUI_BUILD_GLFW_EXAMPLE=ON
cmake --build build --target xui_game_demo --config Release
```

Run `build/examples/game/Release/xui_game_demo.exe` for a Visual Studio multi-config build, or `build/examples/game/xui_game_demo` for a single-config generator. CMake copies `game.xui`, `game.xss`, and `Roboto.ttf` next to the executable.

## Play

Use the authored buttons to move, rotate, soft-drop, hard-drop, pause, and return to the menu. The gravity timer runs only during active play. F3 enables FPS/frame-time sampling; F4 adds the draw-batch count.

The game implements a seven-piece shuffled bag, wall-kick rotation, collision, row clearing, drop and line-clear scoring, and level-based gravity. The board is represented by 200 styled `ProgressBar` widgets, so the sample does not add a backend-specific drawing path. This keeps the example within the existing widget/value API; it uses a single block color and has no hold-piece mechanic.

## Verification

The automated test parses, validates, builds, and lays out the game UI, then checks the board cells and game controls. That test does not verify visible rendering, interactive play, resize behavior, or measured FPS. Run the demo on a desktop to check those behaviors.
