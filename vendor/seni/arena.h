#ifndef ARENA_H
#define ARENA_H
#include <stddef.h>
typedef struct {
    void* data;
    size_t size;
    size_t offset;
} arena;

static void create_arena(arena* a, void* buf, size_t size) {
    a->data = buf;
    a->size = size;
    a->offset = 0;
}

/* raw bump, no alignment: for char data. consecutive calls are contiguous,
   which the string builder in seni.c relies on. */
static void* allocate_bytes(arena* a, size_t s) {
    void* ptr;
    if (s > a->size - a->offset) return NULL; /* no offset+s, that can wrap */
    ptr = (char*)a->data + a->offset;
    a->offset += s;
    return ptr;
}

/* 8-aligned: for structs and arrays of structs */
static void* allocate(arena* a, size_t s) {
    size_t addr = (size_t)((char*)a->data + a->offset);
    size_t pad = (8 - (addr % 8)) % 8;
    if (pad > a->size - a->offset) return NULL;
    a->offset += pad;
    return allocate_bytes(a, s);
}

char* arena_copy_string(arena* a, const char* src, size_t len);
char* arena_sprintf(arena* a, const char* fmt, ...);
#endif
