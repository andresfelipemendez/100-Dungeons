/* tsumami tests: pure, headless -- the gizmo state machine is driven by
   synthetic rays + mouse edges, no UI. Strict C89. */

#include "tsumami.h"
#include <stdio.h>
#include <math.h>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int feq(float a, float b) { return (float)fabs(a - b) < 1e-4f; }

static tsu_v3 mk(float x, float y, float z) { tsu_v3 v; v.x=x; v.y=y; v.z=z; return v; }
static tsu_ray mkray(float ox,float oy,float oz, float dx,float dy,float dz) {
    tsu_ray r; r.o = mk(ox,oy,oz); r.d = mk(dx,dy,dz); return r;
}

static void test_ray_aabb(void) {
    /* unit box at origin [-1,1]^3 */
    tsu_v3 mn = mk(-1,-1,-1), mx = mk(1,1,1);
    /* ray from +z toward origin: enters the +z face at z=1 -> t=4 */
    CHECK(feq(tsu_ray_aabb(mkray(0,0,5, 0,0,-1), mn, mx), 4.0f));
    /* parallel miss in x (x=5, ray travels in -z) */
    CHECK(tsu_ray_aabb(mkray(5,0,5, 0,0,-1), mn, mx) < 0.0f);
    /* box behind the ray (ray goes +z, box in front toward -z is missed) */
    CHECK(tsu_ray_aabb(mkray(0,0,5, 0,0,1), mn, mx) < 0.0f);
    /* origin inside -> entry distance 0 */
    CHECK(feq(tsu_ray_aabb(mkray(0,0,0, 0,0,-1), mn, mx), 0.0f));
    /* angled miss (points away in x) */
    CHECK(tsu_ray_aabb(mkray(0,0,5, 1,0,0), mn, mx) < 0.0f);
    /* diagonal hit: all three slabs active, asymmetric -> exercises the
       tmin/tmax "no update" branches. dir normalized (-2,-1,-2)/3. */
    CHECK(tsu_ray_aabb(mkray(4,2,4, -2.0f/3, -1.0f/3, -2.0f/3), mn, mx) >= 0.0f);
    /* diagonal miss: shoots past the box corner */
    CHECK(tsu_ray_aabb(mkray(4,9,4, -2.0f/3, -1.0f/3, -2.0f/3), mn, mx) < 0.0f);
}

static void test_pick(void) {
    tsu_target t[2];
    t[0].id = 10; t[0].center = mk(0,0,0);  t[0].half = mk(1,1,1); t[0].axis = -1;
    t[1].id = 20; t[1].center = mk(0,0,-8); t[1].half = mk(1,1,1); t[1].axis = -1;
    /* ray down -z hits both bodies; nearer (id 10) wins */
    CHECK(tsu_pick(mkray(0,0,5, 0,0,-1), t, 2) == 10);
    /* ray that misses both */
    CHECK(tsu_pick(mkray(9,9,5, 0,0,-1), t, 2) == -1);
}

/* gizmo handles (axis >= 0) win over object bodies (axis -1) even when a body
   is nearer along the ray -- otherwise a primitive in front blocks the
   selected entity's arrows (the box-behind-sphere reset bug). */
static void test_pick_handle_priority(void) {
    tsu_target t[4];
    t[0].id = 10; t[0].center = mk(0,0,2);  t[0].half = mk(1,1,1); t[0].axis = -1; /* body, near */
    t[1].id = 21; t[1].center = mk(0,0,-3); t[1].half = mk(1,1,1); t[1].axis = 0;  /* handle, far */
    t[2].id = 22; t[2].center = mk(0,0,0);  t[2].half = mk(1,1,1); t[2].axis = 1;  /* handle, nearer */
    t[3].id = 23; t[3].center = mk(0,0,-6); t[3].half = mk(1,1,1); t[3].axis = 2;  /* handle, farthest */
    /* nearer handle wins; t[3] (farther than the running best handle) takes the
       d<bth false arm without displacing the winner */
    CHECK(tsu_pick(mkray(0,0,5, 0,0,-1), t, 4) == 22);
    /* move all handles off the ray: only the body is hit, so it wins */
    t[1].center = mk(9,9,9);
    t[2].center = mk(9,9,9);
    t[3].center = mk(9,9,9);
    CHECK(tsu_pick(mkray(0,0,5, 0,0,-1), t, 4) == 10);
}

static void test_ray_plane(void) {
    /* plane z=0 normal +z, ray from (2,0,5) down -z -> hit (2,0,0) */
    tsu_v3 hit = tsu_ray_plane(mkray(2,0,5, 0,0,-1), mk(0,0,0), mk(0,0,1));
    CHECK(feq(hit.x, 2.0f) && feq(hit.y, 0.0f) && feq(hit.z, 0.0f));
}

static void test_ray_from_screen(void) {
    /* a centred pixel -> ray direction == forward */
    tsu_ray r = tsu_ray_from_screen(mk(0,0,5), mk(0,0,-1), mk(1,0,0), mk(0,1,0),
                                    0.5f, 1.0f, 640.0f, 360.0f, 1280.0f, 720.0f);
    CHECK(feq(r.d.x, 0.0f) && feq(r.d.y, 0.0f) && feq(r.d.z, -1.0f));
    CHECK(feq(r.o.z, 5.0f));
}

static void test_gizmo_select_drag_release(void) {
    /* two targets: the decoy (id 3) sits behind so the center lookup must
       skip a non-matching id, and picking must choose the nearer one. */
    tsu_target tg[2]; tsu_gizmo g;
    tsu_v3 fwd = mk(0,0,-1);          /* camera looks -z */
    int id = -1; tsu_v3 mv = mk(0,0,0);
    int m;
    tg[0].id = 3; tg[0].center = mk(0,0,-9); tg[0].half = mk(1,1,1);
    tg[0].origin = tg[0].center; tg[0].axis = -1; /* decoy */
    tg[1].id = 7; tg[1].center = mk(0,0,0);  tg[1].half = mk(1,1,1);
    tg[1].origin = tg[1].center; tg[1].axis = -1;
    tsu_gizmo_init(&g);
    CHECK(g.selected == -1);

    /* press over the near target -> select id 7 + begin drag (no move yet) */
    m = tsu_gizmo_update(&g, mkray(0,0,5, 0,0,-1), 1, fwd, tg, 2, &id, &mv);
    CHECK(g.selected == 7);
    CHECK(g.dragging == 1);
    CHECK(m == 1 && id == 7);
    CHECK(feq(mv.x, 0.0f) && feq(mv.z, 0.0f)); /* grab frame: centre unchanged */

    /* hold + move the ray +2 in x -> object center follows to x=2 */
    m = tsu_gizmo_update(&g, mkray(2,0,5, 0,0,-1), 1, fwd, tg, 2, &id, &mv);
    CHECK(m == 1);
    CHECK(feq(mv.x, 2.0f));

    /* release -> stop dragging, no move */
    m = tsu_gizmo_update(&g, mkray(2,0,5, 0,0,-1), 0, fwd, tg, 2, &id, &mv);
    CHECK(m == 0);
    CHECK(g.dragging == 0);
}

static void test_gizmo_press_empty(void) {
    tsu_target tg; tsu_gizmo g;
    tsu_v3 fwd = mk(0,0,-1);
    int id = -1; tsu_v3 mv;
    int m;
    tg.id = 7; tg.center = mk(0,0,0); tg.half = mk(1,1,1);
    tg.origin = tg.center; tg.axis = -1;
    tsu_gizmo_init(&g);
    /* press where nothing is -> no selection, no drag */
    m = tsu_gizmo_update(&g, mkray(9,9,5, 0,0,-1), 1, fwd, &tg, 1, &id, &mv);
    CHECK(g.selected == -1);
    CHECK(g.dragging == 0);
    CHECK(m == 0);
}

static void test_gizmo_axis_constrained(void) {
    /* a Y-axis handle: the object is at the origin, the arrow box sits above it.
       Dragging must move ONLY in y, no matter how the ray moves in x/z. */
    tsu_target tg; tsu_gizmo g;
    tsu_v3 fwd = mk(0,0,-1);
    int id; tsu_v3 mv;
    int m;
    tg.id = 5;
    tg.center = mk(0, 0.7f, 0);       /* the arrow's pick box, offset +y */
    tg.half   = mk(0.3f, 0.8f, 0.3f);
    tg.origin = mk(0, 0, 0);          /* the object center (drag reference) */
    tg.axis   = 1;                    /* lock to Y */
    tsu_gizmo_init(&g);

    /* grab on the arrow (ray through the box at y~0.5) */
    m = tsu_gizmo_update(&g, mkray(0, 0.5f, 5, 0,0,-1), 1, fwd, &tg, 1, &id, &mv);
    CHECK(g.selected == 5 && g.grab_axis == 1 && m == 1);
    CHECK(feq(mv.x, 0.0f) && feq(mv.z, 0.0f)); /* grab: object stays put */

    /* drag the ray to (2, 3) -- the x=2 must NOT move the object, only y */
    m = tsu_gizmo_update(&g, mkray(2.0f, 3.0f, 5, 0,0,-1), 1, fwd, &tg, 1, &id, &mv);
    CHECK(m == 1);
    CHECK(feq(mv.x, 0.0f));  /* constrained: no x motion */
    CHECK(feq(mv.z, 0.0f));  /* no z motion */
    CHECK(feq(mv.y, 2.5f));  /* y follows: 3.0 - grab(0.5) = +2.5 */

    /* a grab with a ray PARALLEL to the locked axis: the closest-point solve
       degenerates -- it must return 0 (no divide-by-zero) and not move. */
    {
        tsu_gizmo g2;
        tsu_gizmo_init(&g2);
        m = tsu_gizmo_update(&g2, mkray(0,-5,0, 0,1,0), 1, fwd, &tg, 1, &id, &mv);
        CHECK(g2.selected == 5);
        m = tsu_gizmo_update(&g2, mkray(0,-3,0, 0,1,0), 1, fwd, &tg, 1, &id, &mv);
        CHECK(feq(mv.y, 0.0f)); /* degenerate -> stays at origin */
    }
}

static void test_gizmo_axis_xz(void) {
    /* X- and Z-locked drags so axis_dir is exercised for axes 0 and 2 (the Y
       test only covered axis 1). */
    tsu_gizmo g; int id, m; tsu_v3 mv;
    tsu_v3 fwd = mk(0,0,-1);
    tsu_target tx, tz;

    tx.id = 1; tx.center = mk(0.7f,0,0); tx.half = mk(0.8f,0.3f,0.3f);
    tx.origin = mk(0,0,0); tx.axis = 0;          /* lock to X */
    tsu_gizmo_init(&g);
    m = tsu_gizmo_update(&g, mkray(0.5f,0,5, 0,0,-1), 1, fwd, &tx, 1, &id, &mv);
    CHECK(g.grab_axis == 0);
    m = tsu_gizmo_update(&g, mkray(3,2,5, 0,0,-1), 1, fwd, &tx, 1, &id, &mv);
    CHECK(m == 1 && feq(mv.y,0.0f) && feq(mv.z,0.0f)); /* only x -> axis_dir(0) */

    tz.id = 2; tz.center = mk(0,0,0.7f); tz.half = mk(0.3f,0.3f,0.8f);
    tz.origin = mk(0,0,0); tz.axis = 2;          /* lock to Z */
    tsu_gizmo_init(&g);
    m = tsu_gizmo_update(&g, mkray(5,0,0.5f, -1,0,0), 1, fwd, &tz, 1, &id, &mv);
    CHECK(g.grab_axis == 2);                       /* axis_dir(2) exercised */
    m = tsu_gizmo_update(&g, mkray(5,2,3, -1,0,0), 1, fwd, &tz, 1, &id, &mv);
    CHECK(m == 1 && feq(mv.x,0.0f) && feq(mv.y,0.0f)); /* only z */
}

static void test_pick_id_zero_and_tie(void) {
    /* a hit handle with id 0: distinguishes best_handle >= 0 from > 0 (the id
       is a valid result, not "none") */
    tsu_target h[1];
    h[0].id = 0; h[0].center = mk(0,0,0); h[0].half = mk(1,1,1); h[0].axis = 1;
    CHECK(tsu_pick(mkray(0,0,5, 0,0,-1), h, 1) == 0);
    /* two bodies at the SAME distance: the strict `<` keeps the first seen, so
       a `<=` mutant (which would switch to the second) is killed */
    {
        tsu_target b[2];
        b[0].id = 100; b[0].center = mk(0,0,0); b[0].half = mk(1,1,1); b[0].axis = -1;
        b[1].id = 200; b[1].center = mk(0,0,0); b[1].half = mk(1,1,1); b[1].axis = -1;
        CHECK(tsu_pick(mkray(0,0,5, 0,0,-1), b, 2) == 100);
    }
}

static void test_gizmo_no_press(void) {
    /* mouse already down (no fresh press edge): the press-edge guard
       (mouse_down && !prev_down) must be false -> no grab. Kills the
       '&&' -> '||' mutant, which would grab without an edge. */
    tsu_target tg; tsu_gizmo g;
    tsu_v3 fwd = mk(0,0,-1);
    int id = -1; tsu_v3 mv;
    int m;
    tg.id = 9; tg.center = mk(0,0,0); tg.half = mk(1,1,1);
    tg.origin = tg.center; tg.axis = -1;
    tsu_gizmo_init(&g);
    g.prev_down = 1;   /* button was already down last frame: no press edge */
    m = tsu_gizmo_update(&g, mkray(0,0,5, 0,0,-1), 1, fwd, &tg, 1, &id, &mv);
    CHECK(g.selected == -1 && m == 0);
}

static void test_ray_degenerate(void) {
    /* a zero forward + zero basis at screen center: norm3 of a zero vector
       takes its l<=0 arm and returns the vector unchanged (no divide-by-zero) */
    tsu_v3 z = mk(0,0,0);
    tsu_ray r = tsu_ray_from_screen(z, z, z, z, 1.0f, 1.0f, 0.5f, 0.5f, 1.0f, 1.0f);
    CHECK(feq(r.d.x,0.0f) && feq(r.d.y,0.0f) && feq(r.d.z,0.0f));
}

int main(void) {
    test_ray_aabb();
    test_pick();
    test_pick_handle_priority();
    test_pick_id_zero_and_tie();
    test_gizmo_no_press();
    test_gizmo_axis_xz();
    test_ray_degenerate();
    test_ray_plane();
    test_ray_from_screen();
    test_gizmo_select_drag_release();
    test_gizmo_axis_constrained();
    test_gizmo_press_empty();
    if (g_fail) { printf("%d check(s) failed\n", g_fail); return 1; }
    printf("tsumami: all checks passed\n");
    return 0;
}
