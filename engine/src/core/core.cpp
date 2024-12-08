#include "core.h"
#include "loadlibrary.h"

#include <atomic>
#include <chrono>
#include <engine.h>
#include <externals.h>
#include <functional>
#include <game.h>
#include <iostream>
#include <mutex>
#include <printLog.h>
#include <stdio.h>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>

HMODULE externals_lib = nullptr;

int_pGame_func init_externals_ptr = nullptr;
void_pGame_func update_externals_ptr = nullptr;
void_pGame_func end_externals_ptr = nullptr;

void_pGamepChar_func load_level_ptr = nullptr;
void_pGamepChar_func asset_reload_ptr = nullptr;
void_pGame_func init_engine_ptr = nullptr;

std::atomic<bool> reloadEngineFlag(false);
std::atomic<bool> reloadEditorFlag(false);
std::atomic<bool> reloadExternalsFlag(false);
std::atomic<bool> reloadAssetsFlag(false);

std::mutex mutex;
std::string assetReloaded;

constexpr auto DEBOUNCE_INTERVAL_MS = 4000;

static game g;

void getCurrentWorkingDirectory(char *buffer, size_t size) {
  GetCurrentDirectoryA((DWORD)size, buffer);
}

void_pGame_func getEngineFunction(const game &g, const char *functionName) {
  return (void_pGame_func)getfunction(g.engine_lib, functionName);
}

void_pGamepChar_func loadExternalFunction(const char *functionName) {
  return (void_pGamepChar_func)getfunction(externals_lib, functionName);
}

void_pGame_func loadExternalFunctionpGame(const char *functionName) {
  return (void_pGame_func)getfunction(externals_lib, functionName);
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

void unload_externals(game &g) {
  print_log(COLOR_YELLOW, "Unloading externals.dll\n");
  end_externals_ptr(&g);
  unloadlibrary(externals_lib);
  externals_lib = nullptr;
  printf("unload_externals \n");
}

void load_function_pointers(game &g) {
  g.update = getEngineFunction(g, "update");
  g.begin_frame = getEngineFunction(g, "begin_frame");
  g.draw_editor = getEngineFunction(g, "draw_editor");
  g.load_mesh = loadExternalFunction("load_mesh");
  g.load_shaders = loadExternalFunctionpGame("load_shaders");

  init_engine_ptr = getEngineFunction(g, "init_engine");

  init_externals_ptr =
      (int_pGame_func)getfunction(externals_lib, "init_externals");
  update_externals_ptr =
      (void_pGame_func)getfunction(externals_lib, "update_externals");
  end_externals_ptr =
      (void_pGame_func)getfunction(externals_lib, "end_externals");

  load_level_ptr =
      (void_pGamepChar_func)getfunction(g.engine_lib, "load_level");
  asset_reload_ptr =
      (void_pGamepChar_func)getfunction(g.engine_lib, "asset_reload");
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
    init_externals_ptr(&g);
  } else {
    print_log(COLOR_RED, "Failed to load externals_copy.dll\n");
  }
}

void directory_watch_function(game &g, const std::string &directory,
                              std::function<void(std::string path)> onChange) {
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
    std::cerr << "CreateEvent failed (" << GetLastError() << ")" << std::endl;
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

        FILE_NOTIFY_INFORMATION *fni =
            reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer);
        do {
          // Get the file name from FILE_NOTIFY_INFORMATION
          std::wstring fileNameW(fni->FileName,
                                 fni->FileNameLength / sizeof(WCHAR));
          std::string fileName(fileNameW.begin(), fileNameW.end());

          // Construct full path
          std::string fullPath = directory + "/" + fileName;

          // Call the callback
          onChange(fullPath);

          // Move to the next entry if present
          if (fni->NextEntryOffset == 0)
            break;
          fni = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(
              reinterpret_cast<char *>(fni) + fni->NextEntryOffset);
        } while (true);
      }
      ResetEvent(overlapped.hEvent);
    }
  }

  CloseHandle(hDir);
  CloseHandle(overlapped.hEvent);

  printf("exiting directory_watch_function\n");
}

void begin_game_loop(game &g) {
  printf("Starting game loop.\n");
  auto lastTime = std::chrono::high_resolution_clock::now();
  while (g.play.load()) {

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedTime = currentTime - lastTime;
    g.deltaTime = elapsedTime.count();
    lastTime = currentTime;
    if (g.play.load()) {
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

        load_function_pointers(g);
        init_engine_ptr(&g);
      }
    } else {
      break;
    }

    if (g.play.load()) {
      if (reloadExternalsFlag.load()) {
        reloadExternalsFlag.store(false);
        unload_externals(g);
        reload_externals(g);
      }
    } else {
      break;
    }
    if (g.play.load()) {
      g.begin_frame(&g);
      if (reloadAssetsFlag.load()) {

        {
          std::string pathToReload;
          std::lock_guard<std::mutex> lock(mutex);
          reloadAssetsFlag.store(false);
          pathToReload = assetReloaded;
          asset_reload_ptr(&g, pathToReload.c_str());
        }
      }
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

  printf("Ending game loop...\n");
  end_externals_ptr(&g);
  printf("Ended externals...\n");
  unloadlibrary(externals_lib);
  printf("Freed externals lib...\n");
  unloadlibrary(g.engine_lib);
  printf("Freed engine lib...\n");

  printf("core loop ended\n");
}

EXPORT void stop() {
  g.play.store(false);
  printf("stop core loop\n");
}

void begin_watch(game &g, const std::string &directory,
                 std::function<void(std::string path)> onChange) {
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
  char copiedDllPath[MAX_PATH];
  if (!copy_dll("externals", copiedDllPath, sizeof(copiedDllPath))) {
    return;
  }
  externals_lib = (HMODULE)loadlibrary(copiedDllPath);

  char copiedEgnineDllPath[MAX_PATH];
  if (!copy_dll("engine", copiedEgnineDllPath, sizeof(copiedEgnineDllPath))) {
    return;
  }
  g.engine_lib = loadlibrary(copiedEgnineDllPath);

  load_function_pointers(g);
  init_externals_ptr(&g);
  init_engine_ptr(&g);
  g.begin_frame(&g);
  load_level_ptr(&g, "assets\\scene.toml");

  begin_watch(g, "../../src/editor", [&](std::string path) {
    compile_editor_dll();
    reloadEditorFlag.store(true);
  });
  begin_watch(g, "../../src/engine", [&](std::string path) {
    compile_engine_dll();
    reloadEngineFlag.store(true);
  });
  begin_watch(g, "../../src/externals", [&](std::string path) {
    compile_externals_dll();
    reloadExternalsFlag.store(true);
  });

  begin_watch(g, "../../assets", [&](std::string path) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      assetReloaded = path;
      reloadAssetsFlag.store(true);
    }
  });

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
