#ifndef ENGINE_CAMERA_H
#define ENGINE_CAMERA_H

#include "linalg.h"

/* Camera that circles a fixed target point. `angle` is advanced over time by
   the caller; `pitch` holds the elevation constant. */
typedef struct {
    vec3  target;
    float radius;
    float angle;
    float pitch;
} OrbitCamera;

static inline mat4 camera_view(const OrbitCamera *c) {
    vec3 offset = vec3_make(
        c->radius * cosf(c->pitch) * cosf(c->angle),
        c->radius * sinf(c->pitch),
        c->radius * cosf(c->pitch) * sinf(c->angle));
    vec3 eye = vec3_add(c->target, offset);
    return mat4_look_at(eye, c->target, vec3_make(0.0f, 1.0f, 0.0f));
}

#endif /* ENGINE_CAMERA_H */
