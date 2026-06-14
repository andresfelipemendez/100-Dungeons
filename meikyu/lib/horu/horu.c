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
    p = &list[(*count)++];
    p->n = n;
    for (i = 0; i < n; i++) {
        p->v[i] = verts[i];
    }
    p->plane = pl;
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
