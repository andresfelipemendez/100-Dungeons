/* horu unit tests. Built + run via `meikyu --test horu` (see
   docs/superpowers/specs/2026-06-13-kaji-lib-test-builds-design.md).
   Strict C89: declarations at block top, no // comments. */

#include "horu.h"
#include <stdio.h>
#include <math.h>

static int g_fail = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        g_fail++; \
    } \
} while (0)

static int feq(float a, float b) { return (float)fabs(a - b) < 1e-5f; }

/* signed volume of a closed polygon soup: sum of tetrahedra (origin, fan).
   outward CCW polygons give a positive volume. */
static float poly_soup_volume(const horu_poly *p, int n) {
    float v6 = 0.0f;
    int i, k;
    for (i = 0; i < n; i++) {
        for (k = 0; k + 2 < p[i].n; k++) {
            horu_v3 a = p[i].v[0], b = p[i].v[k + 1], c = p[i].v[k + 2];
            float cx = b.y * c.z - b.z * c.y;
            float cy = b.z * c.x - b.x * c.z;
            float cz = b.x * c.y - b.y * c.x;
            v6 += a.x * cx + a.y * cy + a.z * cz;
        }
    }
    return v6 / 6.0f;
}

static int vol_eq(float a, float b) { return (float)fabs(a - b) < 0.02f; }

/* magnitude of the summed area-weighted triangle normals. For a CLOSED
   (watertight) surface this is ~0; a dropped/missing polygon leaves an open
   boundary and the sum no longer cancels -- so this catches holes that the
   volume check (a near-invariant scalar) cannot. */
static float surface_residual(const horu_poly *p, int n) {
    float rx = 0, ry = 0, rz = 0;
    int i, k;
    for (i = 0; i < n; i++) {
        for (k = 0; k + 2 < p[i].n; k++) {
            horu_v3 a = p[i].v[0], b = p[i].v[k + 1], c = p[i].v[k + 2];
            float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
            float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
            rx += e1y * e2z - e1z * e2y;
            ry += e1z * e2x - e1x * e2z;
            rz += e1x * e2y - e1y * e2x;
        }
    }
    return (float)sqrt((double)(rx * rx + ry * ry + rz * rz));
}

/* CSG working memory (the caller owns it -- here a static buffer) */
static char g_scratch[HORU_CSG_SCRATCH];
#define SCRATCH g_scratch, (int)sizeof g_scratch

/* ---- plane construction -------------------------------------------------- */

static void test_plane_make_normalizes(void) {
    /* input normal (0,3,0) with d=6 describes the set 3y = 6, i.e. y = 2.
       horu_plane_make must return a UNIT normal and scale d to match, so the
       described plane is unchanged: normal (0,1,0), d = 2. */
    horu_plane p = horu_plane_make(0.0f, 3.0f, 0.0f, 6.0f);
    CHECK(feq(p.nx, 0.0f));
    CHECK(feq(p.ny, 1.0f));
    CHECK(feq(p.nz, 0.0f));
    CHECK(feq(p.d, 2.0f));
}

static void test_plane_degenerate(void) {
    /* a zero-length normal has no direction: horu_plane_make returns the
       degenerate zero plane (exercises the len <= 0 branch). */
    horu_plane p = horu_plane_make(0.0f, 0.0f, 0.0f, 5.0f);
    CHECK(feq(p.nx, 0.0f));
    CHECK(feq(p.ny, 0.0f));
    CHECK(feq(p.nz, 0.0f));
    CHECK(feq(p.d, 0.0f));
}

static void test_side_classifies(void) {
    /* plane y = 2 with +normal pointing +y: the front (solid/inside) side is
       y > 2, behind is y < 2, and |dist| <= HORU_EPS counts as on. */
    horu_plane p = horu_plane_make(0.0f, 1.0f, 0.0f, 2.0f);
    CHECK(horu_side(p, 0.0f, 5.0f, 0.0f) == 1);   /* above -> front */
    CHECK(horu_side(p, 0.0f, 0.0f, 0.0f) == -1);  /* below -> behind */
    CHECK(horu_side(p, 3.0f, 2.0f, -7.0f) == 0);  /* exactly on */
    CHECK(horu_side(p, 0.0f, 2.0f + 1e-6f, 0.0f) == 0); /* within epsilon */
}

static void test_side_epsilon_boundary(void) {
    /* a point at EXACTLY +/-HORU_EPS distance counts as ON the plane, not
       front/behind. Pins the `> EPS` and `< -EPS` thresholds (a point clearly
       inside/outside can't tell `>` from `>=`). Plane through the origin so
       dist == y exactly, no float rounding. */
    horu_plane p = horu_plane_make(0.0f, 1.0f, 0.0f, 0.0f); /* y = 0 */
    CHECK(horu_side(p, 0.0f,  HORU_EPS, 0.0f) == 0);
    CHECK(horu_side(p, 0.0f, -HORU_EPS, 0.0f) == 0);
}

static void test_plane_from_points(void) {
    /* a=(0,0,0) b=(1,0,0) c=(0,1,0): CCW seen from +z, so normal is +z and the
       plane is z = 0. */
    horu_plane p = horu_plane_from_points(0.0f, 0.0f, 0.0f,
                                          1.0f, 0.0f, 0.0f,
                                          0.0f, 1.0f, 0.0f);
    CHECK(feq(p.nx, 0.0f));
    CHECK(feq(p.ny, 0.0f));
    CHECK(feq(p.nz, 1.0f));
    CHECK(feq(p.d, 0.0f));
    CHECK(horu_side(p, 0.0f, 0.0f, 1.0f) == 1);  /* above -> front */
    CHECK(horu_side(p, 0.0f, 0.0f, -1.0f) == -1); /* below -> behind */
}

static void test_solid_contains(void) {
    /* a slab -1 <= x <= 1 from two faces with OUTWARD normals:
       +x face at x=1, -x face at x=-1. inside = behind both. */
    horu_solid s;
    s.count = 0;
    s.planes[s.count++] = horu_plane_make( 1.0f, 0.0f, 0.0f, 1.0f); /* x=1 */
    s.planes[s.count++] = horu_plane_make(-1.0f, 0.0f, 0.0f, 1.0f); /* x=-1 */
    CHECK(horu_contains(&s, 0.0f, 0.0f, 0.0f) == 1);  /* inside */
    CHECK(horu_contains(&s, 0.5f, 9.0f, 9.0f) == 1);  /* inside (slab is in x) */
    CHECK(horu_contains(&s, 2.0f, 0.0f, 0.0f) == 0);  /* past +x face */
    CHECK(horu_contains(&s, -2.0f, 0.0f, 0.0f) == 0); /* past -x face */
    CHECK(horu_contains(&s, 1.0f, 0.0f, 0.0f) == 1);  /* exactly on a face */
}

static void test_box(void) {
    /* unit box [-1,1]^3 centred at origin */
    horu_solid b = horu_box(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);
    CHECK(b.count == 6);
    CHECK(horu_contains(&b, 0.0f, 0.0f, 0.0f) == 1);    /* centre */
    CHECK(horu_contains(&b, 0.9f, -0.9f, 0.9f) == 1);   /* near a corner */
    CHECK(horu_contains(&b, 1.0f, 1.0f, 1.0f) == 1);    /* on a corner */
    CHECK(horu_contains(&b, 1.1f, 0.0f, 0.0f) == 0);    /* past +x */
    CHECK(horu_contains(&b, 0.0f, 0.0f, -1.1f) == 0);   /* past -z */

    /* off-centre box, asymmetric extents */
    b = horu_box(5.0f, 0.0f, 0.0f, 2.0f, 4.0f, 6.0f);
    CHECK(horu_contains(&b, 5.0f, 1.9f, 2.9f) == 1);
    CHECK(horu_contains(&b, 3.9f, 0.0f, 0.0f) == 0);    /* past -x (x<4) */
}

static void test_csg_booleans(void) {
    /* two slabs overlapping in x: A spans x in [-1,1], B spans x in [0,2]
       (both huge in y,z). Four regions along x exercise every op and every
       &&/|| condition pair: A-only, both, B-only, neither. */
    horu_solid A = horu_box(0.0f, 0.0f, 0.0f, 2.0f, 100.0f, 100.0f);
    horu_solid B = horu_box(1.0f, 0.0f, 0.0f, 2.0f, 100.0f, 100.0f);
    horu_csg t;
    int la, lb;

    /* union: inside if in A OR B */
    horu_csg_init(&t);
    la = horu_csg_leaf(&t, &A);
    lb = horu_csg_leaf(&t, &B);
    horu_csg_op(&t, HORU_UNION, la, lb);
    CHECK(horu_csg_contains(&t, -0.5f, 0.0f, 0.0f) == 1); /* A only  (a=T)     */
    CHECK(horu_csg_contains(&t,  1.5f, 0.0f, 0.0f) == 1); /* B only  (a=F,b=T) */
    CHECK(horu_csg_contains(&t,  0.5f, 0.0f, 0.0f) == 1); /* both             */
    CHECK(horu_csg_contains(&t,  3.0f, 0.0f, 0.0f) == 0); /* neither (a=F,b=F) */

    /* difference A - B: in A AND NOT in B */
    horu_csg_init(&t);
    la = horu_csg_leaf(&t, &A);
    lb = horu_csg_leaf(&t, &B);
    horu_csg_op(&t, HORU_DIFFERENCE, la, lb);
    CHECK(horu_csg_contains(&t, -0.5f, 0.0f, 0.0f) == 1); /* A not B (a=T,b=F) */
    CHECK(horu_csg_contains(&t,  0.5f, 0.0f, 0.0f) == 0); /* A and B (a=T,b=T) */
    CHECK(horu_csg_contains(&t,  1.5f, 0.0f, 0.0f) == 0); /* B only  (a=F)     */
    CHECK(horu_csg_contains(&t,  3.0f, 0.0f, 0.0f) == 0); /* neither           */

    /* intersection A & B: in A AND in B */
    horu_csg_init(&t);
    la = horu_csg_leaf(&t, &A);
    lb = horu_csg_leaf(&t, &B);
    horu_csg_op(&t, HORU_INTERSECTION, la, lb);
    CHECK(horu_csg_contains(&t,  0.5f, 0.0f, 0.0f) == 1); /* both    (a=T,b=T) */
    CHECK(horu_csg_contains(&t, -0.5f, 0.0f, 0.0f) == 0); /* A not B (a=T,b=F) */
    CHECK(horu_csg_contains(&t,  1.5f, 0.0f, 0.0f) == 0); /* B not A (a=F)     */
    CHECK(horu_csg_contains(&t,  3.0f, 0.0f, 0.0f) == 0); /* neither           */

    /* a lone leaf is a valid root: membership == its solid */
    horu_csg_init(&t);
    horu_csg_leaf(&t, &A);
    CHECK(horu_csg_contains(&t, 0.0f, 0.0f, 0.0f) == 1);
    CHECK(horu_csg_contains(&t, 5.0f, 0.0f, 0.0f) == 0);
}

/* build a polygon from up-to-4 (x,y) verts in the z=0 plane (normal +z) */
static horu_poly quad_z0(float x0, float y0, float x1, float y1,
                         float x2, float y2, float x3, float y3) {
    horu_poly p;
    p.n = 4;
    p.v[0].x = x0; p.v[0].y = y0; p.v[0].z = 0.0f;
    p.v[1].x = x1; p.v[1].y = y1; p.v[1].z = 0.0f;
    p.v[2].x = x2; p.v[2].y = y2; p.v[2].z = 0.0f;
    p.v[3].x = x3; p.v[3].y = y3; p.v[3].z = 0.0f;
    p.plane = horu_plane_make(0.0f, 0.0f, 1.0f, 0.0f); /* z = 0, normal +z */
    return p;
}

static void test_split_spanning(void) {
    /* unit square in z=0, CCW, split by the plane x=0 (normal +x): a front
       piece (x>=0) and a back piece (x<=0), each a quad. */
    horu_poly sq = quad_z0(-1,-1, 1,-1, 1,1, -1,1);
    horu_plane cut = horu_plane_make(1.0f, 0.0f, 0.0f, 0.0f); /* x = 0 */
    horu_poly front[8], back[8];
    int nf = 0, nb = 0, i;
    horu_split_poly(cut, &sq, front, &nf, back, &nb, 8);
    CHECK(nf == 1);
    CHECK(nb == 1);
    CHECK(front[0].n == 4);
    CHECK(back[0].n == 4);
    for (i = 0; i < front[0].n; i++) CHECK(front[0].v[i].x > -HORU_EPS); /* x>=0 */
    for (i = 0; i < back[0].n; i++)  CHECK(back[0].v[i].x <  HORU_EPS);  /* x<=0 */
}

static void test_split_whole_sides(void) {
    horu_poly sq = quad_z0(-1,-1, 1,-1, 1,1, -1,1);
    horu_poly front[8], back[8];
    int nf, nb;
    /* entirely in front of x = -5 */
    nf = nb = 0;
    horu_split_poly(horu_plane_make(1,0,0,-5.0f), &sq, front, &nf, back, &nb, 8);
    CHECK(nf == 1 && nb == 0);
    /* entirely behind x = 5 */
    nf = nb = 0;
    horu_split_poly(horu_plane_make(1,0,0, 5.0f), &sq, front, &nf, back, &nb, 8);
    CHECK(nf == 0 && nb == 1);
}

static void test_split_coplanar(void) {
    /* the square lies in z=0; split by z=0. Same-facing -> front, opposite -> back. */
    horu_poly sq = quad_z0(-1,-1, 1,-1, 1,1, -1,1); /* normal +z */
    horu_poly front[8], back[8];
    int nf, nb;
    nf = nb = 0;
    horu_split_poly(horu_plane_make(0,0, 1, 0.0f), &sq, front, &nf, back, &nb, 8);
    CHECK(nf == 1 && nb == 0); /* same orientation */
    nf = nb = 0;
    horu_split_poly(horu_plane_make(0,0,-1, 0.0f), &sq, front, &nf, back, &nb, 8);
    CHECK(nf == 0 && nb == 1); /* opposite orientation */
}

static void test_flip_poly(void) {
    /* normal +z, verts CCW v0..v3 -> flip -> normal -z, verts reversed v3..v0 */
    horu_poly sq = quad_z0(-1,-1, 1,-1, 1,1, -1,1);
    horu_flip_poly(&sq);
    CHECK(feq(sq.plane.nz, -1.0f));
    CHECK(feq(sq.plane.d, 0.0f));
    CHECK(sq.n == 4);
    CHECK(feq(sq.v[0].x, -1.0f) && feq(sq.v[0].y, 1.0f)); /* old v3 */
    CHECK(feq(sq.v[3].x, -1.0f) && feq(sq.v[3].y, -1.0f)); /* old v0 */
}

static int poly_normal_is(const horu_poly *p, float nx, float ny, float nz) {
    return feq(p->plane.nx, nx) && feq(p->plane.ny, ny) && feq(p->plane.nz, nz);
}

static void test_box_polys(void) {
    horu_poly f[8];
    int n = horu_box_polys(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f, f, 8), i;
    int seen_px = 0, seen_nz = 0;
    CHECK(n == 6);
    for (i = 0; i < n; i++) {
        CHECK(f[i].n == 4);                       /* each face is a quad */
        if (poly_normal_is(&f[i], 1.0f, 0.0f, 0.0f)) {
            seen_px = 1;
            CHECK(feq(f[i].plane.d, 1.0f));        /* +x face at x = 1 */
        }
        if (poly_normal_is(&f[i], 0.0f, 0.0f, -1.0f)) {
            seen_nz = 1;
            CHECK(feq(f[i].plane.d, 1.0f));        /* -z face: -z = 1 -> z = -1 */
        }
    }
    CHECK(seen_px && seen_nz);

    /* every face vertex lies on the box surface (|coord| == 1 on its axis) */
    for (i = 0; i < n; i++) {
        int j;
        for (j = 0; j < f[i].n; j++) {
            CHECK(feq(f[i].v[j].x,  1.0f) || feq(f[i].v[j].x, -1.0f) ||
                  feq(f[i].v[j].y,  1.0f) || feq(f[i].v[j].y, -1.0f) ||
                  feq(f[i].v[j].z,  1.0f) || feq(f[i].v[j].z, -1.0f));
        }
    }
}

static void test_box_polys_capacity(void) {
    horu_poly f[2];
    int n = horu_box_polys(0,0,0, 2,2,2, f, 2);
    CHECK(n == 2); /* stops at capacity */
}

static void test_prism(void) {
    /* square prism (sides=4): n-gon area = (1/2)*4*r^2*sin(90) = 2 r^2;
       r=1,h=2 -> volume 4. positive => outward winding. */
    horu_poly p[256];
    int n = horu_prism_polys(0,0,0, 1.0f, 2.0f, 4, p, 256);
    int i;
    CHECK(n == 12); /* 4 sides * (1 quad + 2 cap tris) */
    CHECK(vol_eq(poly_soup_volume(p, n), 4.0f));
    /* a many-sided prism approximates a cylinder: vol -> pi r^2 h ~ 6.28 */
    n = horu_prism_polys(0,0,0, 1.0f, 2.0f, 64, p, 256); /* 192 polys */
    CHECK(poly_soup_volume(p, n) > 6.0f);
    /* clamp + capacity guards */
    n = horu_prism_polys(0,0,0, 1.0f, 1.0f, 2, p, 256); /* sides<3 -> 3 */
    CHECK(n == 9);
    i = horu_prism_polys(0,0,0, 1,1, 4, p, 2);           /* cap 2 */
    CHECK(i == 2);
}

static void test_cone(void) {
    /* square-base cone (sides=4): base area 2, volume = (1/3)*2*h.
       r=1,h=3 -> 2. positive => outward. */
    horu_poly p[64];
    int n = horu_cone_polys(0,0,0, 1.0f, 3.0f, 4, p, 64);
    CHECK(n == 8); /* 4 sides * (1 side tri + 1 base tri) */
    CHECK(vol_eq(poly_soup_volume(p, n), 2.0f));
    n = horu_cone_polys(0,0,0, 1.0f, 1.0f, 2, p, 64); /* sides<3 -> 3 */
    CHECK(n == 6);
    CHECK(horu_cone_polys(0,0,0, 1,1, 4, p, 1) == 1); /* cap */
}

static void test_sphere(void) {
    horu_poly p[1024];
    int n = horu_sphere_polys(0,0,0, 1.0f, 24, 12, p, 1024);
    float v = poly_soup_volume(p, n);
    CHECK(v > 3.9f && v < 4.25f);   /* ~ 4/3 pi = 4.18, coarse underestimate */
    /* clamp (seg<3, rings<2) + capacity */
    n = horu_sphere_polys(0,0,0, 1.0f, 2, 1, p, 1024); /* -> seg 3, rings 2 */
    CHECK(n == 6);                   /* 3 seg * 2 pole rings, all triangles */
    n = horu_sphere_polys(0,0,0, 1.0f, 24, 12, p, 3);  /* cap 3 */
    CHECK(n == 3);
}

static void test_extrude(void) {
    /* right-triangle profile (0,0),(2,0),(0,2) -- a wedge -- extruded h=2.
       triangle area 2, volume 4. */
    float px[3] = { 0.0f, 2.0f, 0.0f };
    float pz[3] = { 0.0f, 0.0f, 2.0f };
    horu_poly p[16];
    int n = horu_extrude_polys(0,0,0, px, pz, 3, 2.0f, p, 16);
    CHECK(n == 5); /* 3 side quads + 2 cap tris (1 fan tri per cap) */
    CHECK(vol_eq(poly_soup_volume(p, n), 4.0f));
    /* degenerate (npts<3) + capacity */
    CHECK(horu_extrude_polys(0,0,0, px, pz, 2, 2.0f, p, 16) == 0);
    CHECK(horu_extrude_polys(0,0,0, px, pz, 3, 2.0f, p, 2) == 2);
}

static void test_mesh_from_polys(void) {
    horu_poly f[8];
    int npoly = horu_box_polys(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f, f, 8);
    float vx[64], vy[64], vz[64];
    int idx[128], nv = 0, nt, i;
    nt = horu_mesh_from_polys(f, npoly, vx, vy, vz, 64, idx, 128, &nv);
    CHECK(nt == 12); /* 6 quads * (4-2) */
    CHECK(nv == 24); /* 6 quads * 4 verts, no dedup */
    for (i = 0; i < nt * 3; i++) {
        CHECK(idx[i] >= 0 && idx[i] < nv); /* every index in range */
    }
}

static void test_mesh_capacity(void) {
    horu_poly f[8];
    int npoly = horu_box_polys(0,0,0, 2,2,2, f, 8);
    float vx[8], vy[8], vz[8];
    int idx[3], nv = 0, nt;
    /* triangle-index cap 3 -> at most 1 triangle (tri guard) */
    nt = horu_mesh_from_polys(f, npoly, vx, vy, vz, 8, idx, 3, &nv);
    CHECK(nt == 1);
    /* vertex cap 2 -> stops partway through the first quad (vert guard) */
    {
        int idx2[128];
        nv = 0;
        nt = horu_mesh_from_polys(f, npoly, vx, vy, vz, 2, idx2, 128, &nv);
        CHECK(nv == 2);
        CHECK(nt == 0);
    }
}

static void test_split_capacity(void) {
    /* cap 0: lists cannot grow -> the only guard (capacity) drops the piece */
    horu_poly sq = quad_z0(-1,-1, 1,-1, 1,1, -1,1);
    horu_poly front[1], back[1];
    int nf = 0, nb = 0;
    horu_split_poly(horu_plane_make(1,0,0,-5.0f), &sq, front, &nf, back, &nb, 0);
    CHECK(nf == 0 && nb == 0);
}

static void test_csg_union_disjoint(void) {
    horu_poly a[8], b[8], out[64];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int nb = horu_box_polys(5,0,0, 2,2,2, b, 8); /* disjoint */
    int n = horu_csg_polys(HORU_UNION, a, na, b, nb, out, 64, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 16.0f)); /* 8 + 8 */
}

static void test_csg_intersection_self(void) {
    horu_poly a[8], out[64];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int n = horu_csg_polys(HORU_INTERSECTION, a, na, a, na, out, 64, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 8.0f)); /* A ∩ A = A */
}

static void test_csg_difference_self(void) {
    horu_poly a[8], out[64];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int n = horu_csg_polys(HORU_DIFFERENCE, a, na, a, na, out, 64, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 0.0f)); /* A - A = empty */
}

static void test_csg_overlap(void) {
    /* B overlaps A's +++ corner; overlap = [0,1]^3, volume 1 */
    horu_poly a[8], b[8], out[128];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8); /* [-1,1]^3, vol 8 */
    int nb = horu_box_polys(1,1,1, 2,2,2, b, 8); /* [0,2]^3 */
    int n;
    n = horu_csg_polys(HORU_DIFFERENCE, a, na, b, nb, out, 128, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 7.0f)); /* 8 - 1 */
    n = horu_csg_polys(HORU_UNION, a, na, b, nb, out, 128, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 15.0f)); /* 8 + 8 - 1 */
    n = horu_csg_polys(HORU_INTERSECTION, a, na, b, nb, out, 128, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 1.0f)); /* overlap */
}

static void test_csg_empty_operand(void) {
    /* union with an empty solid is identity -> exercises the empty-tree clip */
    horu_poly a[8], out[64];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int n = horu_csg_polys(HORU_UNION, a, na, a, 0, out, 64, SCRATCH);
    CHECK(vol_eq(poly_soup_volume(out, n), 8.0f));
}

static void test_csg_capacity(void) {
    horu_poly a[8], b[8], out[2];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int nb = horu_box_polys(5,0,0, 2,2,2, b, 8);
    int n = horu_csg_polys(HORU_UNION, a, na, b, nb, out, 2, SCRATCH);
    CHECK(n <= 2); /* output clamped to cap */
}

static void test_csg_watertight(void) {
    /* SWEEP a sphere through each box face (as the gizmo drag does) and require
       a watertight result at EVERY position. This is the test that was missing:
       (1) volume alone can't see a hole (a dropped face barely changes it),
       (2) box-box CSG never makes enough coplanar fragments to overflow a node,
       (3) the hole only appears at SOME offsets, so a single static config
       misses it. With the old per-node cap (8) some offset drops faces and the
       summed-normal residual spikes (~0.6); the surface must stay closed. */
    horu_poly a[8], b[64], out[600];
    int ax, i;
    for (ax = 0; ax < 3; ax++) {
        for (i = 0; i < 24; i++) {
            float p = -1.0f + 0.09f * i;
            float cx = (ax == 0) ? p : 1.0f;
            float cy = (ax == 1) ? p : 1.0f;
            float cz = (ax == 2) ? p : 1.0f;
            int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
            int nb = horu_sphere_polys(cx, cy, cz, 1.1f, 8, 4, b, 64);
            int n = horu_csg_polys(HORU_DIFFERENCE, a, na, b, nb, out, 600, SCRATCH);
            CHECK(n > 0);
            CHECK(surface_residual(out, n) < 0.05f); /* closed: no dropped faces */
        }
    }
}

static void test_csg_bounded_scratch(void) {
    /* enough scratch for the two trees but tight for recursion: the arena
       bounds the clip (OOM bail paths) and it still returns without crashing. */
    static char buf[1000000];
    horu_poly a[8], b[64], out[256];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int nb = horu_sphere_polys(0.4f,0.4f,0.4f, 1.1f, 8, 4, b, 64);
    int n = horu_csg_polys(HORU_DIFFERENCE, a, na, b, nb, out, 256,
                           buf, (int)sizeof buf);
    CHECK(n >= 0); /* no crash; result is bounded by the scratch */
}

static void test_csg_tiny_scratch(void) {
    /* far too little scratch: the arena bounds the work and it returns 0
       (or few) without crashing -- exercises the OOM/bail paths. */
    horu_poly a[8], b[8], out[128];
    char tiny[64];
    int na = horu_box_polys(0,0,0, 2,2,2, a, 8);
    int nb = horu_box_polys(1,1,1, 2,2,2, b, 8);
    int n = horu_csg_polys(HORU_DIFFERENCE, a, na, b, nb, out, 128,
                           tiny, (int)sizeof tiny);
    CHECK(n == 0); /* can't even fit the trees */
}

int main(void) {
    test_plane_make_normalizes();
    test_plane_degenerate();
    test_side_classifies();
    test_side_epsilon_boundary();
    test_plane_from_points();
    test_solid_contains();
    test_box();
    test_csg_booleans();
    test_split_spanning();
    test_split_whole_sides();
    test_split_coplanar();
    test_split_capacity();
    test_flip_poly();
    test_box_polys();
    test_box_polys_capacity();
    test_prism();
    test_cone();
    test_sphere();
    test_extrude();
    test_mesh_from_polys();
    test_mesh_capacity();
    test_csg_union_disjoint();
    test_csg_intersection_self();
    test_csg_difference_self();
    test_csg_overlap();
    test_csg_watertight();
    test_csg_empty_operand();
    test_csg_capacity();
    test_csg_bounded_scratch();
    test_csg_tiny_scratch();
    if (g_fail) {
        printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("horu geometry: all checks passed\n");
    return 0;
}
