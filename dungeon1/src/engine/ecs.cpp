#include "ecs.h"
#include <game.h>
#include "memory.h"
#include <stdio.h>

#include <string.h>
#ifdef _WIN32
    #define strcasecmp _stricmp
#endif

const char* component_names[] = {
    #define X(name) #name,
    SUBKEY_TYPES
    #undef X
};

extern size_t component_count =  sizeof(component_names) / sizeof(component_names[0]);

SubkeyType mapStringToSubkeyType(const char* type_key) {
    #define X(name) if (strcasecmp(type_key, #name) == 0) return name##_TYPE;
    SUBKEY_TYPES
    #undef X
    return UNKNOWN_TYPE;
}

size_t create_entity(World* w) {
    size_t entity_id = w->entity_count;
    w->entity_ids[entity_id] = entity_id;
    snprintf(w->entity_names[entity_id], ENTITY_NAME_LENGTH, "Entity %zu", entity_id);
    w->component_masks[entity_id] = 0;
    w->entity_count++;
    return entity_id;
}

void add_component(MemoryHeader* h, size_t entity_id, uint32_t component_mask) {
    switch (component_mask)
    {
    case COMPONENT_POSITION: {
        size_t i = h->transforms->count;
        h->transforms->entity_ids[i] = entity_id;
        h->transforms->positions[i] = {0,0,0};
        h->transforms->count++;
        break;
    }
    }
    h->world.component_masks[entity_id] |= component_mask;
}

bool get_component_value(MemoryHeader* h, size_t entity_id, uint32_t component_mask, Vec3* value) {
    if(!(h->world.component_masks[entity_id] & component_mask)) {
        return false;
    }

    switch (component_mask)
    {
    case COMPONENT_POSITION: {
        for (size_t i = 0; i < h->transforms->count; i++) {
            if (h->transforms->entity_ids[i] == entity_id) {
                *value = h->transforms->positions[i];
                return true;
            }
        }
        break;
    }
    }

    
}

bool set_component_value(MemoryHeader* h, size_t entity_id, uint32_t component_mask, Vec3 value) {
    if (!(h->world.component_masks[entity_id] & component_mask)) {
        return false;
    }

    switch (component_mask) {
        case COMPONENT_POSITION: {
            for (size_t i = 0; i < h->transforms->count; i++) {
                if (h->transforms->entity_ids[i] == entity_id) {
                    h->transforms->positions[i] = value;
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

void ecs_load_level(game* g, const char* saveFilePath) {
    
}

void save_level(MemoryHeader* h, const char* saveFilePath) {

}