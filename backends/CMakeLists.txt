# Backend adapters are optional implementations of the public renderer/platform interfaces.
# This directory is added by the GLFW example after GLFW and OpenGL targets are available.
if(NOT XUI_GLFW_TARGET)
  message(FATAL_ERROR "backends requires XUI_GLFW_TARGET from the GLFW example setup")
endif()
if(NOT TARGET OpenGL::GL)
  message(FATAL_ERROR "backends requires OpenGL::GL from find_package(OpenGL)")
endif()

add_library(xui_backend_glfw STATIC glfw/glfw_platform.cpp)
target_include_directories(xui_backend_glfw PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(xui_backend_glfw PUBLIC xui_platform "${XUI_GLFW_TARGET}" PRIVATE xui_warnings)
if(WIN32)
  # Client-drawn window chrome (borderless NCCALCSIZE/NCHITTEST subclass, native drag/snap).
  target_link_libraries(xui_backend_glfw PRIVATE user32)
endif()

add_library(xui_backend_opengl STATIC opengl/gl_drawlist_renderer.cpp)
target_include_directories(xui_backend_opengl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(xui_backend_opengl PUBLIC xui_renderer "${XUI_GLFW_TARGET}" OpenGL::GL PRIVATE xui_warnings)
