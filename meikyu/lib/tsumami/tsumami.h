#ifndef TSUMAMI_H
#define TSUMAMI_H

/* tsumami (摘み): 3D manipulation handles -- ray-picking and drag math for
   moving objects in a viewport. PURE: no UI, no GPU, no engine headers, its
   own vec3 -- so it is unit-testable headless and reusable for any editor
   (CSG nodes today, mesh vertices/objects later).

   The consumer feeds, each frame, world-space pick targets + a pick ray +
   the mouse-down state; tsumami returns selection and drag deltas via a small
   state machine. It NEVER renders -- drawing the handles is the consumer's.

   Strict C89. */

typedef struct { float x, y, z; } tsu_v3;
typedef struct { tsu_v3 o, d; } tsu_ray;   /* origin + unit direction */

/* a pickable target. `center`/`half` are the pick AABB (e.g. an arrow's box);
   `origin` is the object's center -- the drag reference (an axis arrow's box is
   offset from the object it moves, so the two differ; for a free body handle
   set origin == center). `axis` constrains the drag: -1 = free (view plane),
   0/1/2 = lock to world X/Y/Z. */
typedef struct { int id; tsu_v3 center, half, origin; int axis; } tsu_target;

/* ---- pure geometry ------------------------------------------------------ */

/* Build a pick ray from the camera basis (eye + unit forward/right/up), the
   half-fov tangent, aspect, and a pixel (mx,my) in a w*h viewport (y down,
   origin top-left). No matrix inverse needed. */
tsu_ray tsu_ray_from_screen(tsu_v3 eye, tsu_v3 fwd, tsu_v3 right, tsu_v3 up,
                            float tan_half_fov, float aspect,
                            float mx, float my, float w, float h);

/* Ray vs AABB [mn,mx]: entry distance t >= 0, or -1 if the ray misses. */
float tsu_ray_aabb(tsu_ray ray, tsu_v3 mn, tsu_v3 mx);

/* Nearest target the ray hits; returns its id, or -1 if none. */
int tsu_pick(tsu_ray ray, const tsu_target *t, int n);

/* Ray vs plane (point p, unit normal nrm): the hit point. The caller ensures
   the ray is not parallel to the plane (dot(d,nrm) != 0). */
tsu_v3 tsu_ray_plane(tsu_ray ray, tsu_v3 p, tsu_v3 nrm);

/* ---- the gizmo: a click-select + drag state machine -------------------- */

typedef struct {
    int    selected;     /* id of the selected target, -1 = none */
    int    dragging;     /* 1 while a drag is in progress */
    int    prev_down;    /* mouse-down state last update (edge detection) */
    int    grab_axis;    /* axis of the grabbed handle: -1 free, 0/1/2 = X/Y/Z */
    tsu_v3 grab_center;  /* object center captured at grab */
    float  grab_t;       /* axis param at grab (constrained drag) */
    tsu_v3 grab_offset;  /* selected-center minus grab hit point (free drag) */
    tsu_v3 plane_p;      /* drag-plane point (free drag) */
    tsu_v3 plane_n;      /* drag-plane normal (free drag) */
} tsu_gizmo;

void tsu_gizmo_init(tsu_gizmo *g);

/* One frame of manipulation. On a fresh press (down edge) over a target:
   select it and begin dragging on a plane through its center facing the
   camera (normal = -cam_fwd). While dragging with the button held, write the
   target's new center to *moved (and its id to *out_id) and return 1. On
   release, stop. Returns 0 (no move) otherwise. cam_fwd must be unit. */
int tsu_gizmo_update(tsu_gizmo *g, tsu_ray ray, int mouse_down, tsu_v3 cam_fwd,
                     const tsu_target *t, int n, int *out_id, tsu_v3 *moved);

#endif /* TSUMAMI_H */
