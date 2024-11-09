#ifndef LOADLIBRARY_H
#define LOADLIBRARY_H

#include "export.h"

EXPORT void *loadlibrary(const char *libname);
void unloadlibrary(void *lib);
EXPORT void *getfunction(void *lib, const char *funcname);

#endif // LOADLIBRARY_H
