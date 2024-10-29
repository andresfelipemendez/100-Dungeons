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

struct Models {
    size_t count;
    size_t* entity_ids;
    Vec3* positions;
};

struct World {
    size_t entity_count;
    size_t* entity_ids;
    uint32_t* component_masks;
};

