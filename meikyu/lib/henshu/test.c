/* henshu core tests: pure CSG model ops on EditorState, headless. Strict C89. */

#include "henshu.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static henshu_scratch g_scratch;   /* caller-owned eval scratch */
static horu_poly g_polys[4096];

static void test_default(void) {
    EditorState e;
    memset(&e, 0, sizeof e);
    henshu_default(&e);
    CHECK(e.csg_count == 2);
    CHECK(e.csg_kind[0] == HENSHU_BOX);
    CHECK(e.csg_op[0] == HENSHU_UNION);
    CHECK(e.csg_kind[1] == HENSHU_SPHERE);
    CHECK(e.csg_op[1] == HENSHU_DIFF);
    CHECK(e.csg_selected == 1);
    CHECK(e.drag_node == -1);
    CHECK(e.csg_dirty == 1);
    CHECK(henshu_model_ok(&e));
}

static void test_model_ok(void) {
    EditorState e;
    memset(&e, 0, sizeof e);
    henshu_default(&e);
    e.csg_count = 0;                CHECK(!henshu_model_ok(&e));
    e.csg_count = HENSHU_MAX_SHAPES + 1; CHECK(!henshu_model_ok(&e));
    e.csg_count = 2;
    e.csg_kind[1] = 99;             CHECK(!henshu_model_ok(&e)); /* op-node leftover */
    e.csg_kind[1] = -1;             CHECK(!henshu_model_ok(&e));
    e.csg_kind[1] = HENSHU_SPHERE;  CHECK(henshu_model_ok(&e));
}

static void test_add(void) {
    EditorState e;
    int i;
    memset(&e, 0, sizeof e);
    henshu_default(&e);
    e.csg_dirty = 0;
    henshu_add(&e);
    CHECK(e.csg_count == 3);
    CHECK(e.csg_kind[2] == HENSHU_BOX);
    CHECK(e.csg_op[2] == HENSHU_UNION);
    CHECK(e.csg_selected == 2);
    CHECK(e.csg_dirty == 1);
    /* fill to the cap: further adds are no-ops, count never overflows */
    for (i = e.csg_count; i < HENSHU_MAX_SHAPES; i++) henshu_add(&e);
    CHECK(e.csg_count == HENSHU_MAX_SHAPES);
    henshu_add(&e);
    CHECK(e.csg_count == HENSHU_MAX_SHAPES);
}

static void test_set_kind_op(void) {
    EditorState e;
    memset(&e, 0, sizeof e);
    henshu_default(&e); /* selected = 1 */
    e.csg_dirty = 0;
    henshu_set_kind(&e, HENSHU_CYL);
    CHECK(e.csg_kind[1] == HENSHU_CYL);
    CHECK(e.csg_dirty == 1);
    e.csg_dirty = 0;
    henshu_set_op(&e, HENSHU_ISECT);
    CHECK(e.csg_op[1] == HENSHU_ISECT);
    CHECK(e.csg_dirty == 1);
    /* op on the base (slot 0) is a no-op */
    e.csg_selected = 0; e.csg_dirty = 0;
    henshu_set_op(&e, HENSHU_DIFF);
    CHECK(e.csg_op[0] == HENSHU_UNION);
    CHECK(e.csg_dirty == 0);
    /* out-of-range selection guards both setters: below 0 AND at/above count */
    e.csg_selected = -1; e.csg_dirty = 0;
    henshu_set_kind(&e, HENSHU_BOX);
    henshu_set_op(&e, HENSHU_DIFF);
    CHECK(e.csg_dirty == 0);
    e.csg_selected = e.csg_count; e.csg_dirty = 0;
    henshu_set_kind(&e, HENSHU_BOX);
    henshu_set_op(&e, HENSHU_DIFF);
    CHECK(e.csg_dirty == 0);
}

static void test_names(void) {
    CHECK(strcmp(henshu_kind_name(HENSHU_BOX), "box") == 0);
    CHECK(strcmp(henshu_kind_name(HENSHU_SPHERE), "sphere") == 0);
    CHECK(strcmp(henshu_kind_name(HENSHU_CYL), "cylinder") == 0);
    CHECK(strcmp(henshu_kind_name(HENSHU_POLY), "polygon") == 0);
    CHECK(strcmp(henshu_kind_name(99), "?") == 0);
    CHECK(strcmp(henshu_op_name(HENSHU_UNION), "union") == 0);
    CHECK(strcmp(henshu_op_name(HENSHU_DIFF), "diff") == 0);
    CHECK(strcmp(henshu_op_name(HENSHU_ISECT), "isect") == 0);
    CHECK(strcmp(henshu_op_name(99), "?") == 0);
}

static void test_shape_half(void) {
    EditorState e;
    float hx, hy, hz;
    memset(&e, 0, sizeof e);
    e.csg_sx[0] = 2; e.csg_sy[0] = 4; e.csg_sz[0] = 6;
    e.csg_kind[0] = HENSHU_BOX;
    henshu_shape_half(&e, 0, &hx, &hy, &hz);
    CHECK(hx == 1.0f && hy == 2.0f && hz == 3.0f);  /* half of size */
    e.csg_kind[0] = HENSHU_SPHERE;
    henshu_shape_half(&e, 0, &hx, &hy, &hz);
    CHECK(hx == 2.0f && hy == 2.0f && hz == 2.0f);  /* radius = sx */
    e.csg_kind[0] = HENSHU_CYL;
    henshu_shape_half(&e, 0, &hx, &hy, &hz);
    CHECK(hx == 2.0f && hz == 2.0f && hy == 2.0f);  /* radius sx, half-height sy */
    e.csg_kind[0] = HENSHU_POLY;  /* prisms (cyl/poly) share the half-extent rule */
    henshu_shape_half(&e, 0, &hx, &hy, &hz);
    CHECK(hx == 2.0f && hz == 2.0f && hy == 2.0f);
}

static void test_eval(void) {
    EditorState e;
    int i, n;
    memset(&e, 0, sizeof e);
    /* each primitive kind yields some geometry */
    e.csg_count = 1; e.csg_selected = 0;
    e.csg_sx[0] = 1; e.csg_sy[0] = 1; e.csg_sz[0] = 1;
    for (i = HENSHU_BOX; i <= HENSHU_POLY; i++) {
        e.csg_kind[0] = i;
        CHECK(henshu_eval_shape(&e, 0, g_polys, 4096) > 0);
    }
    /* the default model (box minus sphere) folds to a watertight mesh */
    henshu_default(&e);
    n = henshu_eval_all(&e, g_polys, 4096, &g_scratch);
    CHECK(n > 0);
    /* the output is clamped to cap */
    CHECK(henshu_eval_all(&e, g_polys, 1, &g_scratch) == 1);
    /* an empty list yields nothing */
    e.csg_count = 0;
    CHECK(henshu_eval_all(&e, g_polys, 4096, &g_scratch) == 0);
    /* exercise every fold op: base box, a unioned box, an intersected sphere */
    memset(&e, 0, sizeof e);
    e.csg_count = 3; e.csg_selected = 0;
    e.csg_kind[0] = HENSHU_BOX;    e.csg_op[0] = HENSHU_UNION;
    e.csg_sx[0] = 2; e.csg_sy[0] = 2; e.csg_sz[0] = 2;
    e.csg_kind[1] = HENSHU_BOX;    e.csg_op[1] = HENSHU_UNION;
    e.csg_x[1] = 1; e.csg_sx[1] = 2; e.csg_sy[1] = 2; e.csg_sz[1] = 2;
    e.csg_kind[2] = HENSHU_SPHERE; e.csg_op[2] = HENSHU_ISECT;
    e.csg_sx[2] = 2;
    CHECK(henshu_eval_all(&e, g_polys, 4096, &g_scratch) > 0);
}

static void test_targets(void) {
    EditorState e;
    tsu_target t[64];
    int n;
    memset(&e, 0, sizeof e);
    henshu_default(&e); /* count 2, selected 1 */
    n = henshu_targets(&e, t, 64);
    /* 2 bodies + 3 axis arrows for the selection = 5 */
    CHECK(n == 5);
    CHECK(t[0].id == 0 && t[0].axis == -1);   /* shape 0 body */
    CHECK(t[1].id == 4 && t[1].axis == -1);   /* shape 1 body (id = i*4) */
    CHECK(t[2].id == 5 && t[2].axis == 0);    /* shape 1 X arrow (i*4 + axis + 1) */
    CHECK(t[3].id == 6 && t[3].axis == 1);
    CHECK(t[4].id == 7 && t[4].axis == 2);
    /* cap is honoured: 2 stops after both bodies; 1 stops at the top of the
       second iteration (the n >= cap guard). */
    CHECK(henshu_targets(&e, t, 2) == 2);
    CHECK(henshu_targets(&e, t, 1) == 1);
}

int main(void) {
    test_default();
    test_model_ok();
    test_add();
    test_set_kind_op();
    test_names();
    test_shape_half();
    test_eval();
    test_targets();
    if (g_fail) { printf("%d check(s) failed\n", g_fail); return 1; }
    printf("henshu: all checks passed\n");
    return 0;
}
