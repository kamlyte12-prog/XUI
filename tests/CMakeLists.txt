add_executable(xui_tests test_main.cpp test_core.cpp test_xss.cpp test_xui.cpp test_layout.cpp test_render.cpp test_text.cpp test_runtime.cpp)
target_include_directories(xui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(xui_tests PRIVATE
  XUI_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
  XUI_EXAMPLE_DIR="${PROJECT_SOURCE_DIR}/examples"
  XUI_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(xui_tests PRIVATE xui_assets xui_layout xui_renderer xui_text xui_platform xui_widgets xui_warnings)
add_test(NAME xui_tests COMMAND xui_tests)
