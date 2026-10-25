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

  add_executable(xui_component_demo composites/component_demo.cpp)
  target_link_libraries(xui_component_demo PRIVATE xui_widgets xui_backend_glfw xui_backend_opengl xui_warnings)
  set(COMPONENT_DEMO_MAIN_UI "${CMAKE_CURRENT_SOURCE_DIR}/composites/main.xui")
  set(COMPONENT_DEMO_CARD_UI "${CMAKE_CURRENT_SOURCE_DIR}/composites/card.xui")
  set(COMPONENT_DEMO_STYLES "${CMAKE_CURRENT_SOURCE_DIR}/composites/main.xss")
  set(COMPONENT_DEMO_ASSETS "${COMPONENT_DEMO_MAIN_UI};${COMPONENT_DEMO_CARD_UI};${COMPONENT_DEMO_STYLES}")
  set_property(TARGET xui_component_demo APPEND PROPERTY LINK_DEPENDS "${COMPONENT_DEMO_ASSETS}")
  add_custom_command(TARGET xui_component_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${COMPONENT_DEMO_MAIN_UI}" "$<TARGET_FILE_DIR:xui_component_demo>/main.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${COMPONENT_DEMO_CARD_UI}" "$<TARGET_FILE_DIR:xui_component_demo>/card.xui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${COMPONENT_DEMO_STYLES}" "$<TARGET_FILE_DIR:xui_component_demo>/main.xss")

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/button)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/layout)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/text)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/game)
endif()
