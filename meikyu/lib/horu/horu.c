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
