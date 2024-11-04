#include "memory.h"

#include <game.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void init_engine_memory(game *g)
{
    const size_t initialEntityCount = 100;

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

    // Store total size for memory management or debugging
    header->total_size = offset;
}

MemoryHeader* get_header(game* g) {
    return (MemoryHeader *)g->world;
}

World* get_world(game* g) {
    MemoryHeader *header = get_header(g);
    return &header->world;
}

