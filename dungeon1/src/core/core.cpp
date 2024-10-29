#include <toml.h>

#include "core.h"
#include "loadlibrary.h"
#include <engine.h>
#include <externals.h>
#include <game.h>
#include <stdio.h>

#include <Windows.h>

#include <iostream>
#include <thread>

#include <filesystem>

#include <fstream>
#include <string>
#include <sstream>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

std::atomic<bool> reloadFlag(false);

void waitforreloadsignal(HANDLE hEvent) {

  std::cout << "Current wating for reload signal" << std::endl;
  while (true) {
    DWORD dwWaitResult = WaitForSingleObject(hEvent, INFINITE);
    if (dwWaitResult == WAIT_OBJECT_0) {
      printf("Hot reload signal received\n");
      reloadFlag.store(true);
      break;
    }
  }
}


std::string getCurrentWorkingDirectory() {
  char buffer[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, buffer);
  return std::string(buffer);
}

void write_scene_toml(const Scene *scene, const char *file_path) {
    FILE *file = fopen(file_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open file for writing: %s\n", file_path);
        return;
    }

    // Write camera data
    fprintf(file, "[camera]\n");
    fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->camera.position.x, scene->camera.position.y, scene->camera.position.z);
    fprintf(file, "fov = %.2f\n\n", scene->camera.fov);

    // Write light data
    fprintf(file, "[light]\n");
    fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->light.position.x, scene->light.position.y, scene->light.position.z);
    fprintf(file, "color = { r = %.2f, g = %.2f, b = %.2f }\n", scene->light.color.x, scene->light.color.y, scene->light.color.z);
    fprintf(file, "intensity = %.2f\n\n", scene->light.intensity);

    // Write mesh data
    fprintf(file, "[mesh]\n");
    fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->mesh.position.x, scene->mesh.position.y, scene->mesh.position.z);
    fprintf(file, "scale = { x = %.2f, y = %.2f, z = %.2f }\n", scene->mesh.scale.x, scene->mesh.scale.y, scene->mesh.scale.z);
    fprintf(file, "rotation = { pitch = %.2f, yaw = %.2f, roll = %.2f }\n", scene->mesh.rotation.x, scene->mesh.rotation.y, scene->mesh.rotation.z);
    fprintf(file, "file = \"%s\"\n", scene->mesh.file);

    fclose(file);
}

EXPORT void init() {
  printf("Core initialized\n");

  std::string cwd = getCurrentWorkingDirectory();
  std::cout << "Current working directory: " << cwd << std::endl;

  HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, HOTRELOAD_EVENT_NAME);
  if (hEvent == NULL) {
    std::cerr << "CreateEvent failed (" << GetLastError() << ")" << std::endl;
  }
  std::thread signalThread(waitforreloadsignal, hEvent);
  signalThread.detach();

  game g;

  std::string src = cwd + "\\build\\debug\\engine.dll";
  std::string dest = cwd + "\\build\\debug\\engine_copy.dll";
  std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);

  g.engine_lib = loadlibrary("engine_copy");

  assign_hotreloadable((hotreloadable_imgui_draw_func)getfunction(
      g.engine_lib, "hotreloadable_imgui_draw"));

  init_externals(&g);

  begin_watch_src_directory(g);

  Scene scene = {
        .camera = { .position = {0.0f, 1.0f, 5.0f}, .fov = 45.0f },
        .light = { .position = {10.0f, 10.0f, 10.0f}, .color = {1.0f, 1.0f, 1.0f}, .intensity = 2.0f },
        .mesh = { .position = {0.0f, 0.0f, 0.0f}, .scale = {1.0f, 1.0f, 1.0f}, .rotation = {0.0f, 0.0f, 0.0f}, .file = "assets/models/cube.obj" }
    };

    write_scene_toml(&scene, "scene.toml");
  
   FILE *fp; 
   
  char errbuf[200];

   fp = fopen("scene.toml", "r");
  if (!fp) {
    printf("cannot open sample.toml - ", strerror(errno));
  }

  toml_table_t *conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
  fclose(fp);

  if (!conf) {
    printf("cannot parse - ", errbuf);
  }

  for (int i = 0; ; i++) {
        // Get the key (name) of the table at index i
        const char *key = toml_key_in(conf, i);
        if (!key) {
            // Break when no more keys are found
            break;
        }

        printf("Found table: %s\n", key);

        // If there's a nested table, you could further explore it here
        toml_table_t *nested = toml_table_in(conf, key);
        if (nested) {
            printf("  %s is a table with nested values.\n", key);
            // Recursively list nested tables if needed
        }
    }
    
  begin_game_loop(g);
}


void compile_dll() {
  std::string cwd = getCurrentWorkingDirectory();
  std::string command =
      "cd /d " + cwd +
      " && build_engine.bat"; // Use /d to change the drive as well
  std::cout << "Compiling DLL with command: " << command << std::endl;
  system(command.c_str());
}

void copy_dll(const std::string &src, const std::string &dest) {
  std::filesystem::copy_file(src, dest,
                             std::filesystem::copy_options::overwrite_existing);
}

void watch_src_directory() {

  std::cout << "inside watch_src_directory" << std::endl;
  HANDLE hDir = CreateFile(
      "src", FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

  if (hDir == INVALID_HANDLE_VALUE) {
    std::cerr << "CreateFile failed (" << GetLastError() << ")" << std::endl;
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

  std::cout << "handle is valid being watch loop" << std::endl;
  while (true) {
    if (ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned, &overlapped, NULL)) {
      WaitForSingleObject(overlapped.hEvent, INFINITE);
      FILE_NOTIFY_INFORMATION *pNotify;
      int offset = 0;
      do {
        pNotify = (FILE_NOTIFY_INFORMATION *)((char *)buffer + offset);
        std::wstring fileName(pNotify->FileName,
                              pNotify->FileNameLength / sizeof(WCHAR));
        std::wcout << L"Change detected in: " << fileName << std::endl;
        offset += pNotify->NextEntryOffset;
      } while (pNotify->NextEntryOffset);

      compile_dll();

      ResetEvent(overlapped.hEvent);
      reloadFlag.store(true);
    }
  }
}

void begin_watch_src_directory(game &g) {

  std::cout << "calling watch_src_directory" << std::endl;
  std::thread watchThread(watch_src_directory);
  watchThread.detach();
}

void begin_game_loop(game &g) {
  while (g.play) {
    if (reloadFlag.load()) {
      reloadFlag.store(false);
      printf("Reloading...\n");
      unloadlibrary(g.engine_lib);

      std::string cwd = getCurrentWorkingDirectory();
      std::string src = cwd + "\\build\\Debug\\engine.dll";
      std::string dest = cwd + "\\build\\Debug\\engine_copy.dll";
      std::filesystem::copy_file(
          src, dest, std::filesystem::copy_options::overwrite_existing);

      g.engine_lib = loadlibrary("engine_copy");
      assign_hotreloadable((hotreloadable_imgui_draw_func)getfunction(
          g.engine_lib, "hotreloadable_imgui_draw"));
    }
    update_externals(&g);
  }
}
