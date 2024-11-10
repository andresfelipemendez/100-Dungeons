#include "loadlibrary.h"
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void *loadlibrary(const char *libname) {
  HMODULE hLib = LoadLibrary(libname);
  
  if (hLib == NULL) {
    // Get the last error code
    DWORD errorCode = GetLastError();
    
    // Buffer to hold the error message
    char errorMsg[256];
    
    // Format the error message
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorMsg,
        sizeof(errorMsg),
        NULL
    );
    
    // Print the error message
    fprintf(stderr, "Failed to load library: %s\nError: %s\n", libname, errorMsg);
    return NULL;
  }
  
  return hLib;
}

void unloadlibrary(void *hLib) {
  if (hLib) {
    FreeLibrary((HMODULE)hLib);
  }
}

void *getfunction(void *lib, const char *funcname) {
  void *func = (void*)(uintptr_t) GetProcAddress((HMODULE)lib, funcname);
  if (func == NULL) {
      fprintf(stderr, "Failed to get function address: %s, error code: %lu\n", funcname, GetLastError());
      return nullptr;
  }
  return func;
}
