#version 450

/* triplanar CSG: pass world-space position + normal; the frag projects a grid
   texture from the three axes. SDL GPU SPIR-V: vertex UBO in set 1. */
layout(set = 1, binding = 0, row_major) uniform UBO {
    mat4 u_mvp;
    mat4 u_model;
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_world;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_normal = mat3(u_model) * in_normal;
    v_world = (u_model * vec4(in_position, 1.0)).xyz;
}
