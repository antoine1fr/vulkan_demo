cmake_minimum_required(VERSION 3.12)
project(vulkan_demo C)

# Search for dependencies in submodules when falling back to config mode.
list(INSERT CMAKE_PREFIX_PATH 0 ${PROJECT_SOURCE_DIR})
list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

add_executable(vulkan_demo
  "src/main.c")
target_link_libraries(vulkan_demo
  SDL2::SDL2)
