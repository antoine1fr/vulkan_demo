#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "render/Vertex.hpp"
#include "render/tiny_obj_loader.h"

namespace render {

class MeshLoader {
 public:
  void Load(const std::string& path,
            std::vector<uint32_t>& indices,
            std::vector<Vertex>& vertices) const;

 private:
  void ConsolidateIndices(const tinyobj::attrib_t& attributes,
                          const std::vector<tinyobj::index_t>& tinyobj_indices,
                          std::vector<uint32_t>& indices,
                          std::vector<Vertex>& vertices) const;

  void ComputeVectors(std::vector<Vertex>& vertices) const;

  glm::vec3 ComputeTangent(const glm::vec3& dp1,
                           const glm::vec3& dp2,
                           const glm::vec2& duv1,
                           const glm::vec2& duv2,
                           float kf) const;

  glm::vec3 ComputeBitangent(const glm::vec3& dp1,
                             const glm::vec3& dp2,
                             const glm::vec2& duv1,
                             const glm::vec2& duv2,
                             float kf) const;
};

}  // namespace render
