#version 450

/* SDL GPU SPIR-V convention: fragment sampled textures live in set 2. */
layout(set = 2, binding = 0) uniform sampler2D u_tex;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 n = normalize(v_normal);
    vec3 light_dir = normalize(vec3(0.4, 0.9, 0.5));
    float diffuse = max(dot(n, light_dir), 0.0);
    float ambient = 0.3;
    vec4 tex = texture(u_tex, v_uv);
    vec3 rgb = tex.rgb * (ambient + 0.8 * diffuse);
    out_color = vec4(rgb, 1.0);
}
