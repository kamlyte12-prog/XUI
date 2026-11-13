if(XUI_BUILD_GLFW_EXAMPLE)
  if(NOT TARGET xui_backend_glfw OR NOT TARGET xui_backend_opengl)
    message(FATAL_ERROR
      "XUI_BUILD_GLFW_EXAMPLE requires the optional GLFW/OpenGL backend. "
      "Enable XUI_BUILD_GLFW_BACKEND or use the default example configuration.")
  endif()

  include("${PROJECT_SOURCE_DIR}/cmake/XuiExampleAssets.cmake")

  # Each runnable example owns its source and asset list; shared copy rules live
  # in cmake/XuiExampleAssets.cmake.
  add_subdirectory(showcase)
  add_subdirectory(composites)
  add_subdirectory(button)
  add_subdirectory(layout)
  add_subdirectory(text)
  add_subdirectory(game)
endif()
