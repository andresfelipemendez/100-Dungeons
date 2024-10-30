#include "ecs.h"
#include <game.h>
#include "memory.h"
#include <stdio.h>

const char* component_names[] = {
    #define X(name) #name,
    SUBKEY_TYPES
    #undef X
};

extern size_t component_count =  sizeof(component_names) / sizeof(component_names[0]);

size_t create_entity(struct game* g) {
    World* w = get_world(g);
    size_t entity_id = w->entity_count;
    w->entity_ids[entity_id] = entity_id;
    snprintf(w->entity_names[entity_id], ENTITY_NAME_LENGTH, "Entity %zu", entity_id);
    w->component_masks[entity_id] = 0;
    w->entity_count++;
    return entity_id;
}

void add_component(World* w, size_t entity_id, uint32_t component_mask) {
    switch (component_mask)
    {
    case COMPONENT_POSITION:
        
        break;
    
    default:
        break;
    }
    w->component_masks[entity_id] |= component_mask;
}