/* Shipping host: the engine subset players get. Window, GPU device, memory,
   input, one statically linked game -- nothing else. No dll loading, no
   file watcher, no kansi, no seni, no editor. If sdl_main.c is the
   workshop, this is the product.

   The game entry is linked statically; the bundle layout mirrors the dev
   tree so game code paths work unchanged: the exe sits at the bundle root
   next to build/ (compiled shaders) and assets/. */

#include <SDL3/SDL.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#define platform_chdir _chdir
#else
#include <unistd.h>
#define platform_chdir chdir
#endif

#include "base/base_types.h"
#include "abi/abi_platform.h"
#include "abi/abi_gpu.h"

#define HOT_SIZE       (1u << 20)   /* 1 MB  */
#define TRANSIENT_SIZE (64u << 20)  /* 64 MB */

/* statically linked from the game's unity build */
GAME_UPDATE_AND_RENDER(game_update_and_render);

static void platform_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args);
    va_end(args);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* the exe sits at the bundle root; anchor all relative paths there */
    {
        const char *base = SDL_GetBasePath();
        if (base && platform_chdir(base) != 0) {
            SDL_Log("warning: cannot chdir to bundle root '%s'", base);
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("Dungeon", 1280, 720,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
#ifdef __APPLE__
    /* modern dyld won't find the Vulkan SDK's loader (/usr/local/lib) from
       a bare dlopen name; point SDL at it explicitly. MoltenVK translates
       to Metal underneath -- the SPIR-V pipeline stays unchanged. */
    if (!SDL_GetHint(SDL_HINT_VULKAN_LIBRARY) &&
        access("/usr/local/lib/libvulkan.dylib", F_OK) == 0) {
        SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "/usr/local/lib/libvulkan.dylib");
    }
#endif
    SDL_GPUDevice *device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice (vulkan) failed: %s", SDL_GetError());
        return 1;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return 1;
    }

    GpuContext gpu_context = { window, device };
    PlatformApi api = { &gpu_context, platform_log };

    PlatformMemory memory = { 0 };
    memory.hot_size = HOT_SIZE;
    memory.hot = calloc(1, HOT_SIZE);
    memory.transient_size = TRANSIENT_SIZE;
    memory.transient = calloc(1, TRANSIENT_SIZE);
    if (!memory.hot || !memory.transient) {
        SDL_Log("memory allocation failed");
        return 1;
    }
    memory.reloaded = 1; /* first frame builds the cold state, exactly once */

    b32 running = 1;
    u64 last = SDL_GetPerformanceCounter();
    u64 freq = SDL_GetPerformanceFrequency();

    while (running) {
        u64 now = SDL_GetPerformanceCounter();
        GameInput input = { 0 };
        input.dt = (f32)(now - last) / (f32)freq;
        last = now;
        {
            float mx = 0, my = 0;
            SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
            input.mouse_x = mx;
            input.mouse_y = my;
            input.mouse_left = (buttons & SDL_BUTTON_LMASK) != 0;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            }
        }

        game_update_and_render(&memory, &api, &input);
        memory.reloaded = 0;
    }

    SDL_WaitForGPUIdle(device);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
