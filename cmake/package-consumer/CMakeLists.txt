cmake_minimum_required(VERSION 3.25)
project(xui_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This project only consumes the installed package. Configure it with
# -DCMAKE_PREFIX_PATH=<install prefix>.
find_package(Xui CONFIG REQUIRED)

add_executable(xui_package_consumer main.cpp)
target_link_libraries(xui_package_consumer PRIVATE xui::widgets)
