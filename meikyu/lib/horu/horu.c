#include "horu.h"
#include <math.h>

/* normalize the normal; scale d by the same factor so the described plane
   (the point set dot(n,p) = d) is unchanged. sqrt (not sqrtf) keeps this
   strict-C89. A zero-length normal yields the degenerate zero plane. */
horu_plane horu_plane_make(float nx, float ny, float nz, float d) {
    horu_plane p;
    float len = (float)sqrt((double)(nx * nx + ny * ny + nz * nz));
    if (len > 0.0f) {
        p.nx = nx / len;
        p.ny = ny / len;
        p.nz = nz / len;
        p.d  = d / len;
    } else {
        p.nx = p.ny = p.nz = 0.0f;
        p.d = 0.0f;
    }
    return p;
}

/* signed distance of (x,y,z) from the plane, bucketed by HORU_EPS. */
int horu_side(horu_plane p, float x, float y, float z) {
    float dist = p.nx * x + p.ny * y + p.nz * z - p.d;
    if (dist >  HORU_EPS) return 1;
    if (dist < -HORU_EPS) return -1;
    return 0;
}

horu_plane horu_plane_from_points(float ax, float ay, float az,
                                  float bx, float by, float bz,
                                  float cx, float cy, float cz) {
    float ux = bx - ax, uy = by - ay, uz = bz - az; /* edge a->b */
    float vx = cx - ax, vy = cy - ay, vz = cz - az; /* edge a->c */
    float nx = uy * vz - uz * vy;                   /* n = u x v */
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float d = nx * ax + ny * ay + nz * az;          /* plane through a */
    return horu_plane_make(nx, ny, nz, d);          /* normalizes n and d */
}

/* inside-or-on = strictly in front of no face (faces have outward normals) */
int horu_contains(const horu_solid *s, float x, float y, float z) {
    int i;
    for (i = 0; i < s->count; i++) {
        if (horu_side(s->planes[i], x, y, z) > 0) {
            return 0;
        }
    }
    return 1;
}

horu_solid horu_box(float cx, float cy, float cz,
                    float sx, float sy, float sz) {
    horu_solid s;
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    s.count = 0;
    /* six faces, outward normals; the -axis faces' d is negated to match */
    s.planes[s.count++] = horu_plane_make( 1.0f, 0.0f, 0.0f,  cx + hx);
    s.planes[s.count++] = horu_plane_make(-1.0f, 0.0f, 0.0f, -(cx - hx));
    s.planes[s.count++] = horu_plane_make( 0.0f, 1.0f, 0.0f,  cy + hy);
    s.planes[s.count++] = horu_plane_make( 0.0f,-1.0f, 0.0f, -(cy - hy));
    s.planes[s.count++] = horu_plane_make( 0.0f, 0.0f, 1.0f,  cz + hz);
    s.planes[s.count++] = horu_plane_make( 0.0f, 0.0f,-1.0f, -(cz - hz));
    return s;
}

/* ---- CSG tree ----------------------------------------------------------- */

void horu_csg_init(horu_csg *t) {
    t->solid_count = 0;
    t->node_count = 0;
    t->root = 0;
}

int horu_csg_leaf(horu_csg *t, const horu_solid *s) {
    int si = t->solid_count++;
    int ni = t->node_count++;
    t->solids[si] = *s;
    t->nodes[ni].op = HORU_LEAF;
    t->nodes[ni].a = si;
    t->nodes[ni].b = 0;
    t->root = ni;
    return ni;
}

int horu_csg_op(horu_csg *t, horu_op op, int a, int b) {
    int ni = t->node_count++;
    t->nodes[ni].op = op;
    t->nodes[ni].a = a;
    t->nodes[ni].b = b;
    t->root = ni;
    return ni;
}

/* recursive point membership: leaves test their convex solid, ops combine.
   if-chain (not switch) so adding an op never trips -Wswitch, and the last
   op needs no catch-all branch to leave uncovered. */
static int csg_node_in(const horu_csg *t, int n, float x, float y, float z) {
    const horu_csg_node *nd = &t->nodes[n];
    if (nd->op == HORU_LEAF) {
        return horu_contains(&t->solids[nd->a], x, y, z);
    }
    if (nd->op == HORU_UNION) {
        return csg_node_in(t, nd->a, x, y, z) ||
               csg_node_in(t, nd->b, x, y, z);
    }
    if (nd->op == HORU_DIFFERENCE) {
        return csg_node_in(t, nd->a, x, y, z) &&
               !csg_node_in(t, nd->b, x, y, z);
    }
    /* HORU_INTERSECTION */
    return csg_node_in(t, nd->a, x, y, z) &&
           csg_node_in(t, nd->b, x, y, z);
}

int horu_csg_contains(const horu_csg *t, float x, float y, float z) {
    return csg_node_in(t, t->root, x, y, z);
}

/* ---- PART 2: polygon split ---------------------------------------------- */

enum { HORU_COPLANAR = 0, HORU_FRONT = 1, HORU_BACK = 2, HORU_SPANNING = 3 };

static horu_v3 v3_lerp(horu_v3 a, horu_v3 b, float t) {
    horu_v3 r;
    r.x = a.x + (b.x - a.x) * t;
    r.y = a.y + (b.y - a.y) * t;
    r.z = a.z + (b.z - a.z) * t;
    return r;
}

/* append a polygon (verts + supporting plane) to a list, unless at capacity.
   The split logic guarantees 3 <= n <= HORU_POLY_MAX by the convex invariant,
   so capacity is the only guard. */
static void poly_push(horu_poly *list, int *count, int cap,
                      const horu_v3 *verts, int n, horu_plane pl) {
    horu_poly *p;
    int i;
    if (*count >= cap) {
        return;
    }
    if (n > HORU_POLY_MAX) {
        n = HORU_POLY_MAX; /* clip never grows a piece past the vertex cap */
    }
    p = &list[(*count)++];
    p->n = n;
    for (i = 0; i < n; i++) {
        p->v[i] = verts[i];
    }
    p->plane = pl;
}

void horu_flip_poly(horu_poly *p) {
    int i, n = p->n;
    horu_v3 tmp;
    p->plane.nx = -p->plane.nx;
    p->plane.ny = -p->plane.ny;
    p->plane.nz = -p->plane.nz;
    p->plane.d  = -p->plane.d;
    for (i = 0; i < n / 2; i++) {
        tmp = p->v[i];
        p->v[i] = p->v[n - 1 - i];
        p->v[n - 1 - i] = tmp;
    }
}

int horu_mesh_from_polys(const horu_poly *polys, int npoly,
                         float *vx, float *vy, float *vz, int vcap,
                         int *idx, int icap, int *out_nverts) {
    int vn = 0, tn = 0, i, j, k, stop = 0;
    for (i = 0; i < npoly && !stop; i++) {
        int base = vn, m = polys[i].n;
        for (j = 0; j < m; j++) {
            if (vn >= vcap) { stop = 1; break; }
            vx[vn] = polys[i].v[j].x;
            vy[vn] = polys[i].v[j].y;
            vz[vn] = polys[i].v[j].z;
            vn++;
        }
        if (stop) {
            break;
        }
        for (k = 0; k + 2 < m; k++) {  /* fan from v0 */
            if (tn * 3 + 3 > icap) { stop = 1; break; }
            idx[tn * 3 + 0] = base;
            idx[tn * 3 + 1] = base + k + 1;
            idx[tn * 3 + 2] = base + k + 2;
            tn++;
        }
    }
    *out_nverts = vn;
    return tn;
}

/* ---- PART 2: BSP boolean (exact CSG) ------------------------------------ */

/* coplanar polys held at one node. Must hold the CLIPPED result of a node's
   faces, which splits into more pieces than were built (measured up to ~12 for
   box-minus-sphere); too small drops polygons and punches moving holes in the
   surface. 32 gives margin. (A dynamic per-node pool would remove the cap
   entirely -- the robust follow-up.) */
#define HORU_NODE_POLYS 32
#define HORU_BSP_NODES  256   /* node pool per tree */
#define HORU_SPLIT_CAP  64    /* per-recursion split scratch */

typedef struct {
    horu_plane plane;
    int        valid;             /* plane assigned yet */
    int        front, back;       /* child node index, or -1 */
    horu_poly  cop[HORU_NODE_POLYS];
    int        ncop;              /* coplanar polygons at this node */
} horu_node;

typedef struct {
    horu_node n[HORU_BSP_NODES];
    int       count;
} horu_bsp;

/* All working memory comes from the CALLER'S scratch (a slice of the engine's
   reload/transient block), via this bump arena -- horu keeps NOTHING on the
   stack or in statics, so it is reentrant and a deep BSP can never blow the
   stack. Per-recursion scratch is pushed/popped LIFO; running out of arena
   bounds the recursion (it bails gracefully, dropping nothing it cannot fit).
   The caller sizes the scratch to the CSG complexity it needs. */
typedef struct { char *base; long cap, used; } horu_arena;

static void *ha_push(horu_arena *ar, long bytes) {
    long n = (bytes + 15L) & ~15L;
    void *p;
    if (ar->used + n > ar->cap) {
        return 0;
    }
    p = ar->base + ar->used;
    ar->used += n;
    return p;
}

#define HA_POLYS (long)(sizeof(horu_poly) * HORU_SPLIT_CAP)

static int bsp_alloc(horu_bsp *t) {
    int i;
    if (t->count >= HORU_BSP_NODES) {
        return -1; /* node pool full -- the model is too complex */
    }
    i = t->count++;
    t->n[i].valid = 0;
    t->n[i].front = -1;
    t->n[i].back = -1;
    t->n[i].ncop = 0;
    return i;
}

static int poly_coplanar(horu_plane pl, const horu_poly *p) {
    int i;
    for (i = 0; i < p->n; i++) {
        if (horu_side(pl, p->v[i].x, p->v[i].y, p->v[i].z) != 0) {
            return 0;
        }
    }
    return 1;
}

/* build (extend) the tree rooted at idx with `polys`. Coplanar polygons stay
   at the node; the rest split front/back and recurse. */
static void bsp_build(horu_bsp *t, int idx, const horu_poly *polys, int n,
                      horu_arena *ar) {
    horu_node *nd = &t->n[idx];
    long mark = ar->used;
    horu_poly *front = (horu_poly *)ha_push(ar, HA_POLYS);
    horu_poly *back  = (horu_poly *)ha_push(ar, HA_POLYS);
    int nf = 0, nb = 0, i;
    if (n == 0 || !front || !back) { /* empty, or out of scratch -> bail */
        ar->used = mark;
        return;
    }
    if (!nd->valid) {
        nd->plane = polys[0].plane;
        nd->valid = 1;
    }
    for (i = 0; i < n; i++) {
        if (poly_coplanar(nd->plane, &polys[i])) {
            if (nd->ncop < HORU_NODE_POLYS) {
                nd->cop[nd->ncop++] = polys[i];
            }
        } else {
            horu_split_poly(nd->plane, &polys[i], front, &nf, back, &nb,
                            HORU_SPLIT_CAP);
        }
    }
    if (nf) {
        if (nd->front < 0) nd->front = bsp_alloc(t);
        if (nd->front >= 0) bsp_build(t, nd->front, front, nf, ar);
    }
    if (nb) {
        if (nd->back < 0) nd->back = bsp_alloc(t);
        if (nd->back >= 0) bsp_build(t, nd->back, back, nb, ar);
    }
    ar->used = mark;
}

/* gather every polygon in the tree into out[] (capacity cap); returns count. */
static int bsp_all(const horu_bsp *t, int idx, horu_poly *out, int n, int cap) {
    const horu_node *nd;
    int i;
    if (idx < 0) {
        return n;
    }
    nd = &t->n[idx];
    for (i = 0; i < nd->ncop; i++) {
        if (n < cap) out[n++] = nd->cop[i];
    }
    n = bsp_all(t, nd->front, out, n, cap);
    n = bsp_all(t, nd->back, out, n, cap);
    return n;
}

/* flip every polygon + plane, swap front/back: inverts the solid sense. */
static void bsp_invert(horu_bsp *t, int idx) {
    horu_node *nd;
    int i, tmp;
    if (idx < 0) {
        return;
    }
    nd = &t->n[idx];
    for (i = 0; i < nd->ncop; i++) {
        horu_flip_poly(&nd->cop[i]);
    }
    nd->plane.nx = -nd->plane.nx;
    nd->plane.ny = -nd->plane.ny;
    nd->plane.nz = -nd->plane.nz;
    nd->plane.d  = -nd->plane.d;
    bsp_invert(t, nd->front);
    bsp_invert(t, nd->back);
    tmp = nd->front; nd->front = nd->back; nd->back = tmp;
}

/* return the parts of `in` that lie OUTSIDE the tree (front of every leaf;
   the back of a leaf is inside, dropped). */
static int bsp_clip_polys(const horu_bsp *t, int idx, const horu_poly *in,
                          int n, horu_poly *out, int cap, horu_arena *ar) {
    const horu_node *nd;
    long mark = ar->used;
    horu_poly *front = (horu_poly *)ha_push(ar, HA_POLYS);
    horu_poly *back  = (horu_poly *)ha_push(ar, HA_POLYS);
    int nf = 0, nb = 0, i, on = 0;
    if (!front || !back || !t->n[idx].valid) {
        /* out of scratch, or an empty tree: pass the polygons through */
        ar->used = mark;
        for (i = 0; i < n; i++) {
            if (on < cap) out[on++] = in[i];
        }
        return on;
    }
    nd = &t->n[idx];
    for (i = 0; i < n; i++) {
        horu_split_poly(nd->plane, &in[i], front, &nf, back, &nb, HORU_SPLIT_CAP);
    }
    if (nd->front >= 0) {
        horu_poly *fout = (horu_poly *)ha_push(ar, HA_POLYS);
        int fc = fout ? bsp_clip_polys(t, nd->front, front, nf, fout,
                                       HORU_SPLIT_CAP, ar) : 0;
        for (i = 0; i < fc; i++) if (on < cap) out[on++] = fout[i];
    } else {
        for (i = 0; i < nf; i++) if (on < cap) out[on++] = front[i];
    }
    if (nd->back >= 0) {
        horu_poly *bout = (horu_poly *)ha_push(ar, HA_POLYS);
        int bc = bout ? bsp_clip_polys(t, nd->back, back, nb, bout,
                                       HORU_SPLIT_CAP, ar) : 0;
        for (i = 0; i < bc; i++) if (on < cap) out[on++] = bout[i];
    }
    /* no back child: those polygons are inside, dropped */
    ar->used = mark;
    return on;
}

/* clip every node's polygons in `target` against `clipper`. */
static void bsp_clip_to(horu_bsp *target, int idx, const horu_bsp *clipper,
                        horu_arena *ar) {
    horu_node *nd;
    long mark = ar->used;
    horu_poly *tmp = (horu_poly *)ha_push(ar, HA_POLYS);
    int tn, i;
    if (idx < 0 || !tmp) {
        ar->used = mark;
        return;
    }
    nd = &target->n[idx];
    tn = bsp_clip_polys(clipper, 0, nd->cop, nd->ncop, tmp, HORU_SPLIT_CAP, ar);
    if (tn > HORU_NODE_POLYS) tn = HORU_NODE_POLYS;
    nd->ncop = tn;
    for (i = 0; i < tn; i++) {
        nd->cop[i] = tmp[i];
    }
    bsp_clip_to(target, nd->front, clipper, ar);
    bsp_clip_to(target, nd->back, clipper, ar);
    ar->used = mark;
}

int horu_csg_polys(horu_op op, const horu_poly *a, int na,
                   const horu_poly *b, int nb, horu_poly *out, int cap,
                   void *scratch, int scratch_bytes) {
    horu_arena ar;
    horu_bsp *A, *B;
    horu_poly *gather;
    int gcap = HORU_SPLIT_CAP * HORU_BSP_NODES / 4;
    int m;

    ar.base = (char *)scratch;
    ar.cap = (long)scratch_bytes;
    ar.used = 0;
    A = (horu_bsp *)ha_push(&ar, (long)sizeof(horu_bsp));
    B = (horu_bsp *)ha_push(&ar, (long)sizeof(horu_bsp));
    gather = (horu_poly *)ha_push(&ar, (long)sizeof(horu_poly) * gcap);
    if (!A || !B || !gather) {
        return 0; /* scratch too small for even the two trees */
    }
    A->count = 0; bsp_alloc(A); bsp_build(A, 0, a, na, &ar);
    B->count = 0; bsp_alloc(B); bsp_build(B, 0, b, nb, &ar);

    if (op == HORU_UNION) {
        bsp_clip_to(A, 0, B, &ar);
        bsp_clip_to(B, 0, A, &ar);
        bsp_invert(B, 0);
        bsp_clip_to(B, 0, A, &ar);
        bsp_invert(B, 0);
        m = bsp_all(B, 0, gather, 0, gcap);
        bsp_build(A, 0, gather, m, &ar);
    } else if (op == HORU_INTERSECTION) {
        bsp_invert(A, 0);
        bsp_clip_to(B, 0, A, &ar);
        bsp_invert(B, 0);
        bsp_clip_to(A, 0, B, &ar);
        bsp_clip_to(B, 0, A, &ar);
        m = bsp_all(B, 0, gather, 0, gcap);
        bsp_build(A, 0, gather, m, &ar);
        bsp_invert(A, 0);
    } else { /* HORU_DIFFERENCE: a - b */
        bsp_invert(A, 0);
        bsp_clip_to(A, 0, B, &ar);
        bsp_clip_to(B, 0, A, &ar);
        bsp_invert(B, 0);
        bsp_clip_to(B, 0, A, &ar);
        bsp_invert(B, 0);
        m = bsp_all(B, 0, gather, 0, gcap);
        bsp_build(A, 0, gather, m, &ar);
        bsp_invert(A, 0);
    }
    return bsp_all(A, 0, out, 0, cap);
}

/* ---- PART 2: curved/faceted primitives ---------------------------------- */

#define HORU_PI 3.14159265358979323846

static horu_v3 hv3(float x, float y, float z) {
    horu_v3 v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

static void emit3(horu_poly *p, horu_v3 a, horu_v3 b, horu_v3 c) {
    p->n = 3;
    p->v[0] = a; p->v[1] = b; p->v[2] = c;
    p->plane = horu_plane_from_points(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
}

static void emit4(horu_poly *p, horu_v3 a, horu_v3 b, horu_v3 c, horu_v3 d) {
    p->n = 4;
    p->v[0] = a; p->v[1] = b; p->v[2] = c; p->v[3] = d;
    p->plane = horu_plane_from_points(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
}

/* A regular n-gon prism (a cylinder for large `sides`, a polygon column for
   small): `sides` side quads + two triangle-fan caps. Outward normals. */
int horu_prism_polys(float cx, float cy, float cz, float r, float h,
                     int sides, horu_poly *out, int cap) {
    int n = 0, i;
    float hy = h * 0.5f, y0 = cy - hy, y1 = cy + hy;
    horu_v3 cbot = hv3(cx, y0, cz), ctop = hv3(cx, y1, cz);
    if (sides < 3) {
        sides = 3;
    }
    for (i = 0; i < sides; i++) {
        float a0 = (float)(2.0 * HORU_PI * i / sides);
        float a1 = (float)(2.0 * HORU_PI * (i + 1) / sides);
        float c0 = (float)cos(a0), s0 = (float)sin(a0);
        float c1 = (float)cos(a1), s1 = (float)sin(a1);
        horu_v3 b0 = hv3(cx + r * c0, y0, cz + r * s0);
        horu_v3 b1 = hv3(cx + r * c1, y0, cz + r * s1);
        horu_v3 t0 = hv3(cx + r * c0, y1, cz + r * s0);
        horu_v3 t1 = hv3(cx + r * c1, y1, cz + r * s1);
        if (n < cap) emit4(&out[n++], b0, t0, t1, b1);  /* side, outward */
        if (n < cap) emit3(&out[n++], cbot, b0, b1);    /* bottom cap, -y */
        if (n < cap) emit3(&out[n++], ctop, t1, t0);    /* top cap, +y */
    }
    return n;
}

/* Extrude a 2D outline (points (px[i],pz[i]) in the XZ plane, CCW seen from
   +y, convex) along Y by height h about (cx,cy,cz): side quads + two fan caps.
   This is the "polygon" primitive -- a triangle profile gives a wedge/ramp, a
   trapezoid a slope, etc. Outward normals; returns the polygon count. */
int horu_extrude_polys(float cx, float cy, float cz,
                       const float *px, const float *pz, int npts, float h,
                       horu_poly *out, int cap) {
    int n = 0, i;
    float hy = h * 0.5f, y0 = cy - hy, y1 = cy + hy;
    if (npts < 3) {
        return 0;
    }
    for (i = 0; i < npts; i++) {                 /* side walls */
        int j = (i + 1) % npts;
        horu_v3 b0 = hv3(cx + px[i], y0, cz + pz[i]);
        horu_v3 b1 = hv3(cx + px[j], y0, cz + pz[j]);
        horu_v3 t0 = hv3(cx + px[i], y1, cz + pz[i]);
        horu_v3 t1 = hv3(cx + px[j], y1, cz + pz[j]);
        if (n < cap) emit4(&out[n++], b0, t0, t1, b1);
    }
    for (i = 1; i + 1 < npts; i++) {             /* fan caps from point 0 */
        horu_v3 a0 = hv3(cx + px[0], y0, cz + pz[0]);
        horu_v3 b0 = hv3(cx + px[i], y0, cz + pz[i]);
        horu_v3 c0 = hv3(cx + px[i + 1], y0, cz + pz[i + 1]);
        horu_v3 a1 = hv3(cx + px[0], y1, cz + pz[0]);
        horu_v3 b1 = hv3(cx + px[i], y1, cz + pz[i]);
        horu_v3 c1 = hv3(cx + px[i + 1], y1, cz + pz[i + 1]);
        if (n < cap) emit3(&out[n++], a0, b0, c0); /* bottom, -y */
        if (n < cap) emit3(&out[n++], a1, c1, b1); /* top, +y */
    }
    return n;
}

/* A cone: `sides` base edges fanning to an apex, plus a base cap. radius r,
   height h (apex at +h/2, base at -h/2). Outward normals. Used for arrow tips. */
int horu_cone_polys(float cx, float cy, float cz, float r, float h,
                    int sides, horu_poly *out, int cap) {
    int n = 0, i;
    float hy = h * 0.5f, y0 = cy - hy, y1 = cy + hy;
    horu_v3 cbot = hv3(cx, y0, cz), apex = hv3(cx, y1, cz);
    if (sides < 3) {
        sides = 3;
    }
    for (i = 0; i < sides; i++) {
        float a0 = (float)(2.0 * HORU_PI * i / sides);
        float a1 = (float)(2.0 * HORU_PI * (i + 1) / sides);
        horu_v3 b0 = hv3(cx + r * (float)cos(a0), y0, cz + r * (float)sin(a0));
        horu_v3 b1 = hv3(cx + r * (float)cos(a1), y0, cz + r * (float)sin(a1));
        if (n < cap) emit3(&out[n++], b0, apex, b1); /* side, outward */
        if (n < cap) emit3(&out[n++], cbot, b0, b1); /* base, -y */
    }
    return n;
}

static horu_v3 sphere_pt(float cx, float cy, float cz, float r,
                         float polar, float azim) {
    return hv3(cx + r * (float)(sin(polar) * cos(azim)),
               cy + r * (float)cos(polar),
               cz + r * (float)(sin(polar) * sin(azim)));
}

/* A UV sphere: `seg` longitude * `rings` latitude. Pole rings are triangles,
   the rest quads. Outward normals. */
int horu_sphere_polys(float cx, float cy, float cz, float r,
                      int seg, int rings, horu_poly *out, int cap) {
    int n = 0, i, j;
    if (seg < 3) {
        seg = 3;
    }
    if (rings < 2) {
        rings = 2;
    }
    for (j = 0; j < rings; j++) {
        float p0 = (float)(HORU_PI * j / rings);
        float p1 = (float)(HORU_PI * (j + 1) / rings);
        for (i = 0; i < seg; i++) {
            float t0 = (float)(2.0 * HORU_PI * i / seg);
            float t1 = (float)(2.0 * HORU_PI * (i + 1) / seg);
            horu_v3 a = sphere_pt(cx, cy, cz, r, p0, t0);
            horu_v3 b = sphere_pt(cx, cy, cz, r, p1, t0);
            horu_v3 c = sphere_pt(cx, cy, cz, r, p1, t1);
            horu_v3 d = sphere_pt(cx, cy, cz, r, p0, t1);
            if (j == 0) {                 /* top pole: a == d */
                if (n < cap) emit3(&out[n++], a, c, b);
            } else if (j == rings - 1) {  /* bottom pole: b == c */
                if (n < cap) emit3(&out[n++], a, d, b);
            } else {
                if (n < cap) emit4(&out[n++], a, d, c, b);
            }
        }
    }
    return n;
}

int horu_box_polys(float cx, float cy, float cz,
                   float sx, float sy, float sz,
                   horu_poly *out, int cap) {
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    float x0 = cx - hx, x1 = cx + hx;
    float y0 = cy - hy, y1 = cy + hy;
    float z0 = cz - hz, z1 = cz + hz;
    horu_v3 c[8];
    /* faces wound CCW seen from outside, so plane_from_points -> outward normal */
    static const int faces[6][4] = {
        {0, 3, 2, 1}, /* -z */ {4, 5, 6, 7}, /* +z */
        {0, 4, 7, 3}, /* -x */ {1, 2, 6, 5}, /* +x */
        {0, 1, 5, 4}, /* -y */ {3, 7, 6, 2}  /* +y */
    };
    int i, j, n = 0;

    c[0].x = x0; c[0].y = y0; c[0].z = z0;
    c[1].x = x1; c[1].y = y0; c[1].z = z0;
    c[2].x = x1; c[2].y = y1; c[2].z = z0;
    c[3].x = x0; c[3].y = y1; c[3].z = z0;
    c[4].x = x0; c[4].y = y0; c[4].z = z1;
    c[5].x = x1; c[5].y = y0; c[5].z = z1;
    c[6].x = x1; c[6].y = y1; c[6].z = z1;
    c[7].x = x0; c[7].y = y1; c[7].z = z1;

    for (i = 0; i < 6; i++) {
        horu_poly *p;
        if (n >= cap) {
            break;
        }
        p = &out[n++];
        p->n = 4;
        for (j = 0; j < 4; j++) {
            p->v[j] = c[faces[i][j]];
        }
        p->plane = horu_plane_from_points(p->v[0].x, p->v[0].y, p->v[0].z,
                                          p->v[1].x, p->v[1].y, p->v[1].z,
                                          p->v[2].x, p->v[2].y, p->v[2].z);
    }
    return n;
}

void horu_split_poly(horu_plane plane, const horu_poly *poly,
                     horu_poly *front, int *nf,
                     horu_poly *back, int *nb, int cap) {
    int types[HORU_POLY_MAX];
    int polyType = 0;
    int i, n = poly->n;

    for (i = 0; i < n; i++) {
        int s = horu_side(plane, poly->v[i].x, poly->v[i].y, poly->v[i].z);
        int t = (s > 0) ? HORU_FRONT : (s < 0) ? HORU_BACK : HORU_COPLANAR;
        types[i] = t;
        polyType |= t;
    }

    if (polyType == HORU_COPLANAR) {
        /* same-facing as the split plane -> front, opposite -> back */
        float d = plane.nx * poly->plane.nx + plane.ny * poly->plane.ny +
                  plane.nz * poly->plane.nz;
        if (d > 0.0f) {
            poly_push(front, nf, cap, poly->v, n, poly->plane);
        } else {
            poly_push(back, nb, cap, poly->v, n, poly->plane);
        }
    } else if (polyType == HORU_FRONT) {
        poly_push(front, nf, cap, poly->v, n, poly->plane);
    } else if (polyType == HORU_BACK) {
        poly_push(back, nb, cap, poly->v, n, poly->plane);
    } else { /* SPANNING: cut into a front piece and a back piece */
        horu_v3 f[HORU_POLY_MAX * 2], b[HORU_POLY_MAX * 2];
        int fn = 0, bn = 0;
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int ti = types[i], tj = types[j];
            if (ti != HORU_BACK)  f[fn++] = poly->v[i];
            if (ti != HORU_FRONT) b[bn++] = poly->v[i];
            if ((ti | tj) == HORU_SPANNING) {
                horu_v3 vi = poly->v[i], vj = poly->v[j], vc;
                float di = plane.nx * vi.x + plane.ny * vi.y + plane.nz * vi.z - plane.d;
                float dj = plane.nx * vj.x + plane.ny * vj.y + plane.nz * vj.z - plane.d;
                vc = v3_lerp(vi, vj, di / (di - dj));
                f[fn++] = vc;
                b[bn++] = vc;
            }
        }
        poly_push(front, nf, cap, f, fn, poly->plane);
        poly_push(back, nb, cap, b, bn, poly->plane);
    }
}
