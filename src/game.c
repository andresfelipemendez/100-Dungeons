#include "game.h"
#include <SDL3/SDL.h>

typedef struct GameState {
	uint64_t frames;
	float t;
	float speed_r;
	float speed_g;
	float speed_b;
} GameState;

void game_init(GameMemory *mem) {
	GameState *s = (GameState *)mem->permanent;
	s->frames = 0;
	s->t = 0.0f;
	s->speed_r = 1.0f;
	s->speed_g = 1.3f;
	s->speed_b = 1.7f;
}

void game_update(GameMemory *mem) {
	GameState *s = (GameState *)mem->permanent;

	if(mem->reloaded) {
		s->speed_r = 1.0f;
		s->speed_g = 1.3f;
		s->speed_b = 1.7f;
	}

	s->frames++;
	s->t += (float)mem->dt;

	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(mem->device);
	if(!cmd){
		return;
	}

	SDL_GPUTexture *swapchain = NULL;
	if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, mem->window, &swapchain, NULL, NULL) || !swapchain) {
		SDL_SubmitGPUCommandBuffer(cmd);
		return;
	}

	float r = 0.5f + 0.5f * SDL_sinf(s->t*s->speed_r);
	float g = 0.5f + 0.5f * SDL_sinf(s->t*s->speed_g + 2.0f);
	float b = 0.5f + 0.5f * SDL_sinf(s->t*s->speed_b + 4.0f);

	SDL_GPUColorTargetInfo target;
	SDL_zero(target);
	target.texture = swapchain;
	target.clear_color = (SDL_FColor){r,g,b,1.0f};
	target.load_op = SDL_GPU_LOADOP_CLEAR;
	target.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
	SDL_EndGPURenderPass(pass);

	SDL_SubmitGPUCommandBuffer(cmd);
}