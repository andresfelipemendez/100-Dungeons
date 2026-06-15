/* mono core -- sparse generational entity table. Pure ops on mono_world. */

#include "mono.h"
#include <stddef.h>  /* NULL */
#include <string.h>  /* memset */

void mono_init(mono_world *w) {
    int i;
    for (i = 0; i < MONO_MAX; i++) {
        w->ents[i].kind = MONO_NONE;
        w->ents[i].generation = 1; /* >=1 so MONO_INVALID (id 0) never resolves */
    }
    w->count = 0;
}

mono_id mono_spawn(mono_world *w, int kind) {
    int i;
    if (kind == MONO_NONE) {
        return MONO_INVALID; /* MONO_NONE is the free marker, not a real kind */
    }
    for (i = 0; i < MONO_MAX; i++) {
        if (w->ents[i].kind == MONO_NONE) {
            Entity *e = &w->ents[i];
            int gen = e->generation;      /* survives the wipe below */
            /* clean slate: a reused slot must not leak the previous entity's
               fields. zero everything, then restore the generation and set the
               defaults a fresh entity needs (kind + identity scale; the rest is
               zero -- position 0, no rotation). every future fat-struct field
               thus starts zeroed on spawn unless the caller sets it. */
            memset(e, 0, sizeof *e);
            e->generation = gen;
            e->kind = kind;
            e->sx = e->sy = e->sz = 1.0f;
            w->count++;
            return MONO_ID(i, e->generation);
        }
    }
    return MONO_INVALID; /* table full */
}

Entity *mono_get(mono_world *w, mono_id id) {
    unsigned idx, gen;
    Entity *e;
    if (id == MONO_INVALID) {
        return NULL;
    }
    idx = id & 0xFFFFu;
    gen = id >> 16;
    if (idx >= (unsigned)MONO_MAX) {
        return NULL;
    }
    e = &w->ents[idx];
    if (e->kind == MONO_NONE || (unsigned)e->generation != gen) {
        return NULL; /* freed slot, or a stale handle from before a reuse */
    }
    return e;
}

void mono_destroy(mono_world *w, mono_id id) {
    Entity *e = mono_get(w, id);
    if (!e) {
        return; /* already dead or stale -- idempotent */
    }
    e->kind = MONO_NONE;
    e->generation++; /* invalidate every outstanding handle to this slot */
    w->count--;
}

int mono_alive(mono_world *w, mono_id id) {
    return mono_get(w, id) != NULL;
}
