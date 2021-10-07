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
      ResourceId mesh_id;
      ResourceId material_id;
    };

    UniformBlock uniform_block;
    std::vector<RenderObject> render_objects;
  };

  std::vector<Pass> passes;
};
}  // namespace render