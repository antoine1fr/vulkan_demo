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
#include "render/Vertex.hpp"
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

App::App() : material_id_(std::hash<std::string>{}("some_material")) {
  assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
  std::list<render::UniformBufferDescriptor::Block> blocks{
      {0, 0, sizeof(PassUniforms)},
      {1, sizeof(PassUniforms), sizeof(ObjectUniforms)}};
  render::UniformBufferDescriptor ubo_descriptor{
      sizeof(PassUniforms) + sizeof(ObjectUniforms), blocks};
  CreateFramePacket();
  render_system_.Init(ubo_descriptor);

  std::vector<render::Vertex> vertices{
      {glm::vec2(-0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 1.0f),
       glm::vec2(0.0f, 0.0f)},
      {glm::vec2(0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 1.0f),
       glm::vec2(1.0f, 0.0f)},
      {glm::vec2(-0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 1.0f),
       glm::vec2(0.0f, 1.0f)},
      {glm::vec2(0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 1.0f),
       glm::vec2(1.0f, 1.0f)}};
  std::vector<uint32_t> indices{0, 1, 2, 2, 1, 3};

  render_system_.CreateMesh("quad_mesh", vertices, indices);
  render_system_.LoadMaterial(material_id_, {"../../../assets/yeah.png"});
}

App::~App() {
  render_system_.Cleanup();
  SDL_Quit();
}

void App::Run() {
  bool run = true;

  while (run) {
    SDL_Event event;
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT)
      run = false;
    render_system_.DrawFrame(frame_);
    SDL_Delay(16);
  }
  render_system_.WaitIdle();
}

void App::CreateFramePacket() {
  std::hash<std::string> hash{};
  size_t offset = 0;
  std::tuple<uint32_t, uint32_t> window_dimensions =
      render_system_.GetWindowDimensions();
  float window_width = static_cast<float>(std::get<0>(window_dimensions));
  float window_height = static_cast<float>(std::get<1>(window_dimensions));

  std::vector<uint8_t> pass_uniform_data(sizeof(PassUniforms));
  PassUniforms* pass_uniforms =
      reinterpret_cast<PassUniforms*>(pass_uniform_data.data());
  pass_uniforms->view_matrix =
      glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));
  pass_uniforms->projection_matrix = glm::perspective(
      glm::radians(70.0f), window_width / window_height, 0.1f, 10.0f);
  render::Frame::UniformBlock pass_uniform_block{pass_uniform_data,
                                                 static_cast<uint32_t>(offset)};

  offset += pass_uniform_data.size();

  std::vector<uint8_t> object_uniform_data(sizeof(ObjectUniforms));
  ObjectUniforms* object_uniforms =
      reinterpret_cast<ObjectUniforms*>(object_uniform_data.data());
  object_uniforms->world_matrix = glm::mat4(1.0f);
  render::Frame::UniformBlock object_uniform_block{
      object_uniform_data, static_cast<uint32_t>(offset)};
  render::Frame::Pass::RenderObject render_object{
      object_uniform_block, hash("quad_mesh"), 6, material_id_};

  render::Frame::Pass pass{pass_uniform_block, {render_object}};

  frame_.passes.clear();
  frame_.passes.push_back(pass);
}