#version 450

layout (location = 0) in vec3 frag_color;
layout (location = 1) in vec2 uv;

layout (set = 0, binding = 2) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(frag_color, 1.0) * texture(tex_sampler, uv);
}
