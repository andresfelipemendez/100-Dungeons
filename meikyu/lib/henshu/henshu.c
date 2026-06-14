/* henshu core -- CSG model operations on EditorState. No render, no UI; all
   working memory is the caller's scratch, so the core is reentrant. */

#include "henshu.h"

/* csg_kind[] etc. in henshu_state.h MUST stay sized HENSHU_MAX_SHAPES. seni
   needs the literal 32 there (it cannot expand a macro in raw header text), so
   this catches the two drifting apart -- a compile error, not a silent bug. */
typedef char henshu_shapes_count_check[
    (sizeof(((EditorState *)0)->csg_kind) / sizeof(int) == HENSHU_MAX_SHAPES) ? 1 : -1];

int henshu_eval_shape(const EditorState *e, int i, horu_poly *out, int cap) {
    int k = e->csg_kind[i];
    float x = e->csg_x[i], y = e->csg_y[i], z = e->csg_z[i];
    if (k == HENSHU_SPHERE) {
        return horu_sphere_polys(x, y, z, e->csg_sx[i], 8, 4, out, cap);
    }
    if (k == HENSHU_CYL || k == HENSHU_POLY) {
        return horu_prism_polys(x, y, z, e->csg_sx[i], e->csg_sy[i],
                                (k == HENSHU_CYL) ? 12 : 6, out, cap);
    }
    return horu_box_polys(x, y, z, e->csg_sx[i], e->csg_sy[i], e->csg_sz[i],
                          out, cap); /* HENSHU_BOX (default) */
}

int henshu_eval_all(const EditorState *e, horu_poly *out, int cap, void *scratch) {
    henshu_scratch *sc = (henshu_scratch *)scratch;
    horu_poly *acc = sc->fold_a, *nxt = sc->fold_b, *tmp;
    int i, n, m;
    if (e->csg_count <= 0) {
        return 0;
    }
    n = henshu_eval_shape(e, 0, acc, HENSHU_MAX_POLYS);  /* base shape */
    for (i = 1; i < e->csg_count; i++) {
        horu_op op = (e->csg_op[i] == HENSHU_DIFF)  ? HORU_DIFFERENCE
                   : (e->csg_op[i] == HENSHU_ISECT) ? HORU_INTERSECTION
                                                    : HORU_UNION;
        m = henshu_eval_shape(e, i, sc->fold_s, HENSHU_SHAPE_POLYS);
        n = horu_csg_polys(op, acc, n, sc->fold_s, m, nxt, HENSHU_MAX_POLYS,
                           sc->bsp, HORU_CSG_SCRATCH);
        tmp = acc; acc = nxt; nxt = tmp; /* ping-pong */
    }
    if (n > cap) n = cap;
    for (i = 0; i < n; i++) out[i] = acc[i];
    return n;
}

void henshu_shape_half(const EditorState *e, int i, float *hx, float *hy, float *hz) {
    int k = e->csg_kind[i];
    if (k == HENSHU_SPHERE) {
        *hx = *hy = *hz = e->csg_sx[i];
    } else if (k == HENSHU_CYL || k == HENSHU_POLY) {
        *hx = *hz = e->csg_sx[i]; *hy = e->csg_sy[i] * 0.5f;
    } else { /* box */
        *hx = e->csg_sx[i] * 0.5f; *hy = e->csg_sy[i] * 0.5f;
        *hz = e->csg_sz[i] * 0.5f;
    }
}

/* gizmo handle geometry: id = i*4 + slot; slot 0 = body (free drag, axis -1),
   slots 1/2/3 = the X/Y/Z arrow handles (axis-locked), only for the selection. */
#define HENSHU_GIZ_LEN 0.7f   /* arrow half-length along its axis */
#define HENSHU_GIZ_R   0.22f  /* arrow pick radius */

int henshu_targets(const EditorState *e, tsu_target *t, int cap) {
    int i, a, n = 0;
    for (i = 0; i < e->csg_count; i++) {
        tsu_v3 c;
        float hx, hy, hz;
        if (n >= cap) {
            break;
        }
        c.x = e->csg_x[i]; c.y = e->csg_y[i]; c.z = e->csg_z[i];
        henshu_shape_half(e, i, &hx, &hy, &hz);
        t[n].id = i * 4; t[n].center = c; t[n].origin = c; t[n].axis = -1;
        t[n].half.x = hx; t[n].half.y = hy; t[n].half.z = hz;
        n++;
        if (i != e->csg_selected) {
            continue;
        }
        for (a = 0; a < 3 && n < cap; a++) { /* the three axis arrows */
            tsu_v3 ac = c;
            if (a == 0) ac.x += HENSHU_GIZ_LEN; else if (a == 1) ac.y += HENSHU_GIZ_LEN;
            else ac.z += HENSHU_GIZ_LEN;
            t[n].id = i * 4 + a + 1;
            t[n].center = ac;
            t[n].origin = c;
            t[n].axis = a;
            t[n].half.x = (a == 0) ? HENSHU_GIZ_LEN : HENSHU_GIZ_R;
            t[n].half.y = (a == 1) ? HENSHU_GIZ_LEN : HENSHU_GIZ_R;
            t[n].half.z = (a == 2) ? HENSHU_GIZ_LEN : HENSHU_GIZ_R;
            n++;
        }
    }
    return n;
}

void henshu_add(EditorState *e) {
    int p;
    if (e->csg_count >= HENSHU_MAX_SHAPES) {
        return;
    }
    p = e->csg_count++;
    e->csg_kind[p] = HENSHU_BOX;
    e->csg_op[p] = HENSHU_UNION;
    e->csg_x[p] = 0.0f; e->csg_y[p] = 2.0f; e->csg_z[p] = 0.0f;
    e->csg_sx[p] = 1.0f; e->csg_sy[p] = 1.0f; e->csg_sz[p] = 1.0f;
    e->csg_selected = p;
    e->csg_dirty = 1;
}

void henshu_set_kind(EditorState *e, int kind) {
    int s = e->csg_selected;
    if (s < 0 || s >= e->csg_count) {
        return;
    }
    e->csg_kind[s] = kind;
    e->csg_dirty = 1;
}

void henshu_set_op(EditorState *e, int op) {
    int s = e->csg_selected;
    if (s < 1 || s >= e->csg_count) { /* slot 0 is the base -- no op */
        return;
    }
    e->csg_op[s] = op;
    e->csg_dirty = 1;
}

void henshu_default(EditorState *e) {
    e->csg_kind[0] = HENSHU_BOX;    e->csg_op[0] = HENSHU_UNION;
    e->csg_x[0] = 0; e->csg_y[0] = 0; e->csg_z[0] = 0;
    e->csg_sx[0] = 2; e->csg_sy[0] = 2; e->csg_sz[0] = 2;
    e->csg_kind[1] = HENSHU_SPHERE; e->csg_op[1] = HENSHU_DIFF;
    e->csg_x[1] = 1; e->csg_y[1] = 1; e->csg_z[1] = 1;
    e->csg_sx[1] = 1.1f; e->csg_sy[1] = 1.1f; e->csg_sz[1] = 1.1f;
    e->csg_count = 2; e->csg_selected = 1;
    e->drag_node = -1; e->csg_dirty = 1;
}

int henshu_model_ok(const EditorState *e) {
    int i;
    if (e->csg_count < 1 || e->csg_count > HENSHU_MAX_SHAPES) {
        return 0;
    }
    for (i = 0; i < e->csg_count; i++) {
        if (e->csg_kind[i] < HENSHU_BOX || e->csg_kind[i] > HENSHU_POLY) {
            return 0;
        }
    }
    return 1;
}

const char *henshu_kind_name(int k) {
    switch (k) {
    case HENSHU_BOX:    return "box";
    case HENSHU_SPHERE: return "sphere";
    case HENSHU_CYL:    return "cylinder";
    case HENSHU_POLY:   return "polygon";
    default:            return "?";
    }
}

const char *henshu_op_name(int o) {
    switch (o) {
    case HENSHU_UNION: return "union";
    case HENSHU_DIFF:  return "diff";
    case HENSHU_ISECT: return "isect";
    default:           return "?";
    }
}
