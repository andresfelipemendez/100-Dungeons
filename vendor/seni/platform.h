#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>

/* platform layer for the e2e test harness: directory creation, shared-library
   compilation and dynamic loading. implemented by platform_windows.c and
   platform_linux.c. */

typedef void* platform_lib;

/* create directory if missing; returns 0 on success or already-exists */
int platform_make_dir(const char* path);

/* resolve rel_path (which must exist) to an absolute path with forward
   slashes, suitable for pasting into generated source. needed because gcc's
   assembler resolves .incbin relative to the compiler's working directory,
   not the source file -- an absolute path makes the embed independent of
   where the compiler was invoked from. returns 0 on success. */
int platform_absolute_path(const char* rel_path, char* out, size_t cap);

/* "dll" on windows, "so" on linux */
const char* platform_lib_extension(void);

/* compile src_path into a shared library at lib_path, gcc stderr to err_path;
   returns 0 on success */
int platform_compile_shared(const char* src_path, const char* lib_path, const char* err_path);

platform_lib platform_load_lib(const char* path);   /* NULL on failure, prints reason */
void* platform_get_symbol(platform_lib lib, const char* name);
void platform_unload_lib(platform_lib lib);

#endif
