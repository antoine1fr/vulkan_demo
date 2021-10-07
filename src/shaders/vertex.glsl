#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangent;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 uv_out;

layout (set = 0, binding = 0) uniform PassUniforms {
  mat4 view_matrix;
  mat4 projection_matrix;
} pass_uniforms;

layout (set = 0, binding = 1) uniform ObjectUniforms {
  mat4 world_matrix;
} object_uniforms;

void main() {
  gl_Position = pass_uniforms.projection_matrix
    * pass_uniforms.view_matrix
    * object_uniforms.world_matrix
    * vec4(position, 1.0);
  frag_color = color;
  uv_out = uv;
}
