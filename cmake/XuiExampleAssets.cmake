# Copy authored example assets beside a target's executable. Using
# TARGET_FILE_DIR keeps multi-config generators (Visual Studio) working.
include_guard(GLOBAL)

function(xui_copy_example_assets target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR
      "xui_copy_example_assets: '${target}' is not a target")
  endif()

  # Refuse double-application - the dependency target would collide.
  if(TARGET "xui_example_assets_${target}")
    message(FATAL_ERROR
      "xui_copy_example_assets: already applied to '${target}'")
  endif()

  # Per-configuration stamp directory. CMAKE_CFG_INTDIR expands to the
  # config subdirectory at build time on multi-config generators (and to
  # '.' on single-config ones), so each configuration gets its own stamps.
  set(_stamp_dir
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/xui_example_assets")

  set(_stamps "")
  foreach(_resource IN LISTS ARGN)
    if(NOT EXISTS "${_resource}")
      message(FATAL_ERROR
        "xui_copy_example_assets: missing asset: ${_resource}")
    endif()

    get_filename_component(_name "${_resource}" NAME)

    # Per-target stamp: two targets sharing an asset must not define the
    # same OUTPUT rule twice.
    set(_stamp "${_stamp_dir}/${target}__${_name}.stamp")
    list(APPEND _stamps "${_stamp}")

    add_custom_command(
      OUTPUT "${_stamp}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_stamp_dir}"
      COMMAND ${CMAKE_COMMAND} -E make_directory
              "$<TARGET_FILE_DIR:${target}>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${_resource}"
              "$<TARGET_FILE_DIR:${target}>/${_name}"
      COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
      DEPENDS "${_resource}"
      COMMENT "Copying ${_name} for ${target}"
      VERBATIM)
  endforeach()

  if(_stamps)
    add_custom_target("xui_example_assets_${target}" DEPENDS ${_stamps})
    add_dependencies("${target}" "xui_example_assets_${target}")
  endif()
endfunction()
