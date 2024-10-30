#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define ENTITY_NAME_LENGTH 16

#define SUBKEY_TYPES     \
    X(POSITION)          \
    X(COLOR)             \
    X(SCALE)             \
    X(ROTATION)          \
    X(FOV)               \
    X(INTENSITY)         \
    X(MODEL)             \
    X(MATERIAL)          \
    X(TEXTURE)           

enum SubkeyType {
    #define X(name) name##_TYPE,
    SUBKEY_TYPES
    #undef X
    UNKNOWN_TYPE
};

SubkeyType mapStringToSubkeyType(const char* type_key);

#define EXPAND_AS_ENUM(name, index) COMPONENT_##name = (1 << index),

extern const char* component_names[];
extern size_t component_count;

enum ComponentBitmask {
    #define X(name) EXPAND_AS_ENUM(name, __COUNTER__)
    SUBKEY_TYPES
    #undef X
};

typedef union Vec3 {
    struct { float x, y, z; };         
    struct { float r, g, b; };         
    struct { float pitch, yaw, roll; };
    float data[3];                     
} Vec3;

struct Transforms {
    size_t count;
    size_t* entity_ids;
    Vec3* positions;
};

struct Rotations {
    size_t count;
    size_t* entity_ids;
    Vec3* rotations;
};

struct Models {
    size_t count;
    size_t* entity_ids;
    Vec3* positions;
};

struct Shaders {
    size_t count;
    size_t* entity_ids;
    unsigned int *program_IDs;
};

typedef struct Texture {
	unsigned int textureID;
	int texWidth;
	int texHeight;
} Texture;

typedef struct Textures {
    size_t count;
    size_t* entity_ids;
    Texture *textures;
} Textures;

typedef struct World {
    size_t entity_count;
    size_t* entity_ids;
    uint32_t* component_masks;
    char (*entity_names)[ENTITY_NAME_LENGTH];
} World;

typedef struct ComponentTable {
    size_t count;         
    size_t offset;        
} ComponentTable;

struct MemoryHeader {
    World world;
    Transforms* transforms;
    size_t total_size;     
};

size_t create_entity(World* w);
void add_component(MemoryHeader* h, size_t entity_id, uint32_t component_mask);
bool get_component_value(MemoryHeader* h, size_t entity_id, uint32_t component_mask, Vec3* value);
bool set_component_value(MemoryHeader* h, size_t entity_id, uint32_t component_mask, Vec3 value);

void save_level(MemoryHeader* h, const char* saveFilePath);