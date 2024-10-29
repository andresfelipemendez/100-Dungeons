#include <stdlib.h>
#include <stdint.h>

union Vec3 {
    struct { float x, y, z; };         
    struct { float r, g, b; };         
    struct { float pitch, yaw, roll; };
    float data[3];                     
};

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

struct Texture {
	unsigned int textureID;
	int texWidth;
	int texHeight;
};

struct Textures {
    size_t count;
    size_t* entity_ids;
    Texture *textures;
};

struct World {
    size_t entity_count;
    size_t* entity_ids;
    uint32_t* component_masks;
};

