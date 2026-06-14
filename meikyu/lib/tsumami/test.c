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
    t[0].id = 10; t[0].center = mk(0,0,0);  t[0].half = mk(1,1,1);
    t[1].id = 20; t[1].center = mk(0,0,-8); t[1].half = mk(1,1,1); /* farther */
    /* ray down -z hits both; nearer (id 10) wins */
    CHECK(tsu_pick(mkray(0,0,5, 0,0,-1), t, 2) == 10);
    /* ray that misses both */
    CHECK(tsu_pick(mkray(9,9,5, 0,0,-1), t, 2) == -1);
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
    tg[0].id = 3; tg[0].center = mk(0,0,-9); tg[0].half = mk(1,1,1); /* decoy */
    tg[1].id = 7; tg[1].center = mk(0,0,0);  tg[1].half = mk(1,1,1);
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
    tsu_gizmo_init(&g);
    /* press where nothing is -> no selection, no drag */
    m = tsu_gizmo_update(&g, mkray(9,9,5, 0,0,-1), 1, fwd, &tg, 1, &id, &mv);
    CHECK(g.selected == -1);
    CHECK(g.dragging == 0);
    CHECK(m == 0);
}

int main(void) {
    test_ray_aabb();
    test_pick();
    test_ray_plane();
    test_ray_from_screen();
    test_gizmo_select_drag_release();
    test_gizmo_press_empty();
    if (g_fail) { printf("%d check(s) failed\n", g_fail); return 1; }
    printf("tsumami: all checks passed\n");
    return 0;
}
