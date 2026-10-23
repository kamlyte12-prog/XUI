cmake_minimum_required(VERSION 3.25)
project(Xui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(XUI_BUILD_TESTS       "Build unit tests"        ON)
option(XUI_BUILD_BENCHMARKS  "Build benchmarks"        ON)
option(XUI_BUILD_EXAMPLES    "Build runnable examples" ON)
option(XUI_BUILD_GLFW_EXAMPLE "Build the GLFW + OpenGL example (uses bundled GLFW and system OpenGL)" ON)
option(XUI_WERROR            "Treat warnings as errors" ON)
option(XUI_SANITIZE          "Enable ASan+UBSan (GCC/Clang)" OFF)

# Build vendored FreeType so builds do not depend on a system package or package
# manager. Xui uses FreeType for variable glyf axes/outlines; shaping is provided
# by the separate bundled HarfBuzz target below.
set(XUI_FREETYPE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/freetype-2.15.0")
if(NOT EXISTS "${XUI_FREETYPE_SOURCE_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${XUI_FREETYPE_SOURCE_DIR}/include/freetype/freetype.h")
  message(FATAL_ERROR
    "Bundled FreeType 2.15.0 source is missing. Expected it at "
    "${XUI_FREETYPE_SOURCE_DIR} (CMakeLists.txt and include/freetype/freetype.h).")
endif()

set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable system zlib; FreeType uses its internal inflater" FORCE)
foreach(_ft_optional_feature BZIP2 PNG HARFBUZZ BROTLI HVF)
  set(FT_DISABLE_${_ft_optional_feature} ON CACHE BOOL
      "Disable unused optional FreeType ${_ft_optional_feature} support" FORCE)
endforeach()

# Do not let a consumer's BUILD_SHARED_LIBS setting change FreeType's linkage;
# a static bundled dependency keeps executable deployment predictable.
if(DEFINED BUILD_SHARED_LIBS)
  set(_xui_had_build_shared_libs TRUE)
  set(_xui_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
else()
  set(_xui_had_build_shared_libs FALSE)
endif()
set(BUILD_SHARED_LIBS OFF)
add_subdirectory("${XUI_FREETYPE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/freetype")
if(_xui_had_build_shared_libs)
  set(BUILD_SHARED_LIBS "${_xui_saved_build_shared_libs}")
else()
  unset(BUILD_SHARED_LIBS)
endif()

# Stable dependency name for Xui targets; the implementation remains vendored.
add_library(xui_freetype INTERFACE)
add_library(xui::freetype ALIAS xui_freetype)
target_link_libraries(xui_freetype INTERFACE freetype)

# HarfBuzz applies OpenType GPOS (including variable-font positioning) during text shaping.
# Keep the bundled source in sync with FreeType so builds do not depend on system packages.
set(XUI_HARFBUZZ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/harfbuzz-15.0.0")
if(NOT EXISTS "${XUI_HARFBUZZ_SOURCE_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${XUI_HARFBUZZ_SOURCE_DIR}/src/hb.h")
  message(FATAL_ERROR
    "Bundled HarfBuzz 15.0.0 source is missing. Expected it at "
    "${XUI_HARFBUZZ_SOURCE_DIR} (CMakeLists.txt and src/hb.h).")
endif()
foreach(_hb_unused_feature SUBSET RASTER VECTOR GPU)
  set(HB_BUILD_${_hb_unused_feature} OFF CACHE BOOL
      "Disable unused bundled HarfBuzz ${_hb_unused_feature} support" FORCE)
endforeach()
set(HB_BUILD_UTILS OFF CACHE BOOL "Do not build HarfBuzz command-line utilities" FORCE)
set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "Do not enable the optional Graphite2 shaper" FORCE)
set(HB_HAVE_GLIB OFF CACHE BOOL "Do not enable the optional GLib integration" FORCE)
set(HB_HAVE_ICU OFF CACHE BOOL "Do not enable the optional ICU integration" FORCE)
set(HB_HAVE_CAIRO OFF CACHE BOOL "Do not enable the optional Cairo integration" FORCE)
if(DEFINED BUILD_SHARED_LIBS)
  set(_xui_had_build_shared_libs TRUE)
  set(_xui_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
else()
  set(_xui_had_build_shared_libs FALSE)
endif()
set(BUILD_SHARED_LIBS OFF)
add_subdirectory("${XUI_HARFBUZZ_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/harfbuzz")
if(_xui_had_build_shared_libs)
  set(BUILD_SHARED_LIBS "${_xui_saved_build_shared_libs}")
else()
  unset(BUILD_SHARED_LIBS)
endif()
add_library(xui_harfbuzz INTERFACE)
add_library(xui::harfbuzz ALIAS xui_harfbuzz)
target_link_libraries(xui_harfbuzz INTERFACE harfbuzz)

# One place for warning policy; every Xui target links xui_warnings.
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

# --- xui_core: handles, arenas, interner, math, colour, diagnostics. Zero third-party deps. ---
add_library(xui_core STATIC src/core/color.cpp src/core/diagnostic.cpp)
target_include_directories(xui_core PUBLIC include)
target_link_libraries(xui_core PRIVATE xui_warnings)

# --- xui_assets: .xss (and, from M3, .xui) parsers and compilers. Depends only on xui_core. ---
add_library(xui_assets STATIC
  src/style/style_types.cpp src/style/style_sheet.cpp src/style/xss_parser.cpp src/style/xss_compiler.cpp
  src/lang/xui_parser.cpp src/lang/xui_loader.cpp src/lang/xui_schema.cpp)
target_include_directories(xui_assets PRIVATE src/style)
target_link_libraries(xui_assets PUBLIC xui_core PRIVATE xui_warnings)

# --- xui_layout: flex/grid/stack/split layout engine. Depends only on xui_core. ---
add_library(xui_layout STATIC src/layout/layout_tree.cpp src/layout/layout_solve.cpp src/layout/docking.cpp)
target_include_directories(xui_layout PRIVATE src/layout)
target_link_libraries(xui_layout PUBLIC xui_core PRIVATE xui_warnings)

# --- xui_renderer: Renderer interface, DrawList, Painter, NullRenderer; plus the reference software rasteriser. ---
add_library(xui_renderer STATIC src/render/painter.cpp src/render/null_renderer.cpp src/render/soft_renderer.cpp src/render/image_io.cpp)
target_link_libraries(xui_renderer PUBLIC xui_core PRIVATE xui_warnings)

# --- xui_platform: Platform interface + headless null backend. ---
add_library(xui_platform STATIC src/platform/null_platform.cpp)
target_link_libraries(xui_platform PUBLIC xui_core PRIVATE xui_warnings)

# --- xui_text: TTF parsing, glyph rasteriser, variable-font support, atlas, shaping/wrap cache. ---
add_library(xui_text STATIC
  src/text/font.cpp src/text/rasterizer.cpp src/text/atlas.cpp src/text/text_layout.cpp)
target_link_libraries(xui_text PUBLIC xui_core xui_renderer PRIVATE xui_warnings)
target_link_libraries(xui_text PUBLIC xui::freetype xui::harfbuzz)

# --- xui_widgets: first retained runtime bridge from .xui/.xss to layout, text and paint. ---
add_library(xui_widgets STATIC src/ui/runtime.cpp src/ui/xui.cpp)
target_include_directories(xui_widgets PUBLIC include)
target_link_libraries(xui_widgets PUBLIC xui_assets xui_layout xui_renderer xui_text PRIVATE xui_warnings)

if(XUI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(XUI_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(XUI_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
