#include "game.h"
#include <SDL3/SDL.h>

typedef struct GameState {
	uint64_t frames;
	float t;
	float speed_r;
} GameState;

void game_init(GameMemory *mem) {
	GameState *s = (GameState *)mem->permanent;
	s->frames = 0;
}

void game_update(GameMemory *mem) {
	GameState *s = (GameState *)mem->permanent;

	if(mem->reloaded) {
		s->speed_r = 1.0f;
	}


}