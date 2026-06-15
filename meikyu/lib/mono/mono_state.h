#ifndef MONO_STATE_H
#define MONO_STATE_H

/* mono (物, "thing"): the entity model. ONE fat Entity struct that carries every
   field, held in a fixed, SPARSE array -- not archetype/component arrays. A
   project nests `mono_world world;` in its game_state and seni migrates it (the
   array-of-structs migration). This header is the one place the entity layout
   lives; like henshu's EditorState it is prepended to the game_state.h snapshot
   (the marker's `layout_include`) so the reload diff can resolve the field.

   Keep it seni-parseable: scalars and fixed arrays only, no pointers. Entity
   refs are handles (mono_id = index + generation), never pointers -- already the
   hot-state law. The struct starts minimal (kind + transform); systems add their
   fields here as they land. The arrays below use a LITERAL size -- seni reads
   this as raw text and cannot expand a macro in an array size. */

#include "seni_annotations.h"

/* entity kinds. 0 is reserved: a slot with kind == MONO_NONE is FREE (the sparse
   liveness flag). add real kinds here. */
enum { MONO_NONE = 0, MONO_PROP, MONO_PLAYER, MONO_LIGHT, MONO_CSG };

#define MONO_MAX 1024   /* entity capacity; must match the [1024] arrays below */

typedef struct {
    int   kind;        /* MONO_NONE = free slot; else the entity's kind */
    int   generation;  /* bumped on free; the high half of a handle. starts at 1
                          so a zeroed handle (id 0) never matches a live slot. */

    /* transform -- the one component every entity has. rotation is euler
       degrees for now (editor-friendly); revisit as quaternion if gimbal bites. */
    float px, py, pz;
    float rx, ry, rz;
    float sx SENI_DEFAULT(1.0f), sy SENI_DEFAULT(1.0f), sz SENI_DEFAULT(1.0f);

    /* behaviour fields accrete here -- the fat struct grows, the model does not.
       spin: yaw rate in rad/s, a per-entity simulation input. */
    float spin;
} Entity;

typedef struct {
    Entity ents[1024];  /* 1024 == MONO_MAX (literal for seni) */
    int    count;       /* number of live (kind != MONO_NONE) slots */
} mono_world;

#endif /* MONO_STATE_H */
