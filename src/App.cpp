// STL headers
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "App.hpp"
#include "system.hpp"

namespace {
struct PassUniforms {
  glm::mat4 view_matrix;
  glm::mat4 projection_matrix;
};

struct ObjectUniforms {
  glm::mat4 world_matrix;
};

static constexpr size_t kUniformBufferSize =
    sizeof(PassUniforms) + sizeof(ObjectUniforms);
}  // namespace

App::App() {}

void App::cleanup() {
  render_system_.cleanup();
}

void App::init() {
  std::list<render::UniformBufferDescriptor::Block> blocks{
      {0, 0, sizeof(PassUniforms)},
      {1, sizeof(PassUniforms), sizeof(ObjectUniforms)}};
  render::UniformBufferDescriptor ubo_descriptor{
      sizeof(PassUniforms) + sizeof(ObjectUniforms), blocks};
  create_frame_packet_();
  render_system_.init(ubo_descriptor);
}

void App::run() {
  bool run = true;

  while (run) {
    SDL_Event event;
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT)
      run = false;
    render_system_.draw_frame(frame_);
    SDL_Delay(16);
  }
  render_system_.wait_idle();
}

void App::create_frame_packet_() {
  std::hash<std::string> hash{};
  size_t id = hash("triangle_vertex_buffer");
  size_t offset = 0;
  std::tuple<uint32_t, uint32_t> window_dimensions =
      render_system_.get_window_dimensions();
  float window_width = static_cast<float>(std::get<0>(window_dimensions));
  float window_height = static_cast<float>(std::get<1>(window_dimensions));

  std::vector<uint8_t> pass_uniform_data(sizeof(PassUniforms));
  PassUniforms* pass_uniforms =
      reinterpret_cast<PassUniforms*>(pass_uniform_data.data());
  pass_uniforms->view_matrix =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  pass_uniforms->projection_matrix = glm::perspective(
      glm::radians(45.0f), window_width / window_height, 0.1f, 10.0f);
  render::Frame::UniformBlock pass_uniform_block{pass_uniform_data,
                                                 static_cast<uint32_t>(offset)};

  offset += pass_uniform_data.size();

  std::vector<uint8_t> object_uniform_data(sizeof(ObjectUniforms));
  ObjectUniforms* object_uniforms =
      reinterpret_cast<ObjectUniforms*>(object_uniform_data.data());
  object_uniforms->world_matrix = glm::rotate(
      glm::mat4(1.0f), 1.0f * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  render::Frame::UniformBlock object_uniform_block{
      object_uniform_data, static_cast<uint32_t>(offset)};
  render::Frame::Pass::RenderObject render_object{object_uniform_block, id};

  render::Frame::Pass pass{pass_uniform_block, {render_object}};

  frame_.passes.clear();
  frame_.passes.push_back(pass);
}