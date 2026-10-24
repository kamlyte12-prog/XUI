if(XUI_BUILD_GLFW_EXAMPLE)
  if(NOT TARGET xui_backend_glfw OR NOT TARGET xui_backend_opengl)
    message(FATAL_ERROR
      "XUI_BUILD_GLFW_EXAMPLE requires the optional GLFW/OpenGL backend. "
      "Enable XUI_BUILD_GLFW_BACKEND or use the default example configuration.")
  endif()

  add_executable(xui_glfw_demo glfw_demo.cpp)
  target_link_libraries(xui_glfw_demo PRIVATE xui_widgets xui_backend_glfw xui_backend_opengl xui_warnings)
  # The runtime demo loads only the unified showcase pair. The smaller
  # cross_platform/controls fixtures are owned by tests and linked from user docs.
  add_custom_command(TARGET xui_glfw_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xui" "$<TARGET_FILE_DIR:xui_glfw_demo>/showcase.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/showcase.xss" "$<TARGET_FILE_DIR:xui_glfw_demo>/showcase.xss"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/xui-demo-image.svg" "$<TARGET_FILE_DIR:xui_glfw_demo>/xui-demo-image.svg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/xui-demo-image.png" "$<TARGET_FILE_DIR:xui_glfw_demo>/xui-demo-image.png")

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/button)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/layout)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/text)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/game)
endif()
