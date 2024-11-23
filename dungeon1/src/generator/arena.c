#include "arena.h"
#include <stdlib.h>

Arena *arena_create(size_t size) {
  Arena *arena = (Arena *)malloc(sizeof(Arena));
  arena->buffer = (char *)malloc(size);
  arena->size = size;
  arena->offset = 0;
  return arena;
}

void *arena_alloc(Arena *arena, size_t size) {
  if (arena->offset + size > arena->size) {
    perror("Arena out of memory!");
    return NULL;
  }

  void *ptr = arena->buffer + arena->offset;
  arena->offset += size;
  return ptr;
}

void arena_destroy(Arena *arena) {
  free(arena->buffer);
  free(arena);
}
