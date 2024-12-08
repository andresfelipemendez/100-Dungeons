#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H


typedef struct {
  char *buffer;
  size_t size;
  size_t offset;
} Arena;

Arena *arena_create(size_t size);

void* arena_alloc(Arena* arena, size_t size);

void arena_destroy(Arena* arena);
#endif // ARENA_ALLOCATOR_H
