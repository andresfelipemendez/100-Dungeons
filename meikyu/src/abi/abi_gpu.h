#ifndef ABI_GPU_H
#define ABI_GPU_H

/* Private contract between the platform layer and the render backend.
   Both store/read SDL handles as void* so the ABI stays backend-agnostic;
   only platform/win32_main.c fills it and render_sdlgpu.c casts it. */

typedef struct GpuContext {
    void *window;  /* SDL_Window*    */
    void *device;  /* SDL_GPUDevice* */
} GpuContext;

#endif /* ABI_GPU_H */
