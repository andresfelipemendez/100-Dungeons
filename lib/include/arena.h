#ifndef ARENA_ALLOCATOR
#define ARENA_ALLOCATOR

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>

typedef struct arena {
	struct arena_chunk *head;
	struct arena_chunk *current;
	size_t chunk_size;
	size_t total_allocated;
} arena;

typedef struct arena_chunk {
	struct arena_chunk *next;
	char *memory;
	size_t used;
	size_t capacity;
} arena_chunk;

arena *arena_create(size_t chunk_size) {
	arena *a = malloc(sizeof(arena));
	if(!a) return NULL;

	a->head = NULL;
	a->current = NULL;
	a->chunk_size = chunk_size;
	a->total_allocated = 0;

	return a;
}

static arena_chunk *arena_new_chunk(size_t size) {
	struct arena_chunk *chunk = malloc(sizeof(arena_chunk));
	if(!chunk) return NULL;

	chunk->memory = malloc(size);
	if(!chunk->memory) {
		free(chunk);
		return NULL;
	}

	chunk->next = NULL;
	chunk->used = 0;
	chunk->capacity = size;

	return chunk;
}

static size_t arena_align_to(size_t offset, size_t alignment) {
	const size_t mask = alignment - 1;
	return (offset + mask) & ~mask;
}

void *arena_alloc_aligned(arena *arena, size_t size, size_t alignment) {
	if(!arena || size == 0) return NULL;

	
	if(alignment == 0 || (alignment & (alignment - 1)) != 0) {
		alignment = alignof(max_align_t);
	}

	if(!arena->current) {
		size_t chunk_size = (size + alignment > arena->chunk_size) ? size + alignment : arena->chunk_size;
	}

	arena_chunk *chunk = arena->current;
	size_t aligned_offset = arena_align_to(chunk->used, alignment);

	if(aligned_offset + size > chunk->capacity) {
		size_t chunk_size = (size + alignment)
	}

}

#endif
