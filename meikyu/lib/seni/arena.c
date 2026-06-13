#include "arena.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void create_arena(arena* a, void* buf, size_t size) {
    a->data = buf;
    a->size = size;
    a->offset = 0;
}

void* allocate_bytes(arena* a, size_t s) {
    void* ptr;
    if (s > a->size - a->offset) return NULL; /* no offset+s, that can wrap */
    ptr = (char*)a->data + a->offset;
    a->offset += s;
    return ptr;
}

void* allocate(arena* a, size_t s) {
    size_t addr = (size_t)((char*)a->data + a->offset);
    size_t pad = (8 - (addr % 8)) % 8;
    if (pad > a->size - a->offset) return NULL;
    a->offset += pad;
    return allocate_bytes(a, s);
}

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
