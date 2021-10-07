#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>

namespace render {
struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
  glm::vec3 tangent;
  glm::vec3 bitangent;

  static VkVertexInputBindingDescription get_binding_description() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
  }

  static std::vector<VkVertexInputAttributeDescription>
  get_attribute_descriptions() {
    std::vector<VkVertexInputAttributeDescription> desc{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)},
        {5, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, bitangent)}};
    return desc;
  }
};
}  // namespace render