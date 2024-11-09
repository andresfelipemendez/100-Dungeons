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

#include <chrono>
#include <atomic>
#include <functional>
#include <errno.h>
#include <printLog.h>

HMODULE externals_lib = nullptr; 

void waitforreloadsignal(void* hEvent);
void begin_watch_engine_directory(game &g);
void begin_watch_externals_directory(game &g);
void unload_externals(game &g);
void reload_externals(game &g);

init_externals_func init_externals_ptr  = nullptr;
update_externals_func update_externals_ptr  = nullptr;
end_externals_func end_externals_ptr  = nullptr;

std::atomic<bool> reloadEngineFlag(false);
std::atomic<bool> reloadExternalsFlag(false);

constexpr auto DEBOUNCE_INTERVAL_MS = 500;

namespace fs = std::filesystem;

const char *compiler = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\14.41.34120\\bin\\Hostx64\\x64\\cl.exe";

void getCurrentWorkingDirectory(char* buffer, size_t size)
{
    GetCurrentDirectoryA((DWORD)size, buffer);
}

void copy_dll(const char * dll_name)
{
    char cwd[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, cwd);
    
    char src[MAX_PATH];
    char dest[MAX_PATH];
    snprintf(src, sizeof(src), "%s\\build\\debug\\%s.dll", cwd, dll_name);
    snprintf(dest, sizeof(dest), "%s\\build\\debug\\%s_copy.dll", cwd, dll_name);
    
    if (!CopyFileA(src, dest, FALSE)) {
        printf("Could not copy dll %s: Error %lu\n", src, GetLastError());
    } else {
        printf("DLL copied successfully from %s to %s\n", src, dest);
    }
}

void compile_engine_dll()
{
    char cwd[MAX_PATH];
    getCurrentWorkingDirectory(cwd, MAX_PATH);
    
    char command[1024];
    snprintf(command, sizeof(command),
        "\"%s\" /LD /I%s\\include /Fe:%s\\build\\Debug\\engine.dll %s\\src\\engine\\*.cpp /link /out:%s\\Debug\\engine.dll",
        compiler, cwd,cwd,cwd,cwd);
        
    printf("Compiling Externals DLL with command: %s\n", command);
    system(command);
}

void compile_externals_dll()
{
    char cwd[MAX_PATH];
    getCurrentWorkingDirectory(cwd, MAX_PATH);

    char command[1024];
    snprintf(command, sizeof(command),
        "\"%s\" /LD /I%s\\include /Fe:%s\\build\\Debug\\externals.dll %s\\src\\externals\\*.cpp /link /out:%s\\Debug\\externals.dll",
        compiler, cwd,cwd,cwd,cwd);
        
    printf("Compiling Externals DLL with command: %s\n", command);
    system(command);
}

void shutdown_externals(game &g)
{
    end_externals_ptr(&g);
    std::cout << "Shutting down externals." << std::endl;
}

void unload_externals(game &g)
{
    print_log("Unloading externals.dll", COLOR_YELLOW);
    shutdown_externals(g);
    unloadlibrary(externals_lib);
    externals_lib = nullptr; // Set to nullptr after unloading to prevent accidental use
}


void load_function_pointers(game &g) {
	init_externals_ptr = (init_externals_func)getfunction(externals_lib, "init_externals");
    init_externals_ptr(&g);
	g.begin_frame(&g);
    load_meshes_func load_meshes = (load_meshes_func)getfunction(g.engine_lib, "load_meshes");
    load_meshes(&g);
    g.draw_opengl = (draw_opengl_func)getfunction(g.engine_lib, "draw_opengl");
	update_externals_ptr = (update_externals_func)getfunction(externals_lib, "update_externals");
    end_externals_ptr  = (end_externals_func)getfunction(externals_lib, "end_externals");

    g.draw_opengl = (draw_opengl_func)getfunction(g.engine_lib, "draw_opengl");
    begin_game_loop(g);
}


void reload_externals(game &g)
{
    print_log("Reloading externals.dll", COLOR_GREEN);
    copy_dll("externals");
    externals_lib = (HMODULE)loadlibrary("externals_copy");
    if (externals_lib) {
        load_function_pointers(g);
    } else {
        print_log("Failed to load externals_copy.dll", COLOR_RED);
    }

    g.draw_opengl = (draw_opengl_func)getfunction(g.engine_lib, "draw_opengl");
}

void directory_watch_function(const std::string &directory, std::function<void()> onChange)
{
    HANDLE hDir = CreateFileA(
        directory.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (hDir == INVALID_HANDLE_VALUE)
    {
        std::cerr << "CreateFile failed for directory: " << directory << " (" << GetLastError() << ")" << std::endl;
        return;
    }

    char buffer[1024];
    DWORD bytesReturned;
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (overlapped.hEvent == NULL)
    {
        std::cerr << "CreateEvent failed (" << GetLastError() << ")" << std::endl;
        CloseHandle(hDir);
        return;
    }

    auto lastCompilationTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(DEBOUNCE_INTERVAL_MS);

    while (true)
    {
        if (ReadDirectoryChangesW(
                hDir, buffer, sizeof(buffer), TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                &bytesReturned, &overlapped, NULL))
        {
            WaitForSingleObject(overlapped.hEvent, INFINITE);
            FILE_NOTIFY_INFORMATION *pNotify;
            int offset = 0;
            bool triggerCompilation = false;

            do
            {
                pNotify = (FILE_NOTIFY_INFORMATION *)((char *)buffer + offset);
                std::wstring fileName(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                std::wcout << L"Change detected in: " << fileName << std::endl;

                auto currentTime = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastCompilationTime).count() > DEBOUNCE_INTERVAL_MS)
                {
                    triggerCompilation = true;
                    lastCompilationTime = currentTime;
                }

                offset += pNotify->NextEntryOffset;
            } while (pNotify->NextEntryOffset);

            if (triggerCompilation)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Debounce delay to avoid rapid consecutive builds
                onChange();
            }

            ResetEvent(overlapped.hEvent);
        }
    }
}

void watch_engine_directory()
{
    directory_watch_function("src/engine", []()
                             {
                                 compile_engine_dll();
                                 reloadEngineFlag.store(true);
                             });
}

void watch_externals_directory()
{
    directory_watch_function("src/externals", []()
                             {
                                 compile_externals_dll();
                                 reloadExternalsFlag.store(true);
                             });
}



void begin_game_loop(game &g)
{
    while (g.play)
    {
        if (reloadEngineFlag.load())
        {
            reloadEngineFlag.store(false);
            printf("Reloading Engine...\n");
            unloadlibrary(g.engine_lib);
            copy_dll("engine");

            g.engine_lib = loadlibrary("engine_copy");
            init_engine_func init_engine = (init_engine_func)getfunction(g.engine_lib, "init_engine");
            g.g_imguiUpdate = (hotreloadable_imgui_draw_func)getfunction(g.engine_lib, "hotreloadable_imgui_draw");
            init_engine(&g);

            g.begin_frame = (begin_frame_func)getfunction(g.engine_lib, "begin_frame");
            g.begin_frame(&g);

            g.draw_opengl = (draw_opengl_func)getfunction(g.engine_lib, "draw_opengl");
            load_meshes_func load_meshes = (load_meshes_func)getfunction(g.engine_lib, "load_meshes");
            load_meshes(&g);
        }

        if (reloadExternalsFlag.load())
        {
            reloadExternalsFlag.store(false);
            printf("Reloading Externals...\n");
            unload_externals(g);
            reload_externals(g);
        }

        g.draw_opengl(&g);

        update_externals_ptr(&g);
    }
}

EXPORT void init()
{
    printf("Core initialized\n");

    char cwd[MAX_PATH];
    getCurrentWorkingDirectory(cwd,MAX_PATH);
    printf("Current working directory: %s\n", cwd);

    HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, HOTRELOAD_EVENT_NAME);
    if (hEvent == NULL)
    {
        printf("CreateEvent failed (%lu)\n", GetLastError());
    }
    std::thread signalThread(waitforreloadsignal, hEvent);
    signalThread.detach();

    game g;
    g.world = malloc(100 * 1024);
    if (g.world != NULL)
    {
        memset(g.world, 0, 100 * 1024);
    }
    
    copy_dll("engine");

    g.engine_lib = loadlibrary("engine_copy");

    init_engine_func init_engine = (init_engine_func)getfunction(g.engine_lib, "init_engine");
    init_engine(&g);
    load_level_func load_level = (load_level_func)getfunction(g.engine_lib, "load_level");
    load_level(&g, "scene.toml");

    g.begin_frame = (begin_frame_func)getfunction(g.engine_lib, "begin_frame");
    g.g_imguiUpdate = (hotreloadable_imgui_draw_func)getfunction(g.engine_lib, "hotreloadable_imgui_draw");

    copy_dll("externals");
    externals_lib = (HMODULE)loadlibrary("externals_copy");
    init_externals_func init_externals = (init_externals_func)getfunction(externals_lib, "init_externals");
    init_externals(&g);

    g.begin_frame(&g);
    load_meshes_func load_meshes = (load_meshes_func)getfunction(g.engine_lib, "load_meshes");
    load_meshes(&g);
    g.draw_opengl = (draw_opengl_func)getfunction(g.engine_lib, "draw_opengl");

    begin_watch_engine_directory(g);
    begin_watch_externals_directory(g);

    //g.draw_opengl = (draw_opengl_func)getfunction(externals_lib, "draw_opengl");
    update_externals_ptr = (update_externals_func)getfunction(externals_lib, "update_externals");
    end_externals_ptr  = (end_externals_func)getfunction(externals_lib, "end_externals");

    g.draw_opengl = (draw_opengl_func)getfunction(g.engine_lib, "draw_opengl");
    begin_game_loop(g);
}

void begin_watch_engine_directory(game &g)
{
    std::thread watchThread(watch_engine_directory);
    watchThread.detach();
}

void begin_watch_externals_directory(game &g)
{
    std::thread watchThread(watch_externals_directory);
    watchThread.detach();
}

void waitforreloadsignal(void* hEvent)
{
    HANDLE eventHandle = static_cast<HANDLE>(hEvent);
    while (true)
    {
        DWORD dwWaitResult = WaitForSingleObject(eventHandle, INFINITE);
        if (dwWaitResult == WAIT_OBJECT_0)
        {
            print_log("Hot reload signal received", COLOR_GREEN);
            reloadEngineFlag.store(true);
            break;
        }
    }
}
