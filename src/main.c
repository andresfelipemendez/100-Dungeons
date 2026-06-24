#include <SDL3/SDL.h>
#include "game.h"
#include "watch.h"

#ifndef GAME_LIB_PATH
#define GAME_LIB_PATH "game.dll"
#endif
#ifndef GAME_LIB_TEMP_PATH
#define GAME_LIB_TEMP_PATH "game_active.dll"
#endif

#define ARENA_SIZE ((size_t)256*1024*1024)

typedef struct GameCode {
	SDL_SharedObject *handle;
	GameInitFn init;
	GameUpdateFn update;
	SDL_Time last_write;
	bool valid;
} GameCode;

static SDL_Time source_write_time(void) {
	SDL_PathInfo info;
	if (SDL_GetPathInfo(GAME_LIB_PATH, &info)) {
		return info.modify_time;
	}
	return 0;
}

static GameCode load_game_code(void) {
	GameCode code;
	SDL_zero(code);

	if (!SDL_CopyFile(GAME_LIB_PATH, GAME_LIB_TEMP_PATH)) {
		return code;
	}

	code.handle = SDL_LoadObject(GAME_LIB_TEMP_PATH);
	if (!code.handle) {
		return code;
	}

	code.init = (GameInitFn)SDL_LoadFunction(code.handle, GAME_INIT_SYMBOL);
	code.update = (GameUpdateFn)SDL_LoadFunction(code.handle, GAME_UPDATE_SYMBOL);
	if (!code.update || !code.init) {
		SDL_UnloadObject(code.handle);
		SDL_zero(code);
		return code;
	}

	code.last_write = source_write_time();
	code.valid = true;
	return code;
}

static void unload_game_code(GameCode *code) {
	if (code->handle) {
		SDL_UnloadObject(code->handle);
	}
	SDL_zero(*code);
}

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	SDL_Window *window = SDL_CreateWindow("engine", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		return 1;
	}

	SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
	if (!device) {
		SDL_Log("SDL_CreateGPU failed: %s", SDL_GetError());
		return 1;
	}

	if (!SDL_ClaimWindowForGPUDevice(device, window)) {
		SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
		return 1;
	}

	void *arena = SDL_aligned_alloc(64, ARENA_SIZE);
	if (!arena) {
		SDL_Log("SDL_aligned_alloc failed: %s", SDL_GetError());
		return 1;
	}
	SDL_memset(arena, 0, ARENA_SIZE);

	GameMemory mem;
	SDL_zero(mem);
	mem.permanent = arena;
	mem.permanent_size = ARENA_SIZE;
	mem.window = window;
	mem.device = device;

	GameCode game = load_game_code();
	if (!game.valid) {
		SDL_Log("failed to load game code from %s", GAME_LIB_PATH);
		return 1;
	}

	game.init(&mem);
	mem.initialized = true;

	watch_create(SRC_DIR, on_file_changed)

	const float freq = (float)SDL_GetPerformanceFrequency();
	uint64_t prev = SDL_GetPerformanceCounter();
	bool running = true;
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		int nkeys = 0;
		const bool *ks = SDL_GetKeyboardState(&nkeys);
		SDL_memcpy(mem.input.keys, ks, (size_t)nkeys*sizeof(bool));
		mem.input.mouse_buttons = SDL_GetMouseState(&mem.input.mouse_x, &mem.input.mouse_y);

		const uint64_t now = SDL_GetPerformanceCounter();
		mem.dt = (float)(now - prev)/freq;
		prev = now;

		if (source_write_time() != game.last_write) {
			SDL_WaitForGPUIdle(device);
			unload_game_code(&game);
			for (int attempt = 0; attempt < 100; ++attempt) {
				game = load_game_code();
				if (game.valid) {
					break;
				}
				SDL_Delay(10);
			}
			if (game.valid) {
				mem.reloaded = true;
			} else {
				running = false;
			}
		}

		if (game.valid) {
			game.update(&mem);
			mem.reloaded = false;
			mem.frame++;
		}
	}

	unload_game_code(&game);
	SDL_aligned_free(arena);
	SDL_ReleaseWindowFromGPUDevice(device,window);
	SDL_DestroyGPUDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
