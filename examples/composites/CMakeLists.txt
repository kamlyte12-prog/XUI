add_executable(xui_component_demo component_demo.cpp)
target_link_libraries(xui_component_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(COMPONENT_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/card.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.xss")

xui_copy_example_assets(xui_component_demo ${COMPONENT_RESOURCES})
