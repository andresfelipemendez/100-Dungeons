#pragma once

void init_engine_memory(struct game *g);
struct MemoryHeader *get_header(struct game *g);
struct World *get_world(struct game *g);
void reset_memory(struct MemoryHeader *h);