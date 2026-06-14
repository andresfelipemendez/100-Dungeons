#ifndef HORU_H
#define HORU_H

/* horu (彫): constructive solid geometry, two parts.

   PART 1 -- this file: the abstract boolean. Solids are represented by PLANES
   (half-spaces), never triangles. Boolean ops (union / difference /
   intersection) are pure plane + tree operations. Data-oriented: planes are a
   value type of four floats; the solid is a flat array of BSP nodes carrying
   int child indices (relocatable across realloc and dll reload -- no pointers
   in the hot arrays).

   PART 2 -- horu_tri.h: triangulation. Walks an abstract solid and emits a
   structure-of-arrays triangle mesh for the renderer. One-way, downstream.

   Strict C89. */

/* ---- plane (oriented) --------------------------------------------------- */
/* An oriented plane dot(n, p) = d; n is unit length (horu_plane_make
   normalizes). The plane carries no notion of "solid side" -- that is a
   solid-level convention (see horu_solid) -- so the same primitive serves both
   a face half-space and a BSP splitting plane. */
typedef struct {
    float nx, ny, nz; /* unit normal */
    float d;          /* signed distance of the plane from the origin */
} horu_plane;

/* Distance below which a point counts as lying ON the plane. */
#define HORU_EPS 1e-5f

/* Build a plane from a (not necessarily unit) normal and offset d.
   Normal is normalized; d is scaled to match so the plane is unchanged. */
horu_plane horu_plane_make(float nx, float ny, float nz, float d);

/* Classify a point against a plane:
   +1 = in front  (signed distance >  HORU_EPS)
   -1 = behind    (signed distance < -HORU_EPS)
    0 = on        (|signed distance| <= HORU_EPS) */
int horu_side(horu_plane p, float x, float y, float z);

/* Plane through three points, normal = normalize(cross(b-a, c-a)) -- so the
   winding a->b->c is counter-clockwise when viewed from the +normal (solid)
   side. This is how a solid's faces become half-spaces. */
horu_plane horu_plane_from_points(float ax, float ay, float az,
                                  float bx, float by, float bz,
                                  float cx, float cy, float cz);

/* ---- convex solid ------------------------------------------------------- */
/* A convex solid is the intersection of its faces' half-spaces. Faces carry
   OUTWARD normals, so a point is INSIDE when it is behind-or-on every face
   (never strictly in front of one). Data-oriented: a flat plane array, no
   tree, no vertices -- containment streams the planes in a loop. Non-convex
   results (union/difference) are built later as plane-partition trees. */
#define HORU_MAX_PLANES 64

typedef struct {
    horu_plane planes[HORU_MAX_PLANES];
    int count;
} horu_solid;

/* 1 if (x,y,z) is inside-or-on the convex solid (behind every face), else 0. */
int horu_contains(const horu_solid *s, float x, float y, float z);

/* ---- primitives --------------------------------------------------------- */
/* Axis-aligned box centred at (cx,cy,cz) with full extents (sx,sy,sz): six
   faces with outward normals. */
horu_solid horu_box(float cx, float cy, float cz,
                    float sx, float sy, float sz);

/* ---- CSG tree (the boolean abstract) ------------------------------------ */
/* A general solid is a tree: leaves are convex solids, internal nodes are
   boolean ops over child nodes. This is the abstract boolean layer -- closed
   under all ops (just add a node), evaluated by a recursive point-membership
   walk; triangulation (horu_tri.h, part 2) clips faces against it. Data-
   oriented: flat node + solid pools addressed by int index, no pointers.

   Build bottom-up; every builder call sets the root to the node it returns, so
   the last call is the tree root. */
typedef enum {
    HORU_LEAF, HORU_UNION, HORU_DIFFERENCE, HORU_INTERSECTION
} horu_op;

#define HORU_CSG_MAX_NODES  64
#define HORU_CSG_MAX_LEAVES 16

typedef struct {
    horu_op op;
    int a;   /* leaf: solid index. op: first child node index.  */
    int b;   /* op: second child node index. unused for a leaf. */
} horu_csg_node;

typedef struct {
    horu_solid    solids[HORU_CSG_MAX_LEAVES];  int solid_count;
    horu_csg_node nodes[HORU_CSG_MAX_NODES];    int node_count;
    int root;
} horu_csg;

void horu_csg_init(horu_csg *t);

/* Add a leaf wrapping a copy of `s`; returns its node index (also the root). */
int  horu_csg_leaf(horu_csg *t, const horu_solid *s);

/* Add an op over child node indices a, b; returns its node index (the root). */
int  horu_csg_op(horu_csg *t, horu_op op, int a, int b);

/* 1 if (x,y,z) is inside the solid described by the tree's root, else 0. */
int  horu_csg_contains(const horu_csg *t, float x, float y, float z);

/* ---- PART 2: triangulation (exact CSG via BSP polygon clipping) ---------- */
/* The boolean abstract above answers point membership. To produce renderable
   geometry, solids are carried as POLYGONS (convex, CCW, outward normal) that
   are split and clipped against BSP trees -- the classic Naylor/csg.js
   algorithm. The atom is horu_split_poly. */

typedef struct { float x, y, z; } horu_v3;

#define HORU_POLY_MAX 8   /* verts per polygon; convex splits stay small */

typedef struct {
    horu_v3    v[HORU_POLY_MAX];
    int        n;       /* vertex count (>= 3) */
    horu_plane plane;   /* supporting plane, outward normal */
} horu_poly;

/* Split `poly` by `plane`, appending the resulting pieces to the front and/or
   back lists (each a caller array of capacity `cap` with its count by ref):
     - wholly in front / behind -> the polygon is appended unchanged
     - coplanar -> appended to front if it faces the same way as `plane`, else back
     - spanning -> cut along the plane into a front piece and a back piece
   Pieces keep `poly`'s supporting plane. A list at capacity silently drops. */
void horu_split_poly(horu_plane plane, const horu_poly *poly,
                     horu_poly *front, int *nf,
                     horu_poly *back, int *nb, int cap);

#endif /* HORU_H */
