#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragColor;

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
    * vec4(position, 0.0, 1.0);
  fragColor = color;
}