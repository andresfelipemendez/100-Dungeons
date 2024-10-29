#include "ecs.h"
#include <game.h>
#include "memory.h"
#include <stdio.h>

size_t create_entity(struct game* g) {
    World* w = get_world(g);
    size_t entity_id = w->entity_count;
    w->entity_ids[entity_id] = entity_id;
    snprintf(w->entity_names[entity_id], ENTITY_NAME_LENGTH, "Entity %zu", entity_id);
    w->component_masks[entity_id] = 0;
    w->entity_count++;
    return entity_id;
}