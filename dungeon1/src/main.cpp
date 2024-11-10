#include <stdio.h>
#include <cstdlib> 
#include <printLog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef void (*init_func)(); 

bool isCompilerAccessible() {
    return system("where clang++ >nul 2>&1") == 0;
}

int main() {
    const char* libpath = "core.dll";  
    HMODULE hLib = LoadLibrary(libpath);
    
    if (hLib == NULL) {
        fprintf(stderr, "Failed to load library: %s, error code: %lu\n", libpath, GetLastError());
        return -1;
    } else {
        printf("loaded the core.dll\n");
    }

    init_func init = (init_func)GetProcAddress(hLib, "init");  

    if (!isCompilerAccessible()) {
        print_log(COLOR_RED, "Clang it's not accesible\n");
    } else {
        print_log(COLOR_GREEN, "Clang it's available\n");
    }

    if (init == NULL) {
        fprintf(stderr, "Failed to get function address: init, error code: %lu\n", GetLastError());
    
        FreeLibrary(hLib);  
        return -1;
    }

    init();  
    FreeLibrary(hLib);  
    return 0;
}