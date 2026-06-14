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

/* CSG evaluation works entirely in CALLER-OWNED scratch -- no file statics, so
   the core is reentrant (two editors, parallel tests). One `henshu_scratch`
   bundles horu's BSP working memory with the fold's ping-pong result buffers
   and a per-shape buffer; the caller passes a region of >= HENSHU_SCRATCH_SIZE
   bytes (the game carves it from its transient block). */
#define HENSHU_MAX_POLYS    4096   /* cap on the folded result */
#define HENSHU_SHAPE_POLYS  512    /* cap on one primitive's polygons */

typedef struct {
    unsigned char bsp[HORU_CSG_SCRATCH];  /* horu_csg_polys working memory */
    horu_poly fold_a[HENSHU_MAX_POLYS];   /* ping-pong: the running fold result */
    horu_poly fold_b[HENSHU_MAX_POLYS];
    horu_poly fold_s[HENSHU_SHAPE_POLYS]; /* one shape being folded in */
} henshu_scratch;
#define HENSHU_SCRATCH_SIZE (sizeof(henshu_scratch))

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

/* fold the whole shape list into out[]; returns the polygon count. All working
   memory (BSP + ping-pong + per-shape) is the caller's `scratch`, a region of
   >= HENSHU_SCRATCH_SIZE bytes (a `henshu_scratch`), never the stack. */
int  henshu_eval_all(const EditorState *e, horu_poly *out, int cap, void *scratch);

/* build the tsumami pick targets: every shape is a draggable body (id = i*4,
   axis -1); the SELECTED shape also gets three axis-arrow handles
   (id = i*4 + axis + 1). Returns the target count (<= cap). */
int  henshu_targets(const EditorState *e, tsu_target *t, int cap);

#endif /* HENSHU_H */
