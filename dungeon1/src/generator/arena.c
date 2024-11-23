#include "arena.h"

#include <stdlib.h>
Arena *arena_create(size_t size) {
  Arena *arena = (Arena *)malloc(sizeof(Arena));
  arena->buffer = (char *)malloc(size);
  arena->size = size;
  arena->offset = 0;
  return arena;
}
