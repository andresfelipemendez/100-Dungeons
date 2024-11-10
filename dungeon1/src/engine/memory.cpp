#include "memory.h"

#include <game.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void init_engine_memory(game *g)
{
    const size_t initialEntityCount = 100;
    const size_t initialSubMeshCount = 10;

    MemoryHeader *header = (MemoryHeader *)g->world;
    size_t offset = sizeof(MemoryHeader);

    header->world.entity_ids = (size_t *)((char *)header + offset);
    offset += sizeof(size_t) * initialEntityCount;
    header->world.component_masks = (uint32_t *)((char *)header + offset);
    offset += sizeof(uint32_t) * initialEntityCount;
    header->world.entity_names = (char (*)[ENTITY_NAME_LENGTH])((char *)header + offset);
    offset += ENTITY_NAME_LENGTH * initialEntityCount;


    header->transforms = (Transforms *)((char *)header + offset);
    offset += sizeof(Transforms); 
    header->transforms->entity_ids = (size_t *)((char *)header + offset);
    offset += sizeof(size_t) * initialEntityCount;
    header->transforms->positions = (Vec3 *)((char *)header + offset);
    offset += sizeof(Vec3) * initialEntityCount;

    header->meshes = (Meshes *)((char *)header + offset);
    offset += sizeof(Meshes);
    header->meshes->entity_ids = (size_t*)((char *)header + offset);
    offset += sizeof(size_t) * initialEntityCount;

    header->meshes->mesh_data = (StaticMesh*)((char *)header + offset);
    offset += sizeof(StaticMesh) * initialEntityCount;

    for(size_t i = 0; i < initialEntityCount; ++i) {
        header->meshes->mesh_data[i].submeshes = (SubMesh *)((char *)header + offset);
        offset += sizeof(SubMesh) * initialSubMeshCount;
        header->meshes->mesh_data[i].submesh_count = 0;
    }

    header->total_size = offset;
}

MemoryHeader* get_header(game* g) {
    return (MemoryHeader *)g->world;
}

World* get_world(game* g) {
    MemoryHeader *header = get_header(g);
    return &header->world;
}

