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
  "src/main.cpp"
  "src/App.cpp"
  "src/render/RenderSystem.cpp"
  "src/render/vulkan/Buffer.cpp"
  "src/render/vulkan/Memory.cpp")
target_include_directories(
  vulkan_demo
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(vulkan_demo
  glm::glm
  SDL2::SDL2
  Vulkan::Vulkan)
target_compile_features(vulkan_demo PRIVATE cxx_std_17)
set_target_properties(vulkan_demo PROPERTIES CXX_EXTENSIONS OFF)

if (UNIX)
  target_sources(vulkan_demo
    PRIVATE
    "src/system/system_unix.cpp")
  target_compile_options(vulkan_demo PRIVATE -Wall -Wextra -pedantic -Weffc++)
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " -g3 -O0")
elseif(MSVC)
  target_sources(vulkan_demo
    PRIVATE
    "src/system/system_windows.cpp")
endif()

string(APPEND CMAKE_CXX_FLAGS_DEBUG " -DDEBUG")

if(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
  -framework AppKit \
  -framework Metal \
  -framework MetalKit \
  -framework ModelIO")
endif()

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src/shaders")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vertex.spv"
  COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
  ARGS -O0 -g -o "${CMAKE_CURRENT_BINARY_DIR}/vertex.spv" -fshader-stage=vertex "${SHADER_SOURCE_DIR}/vertex.glsl"
  DEPENDS "${SHADER_SOURCE_DIR}/vertex.glsl"
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fragment.spv"
  COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
  ARGS -O0 -g -o "${CMAKE_CURRENT_BINARY_DIR}/fragment.spv" -fshader-stage=fragment "${SHADER_SOURCE_DIR}/fragment.glsl"
  DEPENDS "${SHADER_SOURCE_DIR}/fragment.glsl"
)

add_custom_target(
  shader_gen ALL
  DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/vertex.spv"
  "${CMAKE_CURRENT_BINARY_DIR}/fragment.spv"
)
