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

std::string getCurrentWorkingDirectory()
{
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    return std::string(buffer);
}

void copy_dll(const std::string &src, const std::string &dest)
{
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
}

void compile_engine_dll()
{
    std::string cwd = getCurrentWorkingDirectory();
    std::string command = "cd /d " + cwd + " && build_engine.bat";
    std::cout << "Compiling Engine DLL with command: " << command << std::endl;
    system(command.c_str());
}

void compile_externals_dll()
{
    std::string cwd = getCurrentWorkingDirectory();
    std::string command = "cd /d " + cwd + " && build_externals.bat";
    std::cout << "Compiling Externals DLL with command: " << command << std::endl;
    system(command.c_str());
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
    
    // Copy the newly compiled externals.dll to externals_copy.dll
    std::string cwd = getCurrentWorkingDirectory();
    std::string src = cwd + "\\build\\Debug\\externals.dll";
    std::string dest = cwd + "\\build\\Debug\\externals_copy.dll";
    copy_dll(src, dest);
    
    // Load externals_copy.dll
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

            std::string cwd = getCurrentWorkingDirectory();
            std::string src = cwd + "\\build\\Debug\\engine.dll";
            std::string dest = cwd + "\\build\\Debug\\engine_copy.dll";
            copy_dll(src, dest);

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

    std::string cwd = getCurrentWorkingDirectory();
    std::cout << "Current working directory: " << cwd << std::endl;

    HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, HOTRELOAD_EVENT_NAME);
    if (hEvent == NULL)
    {
        std::cerr << "CreateEvent failed (" << GetLastError() << ")" << std::endl;
    }
    std::thread signalThread(waitforreloadsignal, hEvent);
    signalThread.detach();

    game g;
    g.world = malloc(100 * 1024);
    if (g.world != NULL)
    {
        memset(g.world, 0, 100 * 1024);
    }

    std::string src = cwd + "\\build\\debug\\engine.dll";
    std::string dest = cwd + "\\build\\debug\\engine_copy.dll";
    copy_dll(src, dest);

    g.engine_lib = loadlibrary("engine_copy");

    init_engine_func init_engine = (init_engine_func)getfunction(g.engine_lib, "init_engine");
    init_engine(&g);
    load_level_func load_level = (load_level_func)getfunction(g.engine_lib, "load_level");
    load_level(&g, "scene.toml");

    g.begin_frame = (begin_frame_func)getfunction(g.engine_lib, "begin_frame");
    g.g_imguiUpdate = (hotreloadable_imgui_draw_func)getfunction(g.engine_lib, "hotreloadable_imgui_draw");

    // Load externals similarly to engine
    std::string externals_src = cwd + "\\build\\debug\\externals.dll";
    std::string externals_dest = cwd + "\\build\\debug\\externals_copy.dll";
    copy_dll(externals_src, externals_dest);
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
