# Central warning and sanitizer policy. Every Xui target links xui_warnings so
# that compile flags stay consistent across the library, backends, examples,
# benchmarks, fuzzers, and tests.
include_guard(GLOBAL)

add_library(xui_warnings INTERFACE)
if(MSVC)
  target_compile_options(xui_warnings INTERFACE /W4 /permissive- /EHsc $<$<BOOL:${XUI_WERROR}>:/WX>)
  target_compile_definitions(xui_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(xui_warnings INTERFACE -Wall -Wextra -Wpedantic $<$<BOOL:${XUI_WERROR}>:-Werror>)
endif()
if(XUI_SANITIZE AND NOT MSVC)
  target_compile_options(xui_warnings INTERFACE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(xui_warnings INTERFACE -fsanitize=address,undefined)
endif()
if(APPLE)
  # Apple's OpenGL headers are deprecated; without this, -Werror breaks every GL call.
  target_compile_definitions(xui_warnings INTERFACE GL_SILENCE_DEPRECATION)
endif()
