#ifndef MONO_H
#define MONO_H

/* mono (物) -- the entity model: a sparse fixed table of fat Entity structs,
   addressed by stable generational handles. Pure operations on `mono_world` (no
   render, no UI, no allocator) -- coverage-gated like horu/tsumami. Iterate the
   live set directly: `for (i = 0; i < MONO_MAX; i++) if (w->ents[i].kind) ...`.

   The model: one Entity type carries every field; a slot's `kind == MONO_NONE`
   means free (sparse, with holes -- never compacted, so indices stay stable). A
   handle packs the slot index with its generation; freeing a slot bumps the
   generation, so a handle held across a free+reuse resolves to NULL instead of
   silently aliasing the new occupant. */

#include "mono_state.h"

/* a stable entity handle: generation in the high 16 bits, slot index in the low
   16. MONO_ID(0) is the invalid handle (a zeroed ref) -- generations start at 1,
   so it never matches a live slot. */
typedef unsigned int mono_id;
#define MONO_ID(index, gen) ((mono_id)(((unsigned)(gen) << 16) | ((unsigned)(index) & 0xFFFFu)))
#define MONO_INVALID ((mono_id)0)

/* clear the table: every slot free, generations seeded to 1, count 0. */
void mono_init(mono_world *w);

/* claim a free slot for an entity of `kind` (must not be MONO_NONE); returns its
   handle, or MONO_INVALID if the table is full. The slot's transform is
   identity (position 0, rotation 0, scale 1). */
mono_id mono_spawn(mono_world *w, int kind);

/* free the slot behind `id` (no-op if already dead/stale). Bumps the slot's
   generation so existing handles to it go stale. */
void mono_destroy(mono_world *w, mono_id id);

/* resolve a handle to its entity, or NULL if the slot is free or the generation
   no longer matches (a stale handle). The only safe way to dereference a ref. */
Entity *mono_get(mono_world *w, mono_id id);

/* 1 if `id` resolves to a live entity, else 0. */
int mono_alive(mono_world *w, mono_id id);

#endif /* MONO_H */
