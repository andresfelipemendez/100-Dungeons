#version 450

/* SDL GPU SPIR-V convention: vertex uniform buffers live in set 1.
   row_major so the CPU-side row-major mat4 (linalg.h) uploads unchanged. */
layout(set = 1, binding = 0, row_major) uniform UBO {
    mat4 u_mvp;
    mat4 u_model;
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;

void main() {
    gl_Position = u_mvp * vec4(in_position, 1.0);
    v_normal = mat3(u_model) * in_normal;
    v_uv = in_uv;
}
