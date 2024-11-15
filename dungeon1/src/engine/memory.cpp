#include "memory.h"
#include "core.h"
#include "ecs.h"

#include <cstddef>
#include <game.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALIGN_OFFSET(type)                                                     \
	offset = (offset + alignof(type) - 1) & ~(alignof(type) - 1)

void reset_memory(MemoryHeader *h) {
	if (h == NULL)
		return;

	// if (h->cameras != NULL) {
	// 	h->cameras->count = 0;
	// }

	// if (h->transforms != NULL) {
	// 	h->transforms->count = 0;
	// }

	// if (h->meshes != NULL) {
	// 	h->meshes->count = 0;
	// 	for (size_t i = 0; i < h->meshes->count; ++i) {
	// 		h->meshes->mesh_data[i].submesh_count = 0;
	// 	}
	// }

	// if (h->materials != NULL) {
	// 	h->materials->count = 0;
	// }

	// if (h->shaders != NULL) {
	// 	h->shaders->count = 0;
	// }

	h->query.count = 0;
	h->world.entity_count = 0;

	printf("reset_memory\n");
}

void init_engine_memory(game *g) {
	const size_t initialEntityCount = 100;
	const size_t initialSubMeshCount = 10;

	MemoryHeader *header = (MemoryHeader *)((char *)g->buffer + g->buffer_size -
											sizeof(MemoryHeader));

	size_t down_offset = g->buffer_size - sizeof(MemoryHeader);

	size_t offset = 0;

	header->query.entities = (size_t *)((char *)g->buffer + offset);
	offset += sizeof(size_t) * initialEntityCount;
	header->query.count = 0;

	header->world.entity_ids = (size_t *)((char *)g->buffer + offset);
	offset += sizeof(size_t) * initialEntityCount;

	header->world.component_masks = (uint32_t *)((char *)g->buffer + offset);
	offset += sizeof(uint32_t) * initialEntityCount;

	header->world.entity_names =
		(char(*)[ENTITY_NAME_LENGTH])((char *)g->buffer + offset);
	offset += ENTITY_NAME_LENGTH * initialEntityCount;

	header->shaders = (Shaders *)((char *)g->buffer + offset);
	offset += sizeof(Shaders);

	header->shaders->program_ids = (unsigned int *)((char *)g->buffer + offset);
	offset += sizeof(unsigned int) * initialEntityCount;

	header->shaders->shader_names =
		(char(*)[ENTITY_NAME_LENGTH])((char *)g->buffer + offset);
	offset += ENTITY_NAME_LENGTH * initialEntityCount;

#define DEFINE_ASSIGN_MEMORY(name)                                             \
	header->p##name##s = (name##s *)((char *)g->buffer + offset);              \
	offset += sizeof(name##s);                                                 \
	header->p##name##s->entity_ids = (size_t *)((char *)g->buffer + offset);   \
	offset += sizeof(size_t) * initialEntityCount;                             \
	header->p##name##s->components = (name *)((char *)g->buffer + offset);     \
	offset += sizeof(name) * initialEntityCount;

#define X(name) DEFINE_ASSIGN_MEMORY(name)
	SUBKEY_TYPES
#undef X

#undef DEFINE_ASSIGN_MEMORY

	// Allocate submeshes for each mesh
	for (size_t i = 0; i < initialEntityCount; ++i) {
		header->pModels->components[i].submeshes =
			(SubMesh *)((char *)g->buffer + offset);
		offset += sizeof(SubMesh) * initialSubMeshCount;
		header->pModels->components[i].submesh_count = 0;
	}

	// Calculate remaining buffer size
	down_offset = g->buffer_size - sizeof(MemoryHeader);
	header->total_size = down_offset - offset;
}

MemoryHeader *get_header(game *g) {
	MemoryHeader *header = (MemoryHeader *)((char *)g->buffer + g->buffer_size -
											sizeof(MemoryHeader));
	return header;
}

World *get_world(game *g) {
	MemoryHeader *header = get_header(g);
	return &header->world;
}
