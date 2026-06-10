#ifndef ENGINE_GPU_H
#define ENGINE_GPU_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "linalg.h"
#include "model.h"

/* Owns the SDL GPU device and everything needed to draw one model:
   the graphics pipeline, a sampler, and a lazily (re)created depth target. */
typedef struct {
    SDL_GPUDevice           *device;
    SDL_Window              *window;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUSampler          *sampler;
    SDL_GPUTexture          *depth_texture;
    Uint32                   depth_w;
    Uint32                   depth_h;
} Gpu;

bool gpu_init(Gpu *g, SDL_Window *window);
void gpu_shutdown(Gpu *g);

/* Renders one frame: clears, draws `model` with the given view-projection
   matrix, and presents. Returns false on a fatal error. */
bool gpu_draw(Gpu *g, const Model *model, mat4 viewproj);

#endif /* ENGINE_GPU_H */
