#ifndef ENGINE_SHADERS_H
#define ENGINE_SHADERS_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/* Initializes / shuts down the SDL_shadercross HLSL compiler. */
bool shaders_init(void);
void shaders_quit(void);

/* Compiles the embedded HLSL model shader for the given stage into a GPU
   shader for `device`. Returns NULL (and logs) on failure. */
SDL_GPUShader *shader_compile(SDL_GPUDevice *device, bool is_vertex);

#endif /* ENGINE_SHADERS_H */
