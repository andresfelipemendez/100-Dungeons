#include <cmath>
#define WIN32_LEAN_AND_MEAN
#include "core.h"
#include "loadlibrary.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <engine.h>
#include <externals.h>
#include <functional>
#include <game.h>
#include <iostream>
#include <printLog.h>
#include <stdio.h>
#include <string>
#include <thread>

HMODULE externals_lib = nullptr;

init_externals_func init_externals_ptr = nullptr;
update_externals_func update_externals_ptr = nullptr;
end_externals_func end_externals_ptr = nullptr;

std::atomic<bool> reloadEngineFlag(false);
std::atomic<bool> reloadEditorFlag(false);
std::atomic<bool> reloadExternalsFlag(false);

constexpr auto DEBOUNCE_INTERVAL_MS = 2000;

static game g;

void getCurrentWorkingDirectory(char *buffer, size_t size) {
	GetCurrentDirectoryA((DWORD)size, buffer);
}

bool copy_dll(const char *dll_name, char *dest, size_t dest_size) {
	char cwd[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, cwd);

	char src[MAX_PATH];
	snprintf(src, sizeof(src), "%s\\%s.dll", cwd, dll_name);
	snprintf(dest, dest_size, "%s\\%s_copy.dll", cwd, dll_name);

	if (!CopyFileA(src, dest, FALSE)) {
		printf("Could not copy DLL %s: Error %lu\n", src, GetLastError());
		return false;
	}
	return true;
}

void compile_engine_dll() {
	char cwd[MAX_PATH];
	getCurrentWorkingDirectory(cwd, MAX_PATH);
	char command[1024];
	snprintf(command, sizeof(command), "\"%s\\build_engine.bat\"", cwd);

	system(command);
}

void compile_externals_dll() {
	print_log(COLOR_YELLOW, "compiling externals.dll");

	char cwd[MAX_PATH];
	getCurrentWorkingDirectory(cwd, MAX_PATH);

	char command[1024];
	snprintf(command, sizeof(command), "\"%s\\build_externals.bat\"", cwd);

	system(command);
}

void compile_editor_dll() {
	print_log(COLOR_YELLOW, "compiling editor.dll");

	char cwd[MAX_PATH];
	getCurrentWorkingDirectory(cwd, MAX_PATH);

	char command[1024];
	snprintf(command, sizeof(command), "\"%s\\build_editor.bat\"", cwd);

	system(command);
}

void shutdown_externals(game &g) { end_externals_ptr(&g); }

void unload_externals(game &g) {
	print_log(COLOR_YELLOW, "Unloading externals.dll\n");
	shutdown_externals(g);
	unloadlibrary(externals_lib);
	externals_lib = nullptr;
	printf("unload_externals \n");
}

void load_function_pointers(game &g) {
	init_externals_ptr =
		(init_externals_func)getfunction(externals_lib, "init_externals");
	init_externals_ptr(&g);
	g.begin_frame(&g);
	load_meshes_func load_meshes =
		(load_meshes_func)getfunction(g.engine_lib, "load_meshes");
	load_meshes(&g);
	g.update = (draw_opengl_func)getfunction(g.engine_lib, "update");
	update_externals_ptr =
		(update_externals_func)getfunction(externals_lib, "update_externals");
	end_externals_ptr =
		(end_externals_func)getfunction(externals_lib, "end_externals");
}

void reload_externals(game &g) {
	print_log("Reloading externals.dll", COLOR_GREEN);
	char copiedExternalsDllPath[MAX_PATH];
	if (!copy_dll("externals", copiedExternalsDllPath,
				  sizeof(copiedExternalsDllPath))) {
		return;
	}
	externals_lib = (HMODULE)loadlibrary(copiedExternalsDllPath);
	if (externals_lib) {
		load_function_pointers(g);
	} else {
		print_log(COLOR_RED, "Failed to load externals_copy.dll\n");
	}

	g.update = (draw_opengl_func)getfunction(g.engine_lib, "update");
}

void directory_watch_function(game &g, const std::string &directory,
							  std::function<void()> onChange) {
	HANDLE hDir = CreateFileA(
		directory.c_str(), FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

	if (hDir == INVALID_HANDLE_VALUE) {
		std::cerr << "CreateFile failed for directory: " << directory << " ("
				  << GetLastError() << ")" << std::endl;
		return;
	}

	char buffer[1024];
	DWORD bytesReturned;
	OVERLAPPED overlapped = {};
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (overlapped.hEvent == NULL) {
		std::cerr << "CreateEvent failed (" << GetLastError() << ")"
				  << std::endl;
		CloseHandle(hDir);
		return;
	}
	auto lastCompilationTime = std::chrono::steady_clock::now();

	while (g.play.load()) {
		if (ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), TRUE,
								  FILE_NOTIFY_CHANGE_FILE_NAME |
									  FILE_NOTIFY_CHANGE_LAST_WRITE,
								  &bytesReturned, &overlapped, NULL)) {
			WaitForSingleObject(overlapped.hEvent, INFINITE);

			auto currentTime = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(
					currentTime - lastCompilationTime)
					.count() >= DEBOUNCE_INTERVAL_MS) {
				lastCompilationTime = currentTime;
				onChange();
			}
			ResetEvent(overlapped.hEvent);
		}
	}
}

void begin_game_loop(game &g) {

	std::cout << "Starting game loop." << std::endl;
	while (g.play.load()) {

		if (reloadEngineFlag.load()) {
			reloadEngineFlag.store(false);
			print_log(COLOR_YELLOW, "Reloading Engine...\n");

			unloadlibrary(g.engine_lib);
			char copiedEgnineDllPath[MAX_PATH];
			if (!copy_dll("engine", copiedEgnineDllPath,
						  sizeof(copiedEgnineDllPath))) {
				return;
			}
			printf("loading lib: %s\n", copiedEgnineDllPath);
			g.engine_lib = loadlibrary(copiedEgnineDllPath);
			init_engine_func init_engine =
				(init_engine_func)getfunction(g.engine_lib, "init_engine");
			g.g_imguiUpdate = (hotreloadable_imgui_draw_func)getfunction(
				g.engine_lib, "hotreloadable_imgui_draw");
			init_engine(&g);

			g.begin_frame =
				(begin_frame_func)getfunction(g.engine_lib, "begin_frame");
			g.begin_frame(&g);

			g.update = (draw_opengl_func)getfunction(g.engine_lib, "update");
			load_meshes_func load_meshes =
				(load_meshes_func)getfunction(g.engine_lib, "load_meshes");
			load_meshes(&g);
		}

		if (reloadExternalsFlag.load()) {
			reloadExternalsFlag.store(false);
			unload_externals(g);
			reload_externals(g);
		}

		if (g.play.load()) {
			g.update(&g);
		} else {
			break;
		}

		if (g.play.load()) {
			update_externals_ptr(&g);
		} else {
			break;
		}
	}

	std::cout << "Ending game loop..." << std::endl;
	unloadlibrary(g.engine_lib);
	unload_externals(g);
	printf("core loop ended\n");
}

EXPORT void stop() {
	g.play.store(false);
	printf("stop core loop\n");
}

void begin_watch(game &g, const std::string &directory,
				 std::function<void()> onChange) {
	std::thread watchThread([&g, directory, onChange]() {
		directory_watch_function(g, directory, onChange);
	});
	watchThread.detach();
}

EXPORT void init() {
	char cwd[MAX_PATH];
	getCurrentWorkingDirectory(cwd, MAX_PATH);
	printf("Current working directory: %s\n", cwd);

	HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, HOTRELOAD_EVENT_NAME);
	if (hEvent == NULL) {
		printf("CreateEvent failed (%lu)\n", GetLastError());
	}

	std::thread signalThread(waitforreloadsignal, hEvent);
	signalThread.detach();

	g.buffer_size = 100 * 1024;
	g.buffer = malloc(g.buffer_size);
	if (g.buffer != NULL) {
		memset(g.buffer, 0, g.buffer_size);
	}

	char copiedEgnineDllPath[MAX_PATH];
	if (!copy_dll("engine", copiedEgnineDllPath, sizeof(copiedEgnineDllPath))) {
		return;
	}

	g.engine_lib = loadlibrary(copiedEgnineDllPath);

	init_engine_func init_engine =
		(init_engine_func)getfunction(g.engine_lib, "init_engine");
	init_engine(&g);
	load_level_func load_level =
		(load_level_func)getfunction(g.engine_lib, "load_level");
	load_level(&g, "assets\\scene.toml");

	g.begin_frame = (begin_frame_func)getfunction(g.engine_lib, "begin_frame");
	g.g_imguiUpdate = (hotreloadable_imgui_draw_func)getfunction(
		g.engine_lib, "hotreloadable_imgui_draw");

	char copiedDllPath[MAX_PATH];
	if (!copy_dll("externals", copiedDllPath, sizeof(copiedDllPath))) {
		return;
	}
	externals_lib = (HMODULE)loadlibrary(copiedDllPath);

	init_externals_func init_externals =
		(init_externals_func)getfunction(externals_lib, "init_externals");
	init_externals(&g);

	g.begin_frame(&g);
	load_meshes_func load_meshes =
		(load_meshes_func)getfunction(g.engine_lib, "load_meshes");
	load_meshes(&g);
	g.update = (draw_opengl_func)getfunction(g.engine_lib, "update");
	begin_watch(g, "../../src/editor", [&]() {
		compile_editor_dll();
		reloadEditorFlag.store(true);
	});
	begin_watch(g, "../../src/engine", [&]() {
		compile_engine_dll();
		reloadEngineFlag.store(true);
	});
	begin_watch(g, "../../src/externals", [&]() {
		compile_externals_dll();
		reloadExternalsFlag.store(true);
	});

	update_externals_ptr =
		(update_externals_func)getfunction(externals_lib, "update_externals");
	end_externals_ptr =
		(end_externals_func)getfunction(externals_lib, "end_externals");

	// load_function_pointers(g);
	begin_game_loop(g);
}

void waitforreloadsignal(void *hEvent) {
	HANDLE eventHandle = static_cast<HANDLE>(hEvent);
	while (g.play.load()) {
		DWORD dwWaitResult = WaitForSingleObject(eventHandle, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0) {
			print_log(COLOR_GREEN, "Hot reload signal received");
			reloadEngineFlag.store(true);
			std::this_thread::sleep_for(
				std::chrono::milliseconds(DEBOUNCE_INTERVAL_MS));
			break;
		}
	}
}