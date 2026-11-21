add_executable(xui_layout_demo layout_demo.cpp)
target_link_libraries(xui_layout_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(LAYOUT_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/layout.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/layout.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Roboto.ttf")

xui_copy_example_assets(xui_layout_demo ${LAYOUT_RESOURCES})
