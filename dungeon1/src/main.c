#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3_image/SDL_image.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>   
#include "main.h"

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    int width;
    int height;
    int pitch;
    bool running;
    uint64_t frame_count;
    uint64_t last_frame_time;
    uint64_t current_time;
    float delta_time;
} Engine_State;

typedef struct {
    uint8_t current[SDL_SCANCODE_COUNT];
    uint8_t previous[SDL_SCANCODE_COUNT];
    int mouse_x;
    int mouse_y;
    uint32_t mouse_buttons;
    uint32_t mouse_buttons_previous;
} Input_State;

static Engine_State engine = { 0 };
static Input_State input = { 0 };

static inline uint32_t button_mask(int button) { return 1u << (button - 1); }

static void list_all_recursive(const char* root) {
    int count = 0;
    // pattern: NULL = no filtering. "*" also fine. Uses '/' in results.
    char** items = SDL_GlobDirectory(root, NULL, 0, &count);
    if (!items) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GlobDirectory failed: %s", SDL_GetError());
        return;
    }

    SDL_Log("FOUND %d entries under: %s", count, root);
    for (int i = 0; i < count; i++) {
        SDL_Log("%s", items[i]);
    }
    SDL_free(items); // single allocation, frees strings too
}

static int engine_init(const char* title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    engine.window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    if (!engine.window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    int wx, wy; SDL_GetWindowPosition(engine.window, &wx, &wy);
    float mx, my; SDL_GetGlobalMouseState(&mx, &my);

    Uint32 wflags = SDL_WINDOW_RESIZABLE;
    SDL_Window* tool = SDL_CreateWindow("Tools", 300, 400, wflags);
    if (!tool) {
        SDL_Log("tool window failed: %s", SDL_GetError());
    }
    else {
        SDL_SetWindowParent(tool, engine.window);
        SDL_SetWindowPosition(tool, (int)mx, (int)my);
        SDL_ShowWindow(tool);
        SDL_RaiseWindow(tool);

        SDL_Renderer* toolr = SDL_CreateRenderer(tool, NULL);
        if (toolr) {
            SDL_SetRenderDrawColor(toolr, 32, 32, 32, 255);
            SDL_RenderClear(toolr);
            SDL_RenderPresent(toolr);
            // keep 'tool' and 'toolr' around; handle its SDL_EVENT_WINDOW_RESIZED if you draw into it
        }
    }

    engine.renderer = SDL_CreateRenderer(engine.window, NULL);
    if (!engine.renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(engine.window);
        SDL_Quit();
        return -1;
    }

    engine.width = width;
    engine.height = height;
    engine.pitch = width * (int)sizeof(uint32_t);

    engine.texture = SDL_CreateTexture(
        engine.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );
    if (!engine.texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(engine.renderer);
        SDL_DestroyWindow(engine.window);
        SDL_Quit();
        return -1;
    }

    engine.pixels = (uint32_t*)malloc((size_t)width * (size_t)height * sizeof(uint32_t));
    if (!engine.pixels) {
        SDL_Log("Failed to allocate pixel buffer");
        SDL_DestroyTexture(engine.texture);
        SDL_DestroyRenderer(engine.renderer);
        SDL_DestroyWindow(engine.window);
        SDL_Quit();
        return -1;
    }

    engine.running = true;
    engine.last_frame_time = SDL_GetPerformanceCounter();

    char* cwd = SDL_GetCurrentDirectory();
    list_all_recursive(cwd);
    SDL_free(cwd);
    return 0;
}

static void engine_shutdown(void) {
    free(engine.pixels);
    engine.pixels = NULL;

    if (engine.texture) SDL_DestroyTexture(engine.texture);
    if (engine.renderer) SDL_DestroyRenderer(engine.renderer);
    if (engine.window) SDL_DestroyWindow(engine.window);

    SDL_Quit();
}

static void handle_resize(int width, int height) {
    engine.width = width;
    engine.height = height;
    engine.pitch = width * (int)sizeof(uint32_t);

    if (engine.texture) SDL_DestroyTexture(engine.texture);
    engine.texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, width, height);

    free(engine.pixels);
    engine.pixels = (uint32_t*)malloc((size_t)width * (size_t)height * sizeof(uint32_t));
}

static void process_events(void) {
    memcpy(input.previous, input.current, sizeof(input.current));
    input.mouse_buttons_previous = input.mouse_buttons;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            engine.running = false;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            handle_resize(event.window.data1, event.window.data2);
            break;

        case SDL_EVENT_KEY_DOWN:
            if ((unsigned)event.key.scancode < SDL_SCANCODE_COUNT)
                input.current[event.key.scancode] = 1;
            break;

        case SDL_EVENT_KEY_UP:
            if ((unsigned)event.key.scancode < SDL_SCANCODE_COUNT)
                input.current[event.key.scancode] = 0;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            input.mouse_x = (int)event.motion.x;
            input.mouse_y = (int)event.motion.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            input.mouse_buttons |= button_mask(event.button.button);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            input.mouse_buttons &= ~button_mask(event.button.button);
            break;
        }
    }
}

static inline bool key_pressed(SDL_Scancode key) {
    return input.current[key] && !input.previous[key];
}
static inline bool key_held(SDL_Scancode key) {
    return input.current[key];
}
static inline bool key_released(SDL_Scancode key) {
    return !input.current[key] && input.previous[key];
}

static inline bool mouse_button_pressed(int button) {
    uint32_t m = button_mask(button);
    return (input.mouse_buttons & m) && !(input.mouse_buttons_previous & m);
}
static inline bool mouse_button_held(int button) {
    return (input.mouse_buttons & button_mask(button)) != 0;
}

static void update(float dt) {
    (void)dt;
    if (key_pressed(SDL_SCANCODE_ESCAPE)) engine.running = false;

    if (key_pressed(SDL_SCANCODE_F11)) {
        static bool fullscreen = false;
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(engine.window, fullscreen);
    }
}

static void render(void) {
    memset(engine.pixels, 0, (size_t)engine.width * (size_t)engine.height * sizeof(uint32_t));

    for (int y = 0; y < engine.height; y++) {
        for (int x = 0; x < engine.width; x++) {
            uint32_t color = 0xFF000000;
            float fx = (float)x / (float)engine.width;
            float fy = (float)y / (float)engine.height;
            uint8_t r = (uint8_t)(fx * 255.0f);
            uint8_t g = (uint8_t)(fy * 255.0f);
            uint8_t b = (uint8_t)(engine.frame_count & 0xFF);
            color |= ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            engine.pixels[(size_t)y * (size_t)engine.width + (size_t)x] = color;
        }
    }

    if (mouse_button_held(SDL_BUTTON_LEFT)) {
        int radius = 20;
        int mx = input.mouse_x;
        int my = input.mouse_y;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy <= radius * radius) {
                    int px = mx + dx, py = my + dy;
                    if ((unsigned)px < (unsigned)engine.width && (unsigned)py < (unsigned)engine.height) {
                        engine.pixels[(size_t)py * (size_t)engine.width + (size_t)px] = 0xFFFFFFFFu;
                    }
                }
            }
        }
    }

    SDL_UpdateTexture(engine.texture, NULL, engine.pixels, engine.pitch);
    SDL_RenderClear(engine.renderer);
    SDL_RenderTexture(engine.renderer, engine.texture, NULL, NULL);
    SDL_RenderPresent(engine.renderer);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
    if (engine_init("Game Engine", 1280, 720) != 0) return -1;

    while (engine.running) {
        engine.current_time = SDL_GetPerformanceCounter();
        engine.delta_time =
            (float)(engine.current_time - engine.last_frame_time) /
            (float)SDL_GetPerformanceFrequency();
        engine.last_frame_time = engine.current_time;

        process_events();
        update(engine.delta_time);
        render();

        engine.frame_count++;
    }

    engine_shutdown();
    return 0;
}
