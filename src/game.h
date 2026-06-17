#ifndef GAME_H
#define GAME_H

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Input {
	float mouse_x;
	float mouse_y;
	uint32_t mouse_buttons;
	bool keys[SDL_SCANCODE_COUNT];
} Input;

typedef struct GameMemory {
	void *permanent;
	size_t permanent_size;

	SDL_Window *window;
	SDL_GPUDevice *device;

	Input input;
	float dt;
	uint64_t frame;

	bool initialized;
	bool reloaded;
} GameMemory; 

typedef void (*GameInitFn)(GameMemory *mem);
typedef void (*GameUpdateFn)(GameMemory *mem);

#define GAME_INIT_SYMBOL "game_init"
#define GAME_UPDATE_SYMBOL "game_update"

#endif // !GAME_H
