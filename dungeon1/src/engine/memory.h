#pragma once

const size_t initialSubMeshCount = 10;
const size_t initialEntityCount = 100;

void init_engine_memory(struct game *g);
// struct MemoryHeader *get_header(struct game *g);
struct Memory *get_header(struct game *g);
struct World *get_world(struct game *g);
void reset_memory(struct Memory *m);
