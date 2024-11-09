#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef void (*init_func)(); 

int main() {
    const char* libpath = "core.dll";  
    HMODULE hLib = LoadLibrary(libpath);
    
    if (hLib == NULL) {
        fprintf(stderr, "Failed to load library: %s\n", libpath);
        return -1;  
    }

    init_func init = (init_func)GetProcAddress(hLib, "init");  
    if (init == NULL) {
        fprintf(stderr, "Failed to get function address: init\n");
        FreeLibrary(hLib);  
        return -1;
    }

    init();  
    FreeLibrary(hLib);  
    return 0;
}