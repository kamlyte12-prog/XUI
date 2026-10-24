set(GAME_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/game.xui"
    "${CMAKE_CURRENT_SOURCE_DIR}/game.xss"
    "${CMAKE_CURRENT_SOURCE_DIR}/../layout/Roboto.ttf")

set(GAME_ASSET_OUTPUTS "")
foreach(asset IN LISTS GAME_ASSETS)
    get_filename_component(asset_name "${asset}" NAME)
    set(asset_output "${CMAKE_CURRENT_BINARY_DIR}/${asset_name}")
    list(APPEND GAME_ASSET_OUTPUTS "${asset_output}")
    add_custom_command(
        OUTPUT "${asset_output}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${asset}" "${asset_output}"
        DEPENDS "${asset}"
        COMMENT "Copying ${asset_name} for xui_game_demo"
        VERBATIM)
endforeach()

add_custom_target(copy_game_assets ALL DEPENDS ${GAME_ASSET_OUTPUTS})

add_executable(xui_game_demo game.cpp)
add_dependencies(xui_game_demo copy_game_assets)
target_link_libraries(xui_game_demo
    PRIVATE
        xui_widgets
        xui_backend_glfw
        xui_backend_opengl
        xui_warnings)
