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
}

World* get_world(game* g) {
    MemoryHeader *header = (MemoryHeader *)g->world;
    return &header->world;
}