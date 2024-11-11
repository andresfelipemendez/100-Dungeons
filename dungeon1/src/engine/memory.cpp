#include "memory.h"
#include "core.h"
#include "ecs.h"

#include <cstddef>
#include <game.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void init_engine_memory(game *g) {
	const size_t initialEntityCount = 100;
	const size_t initialSubMeshCount = 10;

	MemoryHeader *header = (MemoryHeader *)((char *)g->world + g->buffer_size -
											sizeof(MemoryHeader));

	size_t down_offset = g->buffer_size - sizeof(MemoryHeader);

	size_t offset = 0;
	header->world.entity_ids = (size_t *)((char *)g->world + offset);
	offset += sizeof(size_t) * initialEntityCount;

	header->world.component_masks = (uint32_t *)((char *)g->world + offset);
	offset += sizeof(uint32_t) * initialEntityCount;

	header->world.entity_names =
		(char(*)[ENTITY_NAME_LENGTH])((char *)g->world + offset);
	offset += ENTITY_NAME_LENGTH * initialEntityCount;

	// Set up transforms
	header->transforms = (Transforms *)((char *)g->world + offset);
	offset += sizeof(Transforms);

	header->transforms->entity_ids = (size_t *)((char *)g->world + offset);
	offset += sizeof(size_t) * initialEntityCount;

	header->transforms->positions = (Vec3 *)((char *)g->world + offset);
	offset += sizeof(Vec3) * initialEntityCount;

	// Set up cameras
	header->cameras = (Cameras *)((char *)g->world + offset);
	offset += sizeof(Cameras);

	header->cameras->entity_ids = (size_t *)((char *)g->world + offset);
	offset += sizeof(size_t) * initialEntityCount;

	header->cameras->cameras = (Camera *)((char *)g->world + offset);
	offset += sizeof(Camera) * initialEntityCount;

	// Set up shaders
	header->shaders = (Shaders *)((char *)g->world + offset);
	offset += sizeof(Shaders);

	header->shaders->entity_ids = (size_t *)((char *)g->world + offset);
	offset += sizeof(size_t) * initialEntityCount;

	header->shaders->program_ids = (unsigned int *)((char *)g->world + offset);
	offset += sizeof(unsigned int) * initialEntityCount;

	// Set up meshes
	header->meshes = (Meshes *)((char *)g->world + offset);
	offset += sizeof(Meshes);

	header->meshes->entity_ids = (size_t *)((char *)g->world + offset);
	offset += sizeof(size_t) * initialEntityCount;

	header->meshes->mesh_data = (StaticMesh *)((char *)g->world + offset);
	offset += sizeof(StaticMesh) * initialEntityCount;

	// Allocate submeshes for each mesh
	for (size_t i = 0; i < initialEntityCount; ++i) {
		header->meshes->mesh_data[i].submeshes =
			(SubMesh *)((char *)g->world + offset);
		offset += sizeof(SubMesh) * initialSubMeshCount;
		header->meshes->mesh_data[i].submesh_count = 0;
	}

	header->shaders = (Shaders *)((char *)g->world + offset);
	offset += sizeof(Shaders);

	// Calculate remaining buffer size
	down_offset = g->buffer_size - sizeof(MemoryHeader);
	header->total_size = down_offset - offset;
}

MemoryHeader *get_header(game *g) {
	MemoryHeader *header = (MemoryHeader *)((char *)g->world + g->buffer_size -
											sizeof(MemoryHeader));
	return header;
}

World *get_world(game *g) {
	MemoryHeader *header = get_header(g);
	return &header->world;
}
