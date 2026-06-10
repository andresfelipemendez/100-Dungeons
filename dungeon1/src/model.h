#ifndef ENGINE_MODEL_H
#define ENGINE_MODEL_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "linalg.h"

/* A single drawable mesh uploaded to the GPU: all primitives of all nodes in
   the glTF file are baked (with node world transforms applied) into one
   interleaved vertex buffer plus a 32-bit index buffer. */
typedef struct {
    SDL_GPUBuffer  *vertex_buffer;
    SDL_GPUBuffer  *index_buffer;
    SDL_GPUTexture *texture;
    Uint32          index_count;
    vec3            bounds_min;
    vec3            bounds_max;
} Model;

/* Loads geometry from `glb_path` and the color texture from `texture_path`.
   Returns false (and logs the cause) on any failure. */
bool model_load(Model *model, SDL_GPUDevice *device,
                const char *glb_path, const char *texture_path);

void model_destroy(Model *model, SDL_GPUDevice *device);

#endif /* ENGINE_MODEL_H */
