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
