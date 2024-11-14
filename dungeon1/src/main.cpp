#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <windows.h>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#define COLOR_GREEN 2
#define COLOR_RED 4

typedef void (*func_type)();

std::atomic<bool> reloadEngineFlag(false);
std::atomic<bool> shutdownFlag(false);

constexpr int DEBOUNCE_INTERVAL_MS = 5000;

func_type init = nullptr;
func_type stop = nullptr;
std::thread init_thread;

bool copy_dll(const char* dll_name, char* dest, size_t dest_size) {
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

void watch_core() {
    HANDLE hDir = CreateFileA("../../src/core",
                              FILE_LIST_DIRECTORY,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                              NULL);

    if (hDir == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFile failed for directory: src/core (" << GetLastError() << ")" << std::endl;
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

    while (!shutdownFlag.load()) {
        if (ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), TRUE,
                                  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                  &bytesReturned, &overlapped, NULL)) {
            WaitForSingleObject(overlapped.hEvent, INFINITE);

            auto currentTime = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - lastCompilationTime)
                    .count() >= DEBOUNCE_INTERVAL_MS) {
                lastCompilationTime = currentTime;

                system("build_core.bat");
                reloadEngineFlag.store(true);
            }

            ResetEvent(overlapped.hEvent);
        }
    }

    CloseHandle(hDir);
}

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    shutdownFlag.store(true);
}

void start_init_thread() {
    if (init == nullptr) {
        std::cerr << "init function pointer is null, cannot start thread.\n";
        return;
    }

    init_thread = std::thread([=]() {
        init();
    });
}

void stop_and_cleanup() {
    // Stop the current DLL operation
    if (stop != nullptr) {
        stop();
    }

    // Join the init thread to ensure it has exited
    if (init_thread.joinable()) {
        std::cout << "Waiting for init thread to join..." << std::endl;
        init_thread.join();
        std::cout << "Init thread joined successfully." << std::endl;
    }
}

void reload_loop() {
    char copiedCoreDllPath[MAX_PATH];
    if (!copy_dll("core", copiedCoreDllPath, sizeof(copiedCoreDllPath))) {
        return;
    }

    HMODULE hLib = LoadLibrary(copiedCoreDllPath);
    if (hLib == NULL) {
        fprintf(stderr, "Failed to load library: %s, error code: %lu\n",
                copiedCoreDllPath, GetLastError());
        return;
    } else {
        printf("Loaded core_copy.dll\n");
    }

    init = (func_type)GetProcAddress(hLib, "init");
    stop = (func_type)GetProcAddress(hLib, "stop");

    if (init == NULL || stop == NULL) {
        fprintf(stderr, "Failed to get function address: init or stop, error code: %lu\n",
                GetLastError());
        FreeLibrary(hLib);
        return;
    }

    // Start the init thread
    start_init_thread();

    while (!shutdownFlag.load()) {
        if (reloadEngineFlag.load()) {
            reloadEngineFlag.store(false);

            // Stop and cleanup the current DLL
            stop_and_cleanup();

            // Free the old library and retry with delays
            if (hLib) {
                std::cout << "Freeing old DLL library." << std::endl;
                 BOOL result = FreeLibrary(hLib);
			    if (result == 0) {
			        // FreeLibrary failed, print the error code
			        DWORD error = GetLastError();
			        std::cerr << "FreeLibrary failed: Error code " << error << std::endl;
			    } else {
			        // FreeLibrary succeeded
			        std::cout << "FreeLibrary called successfully." << std::endl;
			        hLib = nullptr;
			    }

                // Sleep to ensure the DLL is released
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            HMODULE checkModule = GetModuleHandle(copiedCoreDllPath);
			if (checkModule == NULL && GetLastError() == ERROR_MOD_NOT_FOUND) {
			    std::cout << "DLL has been fully unloaded." << std::endl;
			} else {
			    std::cerr << "DLL is still loaded, trying to unload again..." << std::endl;

	            // Attempt to free again if still loaded
	            while (checkModule != NULL) {
	                BOOL retryResult = FreeLibrary(checkModule);
	                if (retryResult == 0) {
	                    DWORD error = GetLastError();
	                    std::cerr << "FreeLibrary retry failed: Error code " << error << std::endl;
	                    break;
	                } else {
	                    std::cout << "Retrying FreeLibrary successful." << std::endl;
	                }

	                std::this_thread::sleep_for(std::chrono::milliseconds(500));
	                checkModule = GetModuleHandle(copiedCoreDllPath);
	            }

	            if (checkModule == NULL && GetLastError() == ERROR_MOD_NOT_FOUND) {
	                std::cout << "DLL has been finally unloaded." << std::endl;
	            } else {
	                std::cerr << "After multiple attempts, DLL is still loaded. Giving up." << std::endl;
	            }
			}

            // Retry mechanism to copy DLL
            bool copied = false;
            int retries = 3;
            for (int attempt = 0; attempt < retries; ++attempt) {
                if (copy_dll("core", copiedCoreDllPath, sizeof(copiedCoreDllPath))) {
                    copied = true;
                    break;
                } else {
                    std::cerr << "Could not copy DLL, retrying... Attempt " << (attempt + 1) << " of " << retries << "\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }


            if (!copied) {
                std::cerr << "Failed to copy DLL after several attempts. Aborting reload.\n";
                continue;
            }

            // Load the updated core DLL
            hLib = LoadLibrary(copiedCoreDllPath);
            if (hLib == NULL) {
                std::cerr << "Failed to load library: " << copiedCoreDllPath << ", error code: " << GetLastError() << "\n";
                continue;
            } else {
                std::cout << "Reloaded core_copy.dll\n";
            }

            // Get the new init and stop function pointers
            init = (func_type)GetProcAddress(hLib, "init");
            stop = (func_type)GetProcAddress(hLib, "stop");

            if (init == NULL || stop == NULL) {
                std::cerr << "Failed to get function address: init or stop, error code: " << GetLastError() << "\n";
                FreeLibrary(hLib);
                hLib = nullptr;
                continue;
            }

            // Start a new init thread with the updated DLL
            start_init_thread();
        }

        // Sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    stop_and_cleanup();

    if (hLib) {
        FreeLibrary(hLib);
        hLib = nullptr;
    }
}

int main() {
    signal(SIGINT, signalHandler);

    // system("build_core.bat");
    // system("build_externals.bat");
    // system("build_engine.bat");

    std::thread watch_thread(watch_core);
    std::thread reload_thread(reload_loop);

    if (watch_thread.joinable()) {
        watch_thread.join();
    }
    if (reload_thread.joinable()) {
        reload_thread.join();
    }

    return 0;
}
