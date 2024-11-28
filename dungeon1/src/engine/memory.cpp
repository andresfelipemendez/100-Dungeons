#include "memory.h"
#include "core.h"
#include "ecs.h"

#include "components.h"
#include <cstddef>
#include <game.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALIGN_OFFSET(type)                                                     \
  offset = (offset + alignof(type) - 1) & ~(alignof(type) - 1)

void reset_memory(Memory *m) {
  if (m == NULL)
    return;

    // #define DEFINE_ASSIGN_MEMORY(name) m->p##name##s->count = 0;

    // #define X(name) DEFINE_ASSIGN_MEMORY(name)
    //   SUBKEY_TYPES
    // #undef X

#undef DEFINE_ASSIGN_MEMORY

  if (m->shaders != NULL) {
    m->shaders->count = 0;
  }

  // for (size_t i = 0; i < initialEntityCount; ++i) {
  //   m->pModels->components[i].submesh_count = 0;
  // }

  m->query.count = 0;
  m->world.entity_count = 0;
  m->world.entity_count = 0;

  printf("reset_memory\n");
}

char *arena_strdup(StringsArena *strings, char *str) {
  size_t len = strlen(str) + 1;
  if (strings->used + len > strings->size) {
    return NULL; // Arena full
  }
  char *dest = strings->buffer + strings->used;
  strcpy_s(dest, strings->size - strings->used, str);
  strings->used += len;
  return dest;
}

void init_engine_memory(game *g) {

  Memory *m = (Memory *)((char *)g->buffer + g->buffer_size - sizeof(Memory));

  size_t down_offset = g->buffer_size - sizeof(Memory);

  size_t offset = 0;

  m->query.entities = (size_t *)((char *)g->buffer + offset);
  offset += sizeof(size_t) * initialEntityCount;
  m->query.count = 0;

  m->world.entity_ids = (size_t *)((char *)g->buffer + offset);
  offset += sizeof(size_t) * initialEntityCount;

  m->world.component_masks = (uint32_t *)((char *)g->buffer + offset);
  offset += sizeof(uint32_t) * initialEntityCount;

  m->world.entity_names =
      (char(*)[ENTITY_NAME_LENGTH])((char *)g->buffer + offset);
  offset += ENTITY_NAME_LENGTH * initialEntityCount;

  m->shaders = (Shaders *)((char *)g->buffer + offset);
  offset += sizeof(Shaders);

  m->shaders->program_ids = (unsigned int *)((char *)g->buffer + offset);
  offset += sizeof(unsigned int) * initialEntityCount;

  m->shaders->shader_names =
      (char(*)[ENTITY_NAME_LENGTH])((char *)g->buffer + offset);
  offset += ENTITY_NAME_LENGTH * initialEntityCount;

  assign_components_memory(m, g, &offset);

  down_offset = g->buffer_size - sizeof(Memory);
  m->total_size = down_offset - offset;
}

Memory *get_header(game *g) {
  return (Memory *)((char *)g->buffer + g->buffer_size - sizeof(Memory));
}

World *get_world(game *g) { return &get_header(g)->world; }
