#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define TINYOBJLOADER_IMPLEMENTATION

#include "hash.hpp"
#include "render/MeshLoader.hpp"

namespace render {
bool operator==(const Vertex& lhs, const Vertex& rhs) {
  return (lhs.position == rhs.position && lhs.normal == rhs.normal &&
          lhs.uv == rhs.uv);
}
}  // namespace render

namespace {
struct Index {
  int position;
  int normal;
  int uv;
};

bool operator==(const Index& lhs, const Index& rhs) {
  return (lhs.position == rhs.position && lhs.normal == rhs.normal &&
          lhs.uv == rhs.uv);
}
}  // namespace

namespace std {
template <>
struct hash<render::Vertex> {
  typedef render::Vertex argument_type;
  typedef std::size_t result_type;

  std::size_t operator()(const render::Vertex& vertex) const noexcept {
    std::size_t seed = 0;
    hash_combine(seed, vertex.position);
    hash_combine(seed, vertex.normal);
    hash_combine(seed, vertex.uv);
    return seed;
  }
};

template <>
struct hash<Index> {
  typedef Index argument_type;
  typedef std::size_t result_type;

  std::size_t operator()(const Index& index) const noexcept {
    std::size_t seed = 0;
    hash_combine(seed, index.position);
    hash_combine(seed, index.normal);
    hash_combine(seed, index.uv);
    return seed;
  }
};
}  // namespace std

namespace render {

void MeshLoader::Load(const std::string& path,
                      std::vector<uint32_t>& indices,
                      std::vector<Vertex>& vertices) const {
  std::cout << "Loading mesh from file: " << path << '\n';

  tinyobj::attrib_t attributes;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string error;

  tinyobj::LoadObj(&attributes, &shapes, &materials, &error, path.c_str(),
                   nullptr, true);
  // TODO: implement support for sub-meshes.
  ConsolidateIndices(attributes, shapes[0].mesh.indices, indices, vertices);
}

void MeshLoader::ConsolidateIndices(
    const tinyobj::attrib_t& attributes,
    const std::vector<tinyobj::index_t>& tinyobj_indices,
    std::vector<uint32_t>& indices,
    std::vector<Vertex>& vertices) const {
  // 1. expand vertices
  std::unordered_map<Vertex, uint32_t> vertex_map;
  std::vector<Vertex> expanded_vertices;
  expanded_vertices.reserve(tinyobj_indices.size());
  indices.reserve(tinyobj_indices.size());
  for (const tinyobj::index_t& index : tinyobj_indices) {
    size_t vertex_index = static_cast<size_t>(index.vertex_index) * 3;
    size_t normal_index = static_cast<size_t>(index.normal_index) * 3;
    size_t texcoord_index = static_cast<size_t>(index.texcoord_index) * 2;
    Vertex vertex = Vertex{glm::vec3{attributes.vertices[vertex_index + 0],
                                     attributes.vertices[vertex_index + 1],
                                     attributes.vertices[vertex_index + 2]},
                           glm::vec3{attributes.normals[normal_index + 0],
                                     attributes.normals[normal_index + 1],
                                     attributes.normals[normal_index + 2]},
                           glm::vec3{1.0f, 1.0f, 1.0f},
                           glm::vec2{attributes.texcoords[texcoord_index + 0],
                                     attributes.texcoords[texcoord_index + 1]},
                           glm::vec3(0.0f),
                           glm::vec3(0.0f)};
    expanded_vertices.push_back(vertex);

    uint32_t index;
    if (vertex_map.find(vertex) == vertex_map.cend()) {
      index = static_cast<uint32_t>(vertex_map.size());
      vertex_map[vertex] = index;
    } else {
      index = vertex_map[vertex];
    }
    indices.push_back(index);
  }

  // 2. calculate tangent and bitangent
  ComputeVectors(expanded_vertices);

  // 3. write deduplicated vertices in same order than they appear in index
  // buffer
  vertices.reserve(vertex_map.size());
  vertex_map.clear();
  for (const auto& vertex : expanded_vertices) {
    if (vertex_map.find(vertex) == vertex_map.cend()) {
      vertices.push_back(vertex);
      vertex_map[vertex] = 0;
    }
  }

  // debug traces
  std::cout << "\tvertices: " << vertices.size() << '\n';
  std::cout << "\tindices: " << indices.size() << '\n';
}

glm::vec3 MeshLoader::ComputeTangent(const glm::vec3& dp1,
                                     const glm::vec3& dp2,
                                     const glm::vec2& duv1,
                                     const glm::vec2& duv2,
                                     float kf) const {
  glm::vec3 tangent;
  tangent.x = kf * (duv2.y * dp1.x - duv1.y * dp2.x);
  tangent.y = kf * (duv2.y * dp1.y - duv1.y * dp2.y);
  tangent.z = kf * (duv2.y * dp1.z - duv1.y * dp2.z);
  return glm::normalize(tangent);
}

glm::vec3 MeshLoader::ComputeBitangent(const glm::vec3& dp1,
                                       const glm::vec3& dp2,
                                       const glm::vec2& duv1,
                                       const glm::vec2& duv2,
                                       float kf) const {
  glm::vec3 bitangent;
  bitangent.x = kf * (-duv2.y * dp1.x + duv1.y * dp2.x);
  bitangent.y = kf * (-duv2.y * dp1.y + duv1.y * dp2.y);
  bitangent.z = kf * (-duv2.y * dp1.z + duv1.y * dp2.z);
  return glm::normalize(bitangent);
}

void MeshLoader::ComputeVectors(std::vector<Vertex>& vertices) const {
  size_t triangle_count = vertices.size() / 3;
  for (size_t i = 0; i < triangle_count; i++) {
    size_t offset = i * 3;
    glm::vec3 p1 = vertices[offset + 0].position;
    glm::vec3 p2 = vertices[offset + 1].position;
    glm::vec3 p3 = vertices[offset + 2].position;
    glm::vec2 uv1 = vertices[offset + 0].uv;
    glm::vec2 uv2 = vertices[offset + 1].uv;
    glm::vec2 uv3 = vertices[offset + 2].uv;
    glm::vec3 dp1 = p2 - p1;
    glm::vec3 dp2 = p3 - p1;
    glm::vec2 duv1 = uv2 - uv1;
    glm::vec2 duv2 = uv3 - uv1;
    float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);
    glm::vec3 tangent = ComputeTangent(dp1, dp2, duv1, duv2, f);
    glm::vec3 bitangent = ComputeBitangent(dp1, dp2, duv1, duv2, f);
    vertices[offset + 0].tangent = tangent;
    vertices[offset + 1].tangent = tangent;
    vertices[offset + 2].tangent = tangent;
    vertices[offset + 0].bitangent = bitangent;
    vertices[offset + 1].bitangent = bitangent;
    vertices[offset + 2].bitangent = bitangent;
  }
}

}  // namespace render