cmake_minimum_required(VERSION 3.12)
project(vulkan_demo CXX)

# Search for dependencies in submodules when falling back to config mode.
list(INSERT CMAKE_PREFIX_PATH 0 ${PROJECT_SOURCE_DIR})
list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake)

find_package(glm "0.9" REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(vulkan_demo
  "src/main.cpp")
target_link_libraries(vulkan_demo
  glm::glm
  SDL2::SDL2
  Vulkan::Vulkan)
target_compile_options(vulkan_demo PRIVATE -Wall -Wextra -pedantic -Weffc++)
target_compile_features(vulkan_demo PRIVATE cxx_std_17)
set_target_properties(vulkan_demo PROPERTIES CXX_EXTENSIONS OFF)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
  -framework AppKit \
  -framework Metal \
  -framework MetalKit \
  -framework ModelIO")
