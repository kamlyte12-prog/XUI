add_executable(xui_calculator_demo calculator.cpp)
target_link_libraries(xui_calculator_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)

set(CALCULATOR_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/calculator.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/calculator.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Roboto.ttf")
file(GLOB CALCULATOR_ICONS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.svg")
list(APPEND CALCULATOR_RESOURCES ${CALCULATOR_ICONS})

xui_copy_example_assets(xui_calculator_demo ${CALCULATOR_RESOURCES})
