#ifndef SYSTEMS_H
#define SYSTEMS_H

void systems(struct game *g, struct MemoryHeader *h);
void input_system(struct game *g, struct MemoryHeader *h);
void rendering_system(struct MemoryHeader *h);
#endif // SYSTEMS_H