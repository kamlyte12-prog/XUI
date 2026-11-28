add_executable(xui_button_demo button_demo.cpp)
target_link_libraries(xui_button_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(BUTTON_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/button.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/button.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Roboto.ttf")

xui_copy_example_assets(xui_button_demo ${BUTTON_RESOURCES})
