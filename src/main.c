#include <SDL3/SDL.h>
#include "game.h"

typedef struct GameCode {
	SDL_SharedObject *handle;
	GameInitFn init;
} GameCode;

static GameCode load_game_code(void) {
	GameCode code;
	SDL_zero(code);

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
	
}
