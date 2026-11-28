add_executable(xui_glfw_demo glfw_demo.cpp)
target_link_libraries(xui_glfw_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(SHOWCASE_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/xui-demo-image.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/xui-demo-image.svg")

xui_copy_example_assets(xui_glfw_demo ${SHOWCASE_RESOURCES})
