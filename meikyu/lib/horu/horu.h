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

/* ---- plane (half-space) ------------------------------------------------- */
/* The set { p : dot(n, p) - d >= 0 } is "inside" (the +normal side is solid).
   n is unit length by construction (horu_plane_make normalizes). */
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
   +1 = in front  (signed distance >  HORU_EPS, the solid/inside side)
   -1 = behind    (signed distance < -HORU_EPS)
    0 = on        (|signed distance| <= HORU_EPS) */
int horu_side(horu_plane p, float x, float y, float z);

/* Plane through three points, normal = normalize(cross(b-a, c-a)) -- so the
   winding a->b->c is counter-clockwise when viewed from the +normal (solid)
   side. This is how a solid's faces become half-spaces. */
horu_plane horu_plane_from_points(float ax, float ay, float az,
                                  float bx, float by, float bz,
                                  float cx, float cy, float cz);

#endif /* HORU_H */
