#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define ARENA_SIZE 1024  

struct Arena {
    uint8_t *memory;
    size_t size;
    size_t offset;
};

