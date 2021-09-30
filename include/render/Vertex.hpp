#pragma once

#include <glm/glm.hpp>
#include <array>

namespace render {
struct Vertex {
  glm::vec2 position;
  glm::vec3 color;
  glm::vec2 uv;

  static VkVertexInputBindingDescription get_binding_description() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
  }

  static std::array<VkVertexInputAttributeDescription, 3>
  get_attribute_descriptions() {
    std::array<VkVertexInputAttributeDescription, 3> descs{};
    descs[0].binding = 0;
    descs[0].location = 0;
    descs[0].format = VK_FORMAT_R32G32_SFLOAT;
    descs[0].offset = offsetof(Vertex, position);
    descs[1].binding = 0;
    descs[1].location = 1;
    descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    descs[1].offset = offsetof(Vertex, color);
    descs[2].binding = 0;
    descs[2].location = 2;
    descs[2].format = VK_FORMAT_R32G32_SFLOAT;
    descs[2].offset = offsetof(Vertex, uv);
    return descs;
  }
};
}  // namespace render