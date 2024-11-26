#include "memory.h"
#include "core.h"
#include "ecs.h"

#include <cstddef>
#include <game.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const size_t initialEntityCount = 100;

#define ALIGN_OFFSET(type)                                                     \
  offset = (offset + alignof(type) - 1) & ~(alignof(type) - 1)

void reset_memory(Memory *h) {
  if (h == NULL)
    return;

    // #define DEFINE_ASSIGN_MEMORY(name) h->p##name##s->count = 0;

    // #define X(name) DEFINE_ASSIGN_MEMORY(name)
    //   SUBKEY_TYPES
    // #undef X

#undef DEFINE_ASSIGN_MEMORY

  if (h->shaders != NULL) {
    h->shaders->count = 0;
  }

  // for (size_t i = 0; i < initialEntityCount; ++i) {
  //   h->pModels->components[i].submesh_count = 0;
  // }

  h->query.count = 0;
  h->world.entity_count = 0;
  h->world.entity_count = 0;

  printf("reset_memory\n");
}

void init_engine_memory(game *g) {

  const size_t initialSubMeshCount = 10;

  Memory *header =
      (Memory *)((char *)g->buffer + g->buffer_size - sizeof(Memory));

  size_t down_offset = g->buffer_size - sizeof(Memory);

  size_t offset = 0;

  header->query.entities = (size_t *)((char *)g->buffer + offset);
  offset += sizeof(size_t) * initialEntityCount;
  header->query.count = 0;

  header->world.entity_ids = (size_t *)((char *)g->buffer + offset);
  offset += sizeof(size_t) * initialEntityCount;

  header->world.component_masks = (uint32_t *)((char *)g->buffer + offset);
  offset += sizeof(uint32_t) * initialEntityCount;

  header->world.entity_names =
      (char(*)[ENTITY_NAME_LENGTH])((char *)g->buffer + offset);
  offset += ENTITY_NAME_LENGTH * initialEntityCount;

  header->shaders = (Shaders *)((char *)g->buffer + offset);
  offset += sizeof(Shaders);

  header->shaders->program_ids = (unsigned int *)((char *)g->buffer + offset);
  offset += sizeof(unsigned int) * initialEntityCount;

  header->shaders->shader_names =
      (char(*)[ENTITY_NAME_LENGTH])((char *)g->buffer + offset);
  offset += ENTITY_NAME_LENGTH * initialEntityCount;

  // #define DEFINE_ASSIGN_MEMORY(name)                                             \
//   header->p##name##s = (name##s *)((char *)g->buffer + offset);                \
//   offset += sizeof(name##s);                                                   \
//   header->p##name##s->entity_ids = (size_t *)((char *)g->buffer + offset);     \
//   offset += sizeof(size_t) * initialEntityCount;                               \
//   header->p##name##s->components = (name *)((char *)g->buffer + offset);       \
//   offset += sizeof(name) * initialEntityCount;

  // #define X(name) DEFINE_ASSIGN_MEMORY(name)
  //   SUBKEY_TYPES
  // #undef X

  // #undef DEFINE_ASSIGN_MEMORY

  // for (size_t i = 0; i < initialEntityCount; ++i) {
  //   header->pModels->components[i].submeshes =
  //       (SubMesh *)((char *)g->buffer + offset);
  //   offset += sizeof(SubMesh) * initialSubMeshCount;
  // }

  down_offset = g->buffer_size - sizeof(Memory);
  header->total_size = down_offset - offset;
}

Memory *get_header(game *g) {
  return (Memory *)((char *)g->buffer + g->buffer_size - sizeof(Memory));
}

World *get_world(game *g) {
  // Memory *header = get_header(g);
  return &get_header(g)->world;
}
