# Fuzz harnesses exercise parsers and layout/paint traversal with deterministic
# seeds. They are ordinary executables so they can run on every CI platform;
# a libFuzzer/AFL target can be added later without changing the sources.
add_executable(fuzz_parsers fuzz_parsers.cpp)
target_link_libraries(fuzz_parsers PRIVATE xui_assets xui_warnings)

add_executable(fuzz_layout fuzz_layout.cpp)
target_link_libraries(fuzz_layout PRIVATE xui_layout xui_warnings)

add_executable(fuzz_paint fuzz_paint.cpp)
target_link_libraries(fuzz_paint PRIVATE xui_renderer xui_warnings)
