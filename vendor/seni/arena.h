#ifndef ARENA_H
#define ARENA_H
#include <stddef.h>
typedef struct {
    void* data;
    size_t size;
    size_t offset;
} arena;

/* definitions live in arena.c: as header statics, every TU that included
   this without using all three tripped clang's -Wunused-function -Werror */
void create_arena(arena* a, void* buf, size_t size);

/* raw bump, no alignment: for char data. consecutive calls are contiguous,
   which the string builder in seni.c relies on. */
void* allocate_bytes(arena* a, size_t s);

/* 8-aligned: for structs and arrays of structs */
void* allocate(arena* a, size_t s);

char* arena_copy_string(arena* a, const char* src, size_t len);
char* arena_sprintf(arena* a, const char* fmt, ...);
#endif
