#ifndef SENI_REGISTRY_H
#define SENI_REGISTRY_H

/* Array registry: makes persistent memory self-describing at the array
   level, the way seni_embed.h makes the dll self-describing at the layout
   level. The schema text travels with the code artifact (embedded header);
   the schema instantiation -- where the arrays actually sit -- travels with
   the data artifact, as a table at the base of the block, like the catalog
   tables a database keeps inside its own file.

   Protocol:
   - host owns the block, calls seni_registry_init once on fresh memory
   - reloadable code carves every persistent array with seni_carve(name,...)
     and finds them again after reload with seni_registry_find
   - on reload, a driver (seni_reload.h) walks the OLD block's registry and
     migrates entry by entry into a fresh block -- neither host nor driver
     compiles in any struct layout

   The registry survives reload because it is data in the block, same as
   everything else. `name` keys the migrate_<name> symbol in the migration
   dll; `stride` is the element size under the layout that carved the array,
   which the reload driver cross-checks against the migration dll's
   migrate_<name>_old_size export.

   Strict c89: this header is compiled into game dlls built with
   -std=c89 -pedantic. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SENI_REGISTRY_MAGIC 0x53454E49UL /* "SENI" */
#define SENI_REGISTRY_MAX_ARRAYS 32
#define SENI_REGISTRY_NAME_MAX 64
#define SENI_REGISTRY_ALIGN 16

typedef struct {
    char name[SENI_REGISTRY_NAME_MAX]; /* struct name; keys migrate_<name> */
    size_t offset;                     /* array start, bytes from block base */
    size_t count;                      /* elements */
    size_t stride;                     /* element size in the carving layout */
} seni_array_desc;

typedef struct {
    unsigned long magic;
    size_t cap;        /* total block size, bytes */
    size_t used;       /* bytes consumed, registry included */
    size_t entry_count;
    seni_array_desc entries[SENI_REGISTRY_MAX_ARRAYS];
} seni_registry;

/* host calls once on a fresh block; cap is the full block size */
static void seni_registry_init(void* base, size_t cap) {
    seni_registry* r = (seni_registry*)base;
    r->magic = SENI_REGISTRY_MAGIC;
    r->cap = cap;
    r->used = (sizeof(seni_registry) + (SENI_REGISTRY_ALIGN - 1))
              & ~(size_t)(SENI_REGISTRY_ALIGN - 1);
    r->entry_count = 0;
}

/* NULL if base does not start with a registry (wrong magic) */
static seni_registry* seni_registry_get(void* base) {
    seni_registry* r = (seni_registry*)base;
    if (r->magic != SENI_REGISTRY_MAGIC) {
        fprintf(stderr, "seni_registry: bad magic 0x%lx at block base (expected 0x%lx)\n",
                r->magic, SENI_REGISTRY_MAGIC);
        return NULL;
    }
    return r;
}

/* NULL if no array named name has been carved in this block */
static seni_array_desc* seni_registry_find(void* base, const char* name) {
    seni_registry* r = seni_registry_get(base);
    size_t i;
    if (!r) return NULL;
    for (i = 0; i < r->entry_count; i++) {
        if (strcmp(r->entries[i].name, name) == 0) return &r->entries[i];
    }
    return NULL;
}

/* carve count*stride bytes for a persistent array and record it in the
   registry. returns the array pointer, NULL on exhaustion / full registry /
   duplicate or oversized name. */
static void* seni_carve(void* base, const char* name, size_t count, size_t stride) {
    seni_registry* r = seni_registry_get(base);
    seni_array_desc* e;
    size_t bytes;
    if (!r) return NULL;
    if (strlen(name) >= SENI_REGISTRY_NAME_MAX) {
        fprintf(stderr, "seni_carve: name '%s' is %lu chars, max %d\n",
                name, (unsigned long)strlen(name), SENI_REGISTRY_NAME_MAX - 1);
        return NULL;
    }
    if (seni_registry_find(base, name)) {
        fprintf(stderr, "seni_carve: array '%s' already carved in this block\n", name);
        return NULL;
    }
    if (r->entry_count >= SENI_REGISTRY_MAX_ARRAYS) {
        fprintf(stderr, "seni_carve: registry full (%d arrays) carving '%s'\n",
                SENI_REGISTRY_MAX_ARRAYS, name);
        return NULL;
    }
    if (stride != 0 && count > ((size_t)-1 - SENI_REGISTRY_ALIGN) / stride) {
        fprintf(stderr, "seni_carve: '%s' count %lu * stride %lu overflows size_t\n",
                name, (unsigned long)count, (unsigned long)stride);
        return NULL;
    }
    bytes = (count * stride + (SENI_REGISTRY_ALIGN - 1))
            & ~(size_t)(SENI_REGISTRY_ALIGN - 1);
    if (bytes > r->cap - r->used) {
        fprintf(stderr, "seni_carve: '%s' needs %lu bytes, block has %lu of %lu free\n",
                name, (unsigned long)bytes,
                (unsigned long)(r->cap - r->used), (unsigned long)r->cap);
        return NULL;
    }
    e = &r->entries[r->entry_count];
    strcpy(e->name, name);
    e->offset = r->used;
    e->count = count;
    e->stride = stride;
    r->used += bytes;
    r->entry_count++;
    return (char*)base + e->offset;
}

#endif
