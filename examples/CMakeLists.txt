option(XUI_BUILD_GLFW_EXAMPLE "Build the cross-platform GLFW + OpenGL example (uses bundled GLFW and system OpenGL)" ON)
if(XUI_BUILD_GLFW_EXAMPLE)
  # --- GLFW from the bundled examples/lib dependency ---
  # GLFW ships its own CMakeLists.txt, so add_subdirectory is enough.
  # Common toggles to keep the build lean; adjust as you like.
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw-3.5.1
                  ${CMAKE_BINARY_DIR}/glfw EXCLUDE_FROM_ALL)

  find_package(OpenGL REQUIRED)
  if(TARGET glfw)
    set(XUI_GLFW_TARGET glfw)
  elseif(TARGET glfw3::glfw)
    set(XUI_GLFW_TARGET glfw3::glfw)
  else()
    message(FATAL_ERROR "The glfw3 package did not export a supported GLFW target")
  endif()
  add_executable(xui_glfw_demo glfw_demo.cpp glfw_platform.cpp gl_drawlist_renderer.cpp)
  target_link_libraries(xui_glfw_demo PRIVATE xui_widgets ${XUI_GLFW_TARGET} OpenGL::GL xui_warnings)
  add_custom_command(TARGET xui_glfw_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/cross_platform.xui" "$<TARGET_FILE_DIR:xui_glfw_demo>/cross_platform.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/cross_platform.xss" "$<TARGET_FILE_DIR:xui_glfw_demo>/cross_platform.xss"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/controls.xui" "$<TARGET_FILE_DIR:xui_glfw_demo>/controls.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/controls.xss" "$<TARGET_FILE_DIR:xui_glfw_demo>/controls.xss"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xui" "$<TARGET_FILE_DIR:xui_glfw_demo>/showcase.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xss" "$<TARGET_FILE_DIR:xui_glfw_demo>/showcase.xss")
  if(WIN32)
    add_executable(xui_hello glfw_demo.cpp glfw_platform.cpp gl_drawlist_renderer.cpp)
    target_link_libraries(xui_hello PRIVATE xui_widgets ${XUI_GLFW_TARGET} OpenGL::GL xui_warnings)
    add_custom_command(TARGET xui_hello POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/cross_platform.xui" "$<TARGET_FILE_DIR:xui_hello>/cross_platform.xui"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/cross_platform.xss" "$<TARGET_FILE_DIR:xui_hello>/cross_platform.xss"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/controls.xui" "$<TARGET_FILE_DIR:xui_hello>/controls.xui"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/controls.xss" "$<TARGET_FILE_DIR:xui_hello>/controls.xss"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xui" "$<TARGET_FILE_DIR:xui_hello>/showcase.xui"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xss" "$<TARGET_FILE_DIR:xui_hello>/showcase.xss")
  endif()
endif()
