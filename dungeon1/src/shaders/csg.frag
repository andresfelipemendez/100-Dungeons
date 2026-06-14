#version 450

/* triplanar grid: sample the grid texture on the three world planes (yz, xz,
   xy) and blend by the squared surface normal, so every CSG face -- whatever
   its orientation, with no UVs -- shows a clean square grid. SDL GPU: sampled
   texture in set 2. */
layout(set = 2, binding = 0) uniform sampler2D u_tex;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world;

layout(location = 0) out vec4 out_color;

const float SCALE = 1.5; /* texture tiles per world unit */

void main() {
    vec3 n = normalize(v_normal);
    /* sharpen the blend (pow 4) so a 45-degree face snaps to its dominant axis
       instead of ghosting all three grids together */
    vec3 w = pow(abs(n), vec3(4.0));
    w = w / max(w.x + w.y + w.z, 1e-4);

    vec3 gx = texture(u_tex, v_world.yz * SCALE).rgb;
    vec3 gy = texture(u_tex, v_world.xz * SCALE).rgb;
    vec3 gz = texture(u_tex, v_world.xy * SCALE).rgb;
    vec3 grid = gx * w.x + gy * w.y + gz * w.z;

    vec3 light_dir = normalize(vec3(0.4, 0.9, 0.5));
    float diffuse = max(dot(n, light_dir), 0.0);
    vec3 rgb = grid * (0.35 + 0.75 * diffuse);
    out_color = vec4(rgb, 1.0);
}
