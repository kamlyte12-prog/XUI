# ---- Resources that must sit next to the executable ----------------------
set(LAYOUT_RESOURCE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/layout.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/layout.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/Roboto.ttf"
)

# A custom command per resource: copies only when the source is newer.
# Outputs go into the binary dir (same place as the executable for
# single-config generators). No POST_BUILD, no PRE_*, no source-file mixing.
set(LAYOUT_RESOURCE_COPIES "")
foreach(res_src ${LAYOUT_RESOURCE_SOURCES})
    get_filename_component(res_name "${res_src}" NAME)
    set(res_dst "${CMAKE_CURRENT_BINARY_DIR}/${res_name}")
    list(APPEND LAYOUT_RESOURCE_COPIES "${res_dst}")

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
add_custom_target(copy_layout_resources ALL
    DEPENDS ${LAYOUT_RESOURCE_COPIES}
)

# ---- Demo executable -----------------------------------------------------
add_executable(xui_layout_demo layout_demo.cpp)
add_dependencies(xui_layout_demo copy_layout_resources)

target_link_libraries(xui_layout_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings
)