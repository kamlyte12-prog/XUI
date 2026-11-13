# Copy authored example assets beside a target's executable. Using
# TARGET_FILE_DIR keeps multi-config generators (Visual Studio) working.
include_guard(GLOBAL)

function(xui_copy_example_assets target)
  foreach(resource IN LISTS ARGN)
    if(NOT EXISTS "${resource}")
      message(FATAL_ERROR "Missing example asset: ${resource}")
    endif()

    get_filename_component(resource_name "${resource}" NAME)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${resource}"
              "$<TARGET_FILE_DIR:${target}>/${resource_name}"
      COMMENT "Copying ${resource_name} for ${target}"
      VERBATIM)
  endforeach()
endfunction()
