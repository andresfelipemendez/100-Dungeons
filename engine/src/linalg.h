#ifndef ENGINE_LINALG_H
#define ENGINE_LINALG_H

#include <math.h>

/* Matrices are row-major: element (row, col) lives at m[row * 4 + col].
   The HLSL shaders declare their matrices `row_major` to match. */

typedef struct { float x, y, z; } vec3;
typedef struct { float m[16]; } mat4;

static inline vec3 vec3_make(float x, float y, float z) {
    vec3 v = { x, y, z };
    return v;
}
static inline vec3 vec3_add(vec3 a, vec3 b) { return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 vec3_sub(vec3 a, vec3 b) { return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 vec3_scale(vec3 a, float s) { return vec3_make(a.x * s, a.y * s, a.z * s); }
static inline float vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 vec3_cross(vec3 a, vec3 b) {
    return vec3_make(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
}
static inline float vec3_len(vec3 a) { return sqrtf(vec3_dot(a, a)); }
static inline vec3 vec3_norm(vec3 a) {
    float l = vec3_len(a);
    return l > 0.0f ? vec3_scale(a, 1.0f / l) : a;
}

static inline mat4 mat4_identity(void) {
    mat4 r = { { 0 } };
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static inline mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) {
                s += a.m[row * 4 + k] * b.m[k * 4 + col];
            }
            r.m[row * 4 + col] = s;
        }
    }
    return r;
}

/* Transform a position (implicit w = 1). */
static inline vec3 mat4_mul_point(mat4 a, vec3 p) {
    return vec3_make(
        a.m[0] * p.x + a.m[1] * p.y + a.m[2]  * p.z + a.m[3],
        a.m[4] * p.x + a.m[5] * p.y + a.m[6]  * p.z + a.m[7],
        a.m[8] * p.x + a.m[9] * p.y + a.m[10] * p.z + a.m[11]);
}

/* Transform a direction (implicit w = 0). */
static inline vec3 mat4_mul_dir(mat4 a, vec3 d) {
    return vec3_make(
        a.m[0] * d.x + a.m[1] * d.y + a.m[2]  * d.z,
        a.m[4] * d.x + a.m[5] * d.y + a.m[6]  * d.z,
        a.m[8] * d.x + a.m[9] * d.y + a.m[10] * d.z);
}

/* Right-handed perspective projecting depth into [0, 1] (Metal/Vulkan/D3D). */
static inline mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    mat4 r = { { 0 } };
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = zfar / (znear - zfar);
    r.m[11] = (znear * zfar) / (znear - zfar);
    r.m[14] = -1.0f;
    return r;
}

/* Right-handed look-at view matrix. */
static inline mat4 mat4_look_at(vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_norm(vec3_sub(center, eye));
    vec3 s = vec3_norm(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);
    mat4 r = mat4_identity();
    r.m[0] = s.x;  r.m[1] = s.y;  r.m[2]  = s.z;  r.m[3]  = -vec3_dot(s, eye);
    r.m[4] = u.x;  r.m[5] = u.y;  r.m[6]  = u.z;  r.m[7]  = -vec3_dot(u, eye);
    r.m[8] = -f.x; r.m[9] = -f.y; r.m[10] = -f.z; r.m[11] =  vec3_dot(f, eye);
    return r;
}

#endif /* ENGINE_LINALG_H */
