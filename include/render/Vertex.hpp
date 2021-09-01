#pragma once

namespace render {
struct Vertex {
  glm::vec2 position;
  glm::vec3 color;

  static VkVertexInputBindingDescription get_binding_description() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
  }

  static std::array<VkVertexInputAttributeDescription, 2>
  get_attribute_descriptions() {
    std::array<VkVertexInputAttributeDescription, 2> descs{};
    descs[0].binding = 0;
    descs[0].location = 0;
    descs[0].format = VK_FORMAT_R32G32_SFLOAT;
    descs[0].offset = offsetof(Vertex, position);
    descs[1].binding = 0;
    descs[1].location = 1;
    descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    descs[1].offset = offsetof(Vertex, color);
    return descs;
  }
};
}  // namespace render