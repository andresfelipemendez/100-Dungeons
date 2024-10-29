#include <stdlib.h>
#include <stdint.h>

struct Transforms {
    int   id;
    float x;
    float y;
    float z;
};

struct Entity {
    uint32_t components;
};

struct world {
    int entitiesCount;
};

