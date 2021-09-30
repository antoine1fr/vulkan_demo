#pragma once

#include "base.hpp"

namespace render {
struct Frame {
  struct UniformBlock {
    std::vector<uint8_t> data;
    uint32_t offset;
  };

  struct Pass {
    struct RenderObject {
      UniformBlock uniform_block;
      ResourceId vertex_buffer_id;
      size_t vertex_count;
    };

    UniformBlock uniform_block;
    std::vector<RenderObject> render_objects;
  };

  std::vector<Pass> passes;
};
}  // namespace render