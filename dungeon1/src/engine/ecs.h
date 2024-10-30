#pragma once
#include <stdlib.h>
#include <stdint.h>

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
    ComponentTable transforms;
    ComponentTable rotations;
    ComponentTable models;
    ComponentTable shaders;
    ComponentTable textures;
    size_t total_size;     
};

size_t create_entity(struct game* g);