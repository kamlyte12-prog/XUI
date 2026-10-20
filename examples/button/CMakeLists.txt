# ---- Resources that must sit next to the executable ----------------------
set(BUTTON_RESOURCE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/button.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/button.xss"
)

# A custom command per resource: copies only when the source is newer.
# Outputs go into the binary dir (same place as the executable for
# single-config generators). No POST_BUILD, no PRE_*, no source-file mixing.
set(BUTTON_RESOURCE_COPIES "")
foreach(res_src ${BUTTON_RESOURCE_SOURCES})
    get_filename_component(res_name "${res_src}" NAME)
    set(res_dst "${CMAKE_CURRENT_BINARY_DIR}/${res_name}")
    list(APPEND BUTTON_RESOURCE_COPIES "${res_dst}")

    add_custom_command(
        OUTPUT  "${res_dst}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${res_src}"
                "${res_dst}"
        DEPENDS "${res_src}"
        COMMENT "Copying ${res_name} if changed"
        VERBATIM
    )
endforeach()

# Dedicated target whose only job is to *consume* the copied files so CMake
# actually runs the custom commands. It never compiles anything, so the
# `.xui`/`.xss` files never end up in compile_commands.json.
add_custom_target(copy_button_resources ALL
    DEPENDS ${BUTTON_RESOURCE_COPIES}
)

# ---- Demo executable -----------------------------------------------------
add_executable(xui_button_demo button_demo.cpp)
add_dependencies(xui_button_demo copy_button_resources)

target_link_libraries(xui_button_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)