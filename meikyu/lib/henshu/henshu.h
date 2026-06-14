#ifndef HENSHU_H
#define HENSHU_H

/* henshu (編集) -- reusable CSG scene editor, CORE.

   This is the testable half: pure operations on EditorState plus the CSG
   evaluation that turns the shape list into geometry. It depends only on the
   leaf libs horu (CSG) and tsumami (pick targets) -- no render, no UI -- so it
   builds and is coverage-gated like horu/tsumami. The render/UI glue (panels,
   gizmo draw, mesh upload, resize grips) lives in henshu_ui.h, compiled only
   into the game dll.

   The model is a FLAT LIST of shape entities -- no boolean "nodes". Each slot is
   one primitive; the rendered solid is the LEFT FOLD of the list:
       result = shape[0];   for i>0:  result = shape[i].op(result, shape[i])
   so a difference/intersection shape acts on everything earlier in the list. */

#include "henshu_state.h"
#include "horu.h"
#include "tsumami.h"

/* shape kinds (csg_kind) */
enum { HENSHU_BOX, HENSHU_SPHERE, HENSHU_CYL, HENSHU_POLY };
/* fold operations (csg_op); slot 0 (the base) ignores its op */
enum { HENSHU_UNION, HENSHU_DIFF, HENSHU_ISECT };

/* a fresh starter model: a box with a sphere subtracted. selects the sphere. */
void henshu_default(EditorState *e);

/* the live hot model may predate this flat-list format (old data had op nodes
   whose kinds fall outside 0..3); 0 means re-init with henshu_default. */
int  henshu_model_ok(const EditorState *e);

/* append a unioned box and select it; tweak kind/op/transform after. no-op when
   the list is full. */
void henshu_add(EditorState *e);
/* set the selected shape's primitive kind / fold op. set_op is a no-op on slot 0
   (the base has no op). both mark the model dirty. */
void henshu_set_kind(EditorState *e, int kind);
void henshu_set_op(EditorState *e, int op);

/* human labels for kind/op codes (for the inspector + scene list). */
const char *henshu_kind_name(int k);
const char *henshu_op_name(int o);

/* the AABB half-extent of one shape (boxes use size, spheres/columns radius). */
void henshu_shape_half(const EditorState *e, int i, float *hx, float *hy, float *hz);

/* evaluate one shape into out[]; returns the polygon count. */
int  henshu_eval_shape(const EditorState *e, int i, horu_poly *out, int cap);

/* fold the whole shape list into out[]; returns the polygon count. The boolean
   work happens in `scratch` (>= HORU_CSG_SCRATCH bytes), never the stack. */
int  henshu_eval_all(const EditorState *e, horu_poly *out, int cap, void *scratch);

/* build the tsumami pick targets: every shape is a draggable body (id = i*4,
   axis -1); the SELECTED shape also gets three axis-arrow handles
   (id = i*4 + axis + 1). Returns the target count (<= cap). */
int  henshu_targets(const EditorState *e, tsu_target *t, int cap);

#endif /* HENSHU_H */
