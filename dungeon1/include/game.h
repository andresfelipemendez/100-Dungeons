#pragma once

typedef void *(*ImGuiMemAllocFunc)(size_t sz, void *user_data);
typedef void (*ImGuiMemFreeFunc)(void *ptr, void *user_data);

typedef void *(*GLADloadproc)(const char *name);

#include <externals.h>

typedef struct game {
	int play;
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
	begin_frame_func begin_frame = nullptr;
	draw_opengl_func draw_opengl = nullptr;
	void_pGame_func update = nullptr;
} game;
