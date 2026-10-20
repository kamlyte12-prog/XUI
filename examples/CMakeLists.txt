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
  # Backend adapters are separate targets and stay outside the framework libraries.
  add_subdirectory("${PROJECT_SOURCE_DIR}/backends" "${PROJECT_BINARY_DIR}/backends")

  add_executable(xui_glfw_demo glfw_demo.cpp)
  target_link_libraries(xui_glfw_demo PRIVATE xui_widgets xui_backend_glfw xui_backend_opengl xui_warnings)
  # The runtime demo loads only the unified showcase pair. The smaller
  # cross_platform/controls fixtures are owned by tests and linked from user docs.
  add_custom_command(TARGET xui_glfw_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xui" "$<TARGET_FILE_DIR:xui_glfw_demo>/showcase.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xss" "$<TARGET_FILE_DIR:xui_glfw_demo>/showcase.xss")

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/button)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/layout)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/text)
endif()
