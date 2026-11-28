add_executable(xui_game_demo game.cpp)
target_link_libraries(xui_game_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(GAME_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/game.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/game.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Roboto.ttf")

xui_copy_example_assets(xui_game_demo ${GAME_RESOURCES})
