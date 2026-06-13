#version 450

/* SDL GPU SPIR-V convention: vertex uniform buffers live in set 1. */
layout(set = 1, binding = 0) uniform UBO {
    vec2 u_screen; /* swapchain size in pixels */
};

layout(location = 0) in vec2 in_position; /* pixels, y-down, origin top-left */
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

void main() {
    vec2 ndc = vec2(2.0 * in_position.x / u_screen.x - 1.0,
                    1.0 - 2.0 * in_position.y / u_screen.y);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = in_uv;
    v_color = in_color;
}
