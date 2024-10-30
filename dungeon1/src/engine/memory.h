#pragma once

#include "ecs.h"

void init_engine_memory(struct game* g);
MemoryHeader* get_header(struct game* g);
World* get_world(struct game* g);