#pragma once

#include <atomic>
#include <externals.h>

typedef void *(*ImGuiMemAllocFunc)(size_t sz, void *user_data);
typedef void (*ImGuiMemFreeFunc)(void *ptr, void *user_data);

typedef void *(*GLADloadproc)(const char *name);

typedef struct game {
	std::atomic<bool> play{true};
	struct GLFWwindow *window;
	struct ImGuiContext *ctx;
	ImGuiMemAllocFunc alloc_func;
	ImGuiMemFreeFunc free_func;
	void *user_data;
	void *engine_lib;
	void *buffer;
	size_t buffer_size;

	GLADloadproc loader;
	hotreloadable_imgui_draw_func g_imguiUpdate = nullptr;
	void_pGame_func begin_frame = nullptr;
	void_pGame_func update = nullptr;
	void_pGame_func compile_shaders = nullptr;
	double deltaTime = 0.0;
} game;
