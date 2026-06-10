#include <SDL3/SDL.h>
#include <stdbool.h>
#include "linalg.h"
#include "camera.h"
#include "gpu.h"
#include "model.h"
#include "shaders.h"

#ifndef ASSETS_DIR
#define ASSETS_DIR "../assets"
#endif

#define DEG2RAD(d) ((d) * 3.14159265358979f / 180.0f)

int main(int argc, char *argv[]) {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Dungeon - SDL GPU", 1280, 720,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    if (!shaders_init()) {
        return 1;
    }

    Gpu gpu;
    if (!gpu_init(&gpu, window)) {
        return 1;
    }

    const char *glb_path = ASSETS_DIR "/Models/GLB format/barrel.glb";
    const char *texture_path = ASSETS_DIR "/Models/GLB format/Textures/colormap.png";
    if (argc > 1) {
        glb_path = argv[1];
    }

    Model model;
    if (!model_load(&model, gpu.device, glb_path, texture_path)) {
        return 1;
    }

    /* Frame the camera around the model's bounding box. */
    vec3 center = vec3_scale(vec3_add(model.bounds_min, model.bounds_max), 0.5f);
    float model_radius = 0.5f * vec3_len(vec3_sub(model.bounds_max, model.bounds_min));
    if (model_radius < 0.001f) {
        model_radius = 1.0f;
    }
    OrbitCamera cam = {
        .target = center,
        .radius = model_radius * 2.4f,
        .angle = 0.0f,
        .pitch = 0.5f,
    };

    bool running = true;
    Uint64 last = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / (float)freq;
        last = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       event.key.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
            }
        }

        cam.angle += dt * 0.6f;

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        float aspect = h > 0 ? (float)w / (float)h : 1.0f;

        mat4 proj = mat4_perspective(DEG2RAD(60.0f), aspect,
                                     model_radius * 0.05f + 0.01f,
                                     model_radius * 20.0f + 10.0f);
        mat4 view = camera_view(&cam);
        mat4 viewproj = mat4_mul(proj, view);

        if (!gpu_draw(&gpu, &model, viewproj)) {
            running = false;
        }
    }

    model_destroy(&model, gpu.device);
    gpu_shutdown(&gpu);
    shaders_quit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
