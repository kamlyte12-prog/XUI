add_executable(xui_text_demo text_demo.cpp)
target_link_libraries(xui_text_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(TEXT_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/text.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/text.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Roboto.ttf")

xui_copy_example_assets(xui_text_demo ${TEXT_RESOURCES})
