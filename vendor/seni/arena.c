#include "arena.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

char* arena_copy_string(arena* a, const char* src, size_t len) {
    char* dst = allocate_bytes(a, len + 1);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/* c89: no vsnprintf, so format into a fixed temp then copy into the arena.
   callers only format short error messages; 1024 is plenty. */
char* arena_sprintf(arena* a, const char* fmt, ...) {
    char tmp[1024];
    va_list args;
    int n;
    va_start(args, fmt);
    n = vsprintf(tmp, fmt, args);
    va_end(args);
    if (n < 0) return NULL;
    return arena_copy_string(a, tmp, (size_t)n);
}
