#pragma once

#include "render/vulkan/Buffer.hpp"

namespace render {
struct Mesh {
  std::unique_ptr<vulkan::Buffer> vertex_buffer;
  std::unique_ptr<vulkan::Buffer> index_buffer;
};
}  // namespace render