#include "tsumami.h"
#include <math.h>

static tsu_v3 v3(float x, float y, float z) { tsu_v3 v; v.x = x; v.y = y; v.z = z; return v; }
static tsu_v3 add3(tsu_v3 a, tsu_v3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static tsu_v3 sub3(tsu_v3 a, tsu_v3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static tsu_v3 scl3(tsu_v3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float  dot3(tsu_v3 a, tsu_v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static tsu_v3 norm3(tsu_v3 a) {
    float l = (float)sqrt((double)dot3(a, a));
    return l > 0.0f ? scl3(a, 1.0f / l) : a;
}

tsu_ray tsu_ray_from_screen(tsu_v3 eye, tsu_v3 fwd, tsu_v3 right, tsu_v3 up,
                            float tan_half_fov, float aspect,
                            float mx, float my, float w, float h) {
    float nx = (2.0f * mx / w - 1.0f) * aspect * tan_half_fov;
    float ny = (1.0f - 2.0f * my / h) * tan_half_fov; /* pixels y-down -> flip */
    tsu_ray r;
    r.o = eye;
    r.d = norm3(add3(fwd, add3(scl3(right, nx), scl3(up, ny))));
    return r;
}

float tsu_ray_aabb(tsu_ray ray, tsu_v3 mn, tsu_v3 mx) {
    float tmin = -1e30f, tmax = 1e30f;
    float o[3], d[3], lo[3], hi[3];
    int i;
    o[0] = ray.o.x; o[1] = ray.o.y; o[2] = ray.o.z;
    d[0] = ray.d.x; d[1] = ray.d.y; d[2] = ray.d.z;
    lo[0] = mn.x; lo[1] = mn.y; lo[2] = mn.z;
    hi[0] = mx.x; hi[1] = mx.y; hi[2] = mx.z;
    for (i = 0; i < 3; i++) {
        if (d[i] != 0.0f) {
            float t1 = (lo[i] - o[i]) / d[i];
            float t2 = (hi[i] - o[i]) / d[i];
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
        } else if (o[i] < lo[i] || o[i] > hi[i]) {
            return -1.0f; /* parallel and outside this slab */
        }
    }
    if (tmax < tmin || tmax < 0.0f) {
        return -1.0f;
    }
    return tmin >= 0.0f ? tmin : 0.0f; /* entry ahead, or 0 if origin is inside */
}

int tsu_pick(tsu_ray ray, const tsu_target *t, int n) {
    int i, best = -1;
    float bt = 1e30f;
    for (i = 0; i < n; i++) {
        float d = tsu_ray_aabb(ray, sub3(t[i].center, t[i].half),
                               add3(t[i].center, t[i].half));
        if (d >= 0.0f && d < bt) {
            bt = d;
            best = t[i].id;
        }
    }
    return best;
}

tsu_v3 tsu_ray_plane(tsu_ray ray, tsu_v3 p, tsu_v3 nrm) {
    float t = dot3(sub3(p, ray.o), nrm) / dot3(ray.d, nrm);
    return add3(ray.o, scl3(ray.d, t));
}

/* unit direction of world axis 0/1/2 = X/Y/Z */
static tsu_v3 axis_dir(int a) {
    return v3(a == 0 ? 1.0f : 0.0f, a == 1 ? 1.0f : 0.0f, a == 2 ? 1.0f : 0.0f);
}

/* parameter t on the axis line (point p, unit dir d) of the point closest to
   `ray`. Lets a constrained drag slide the object along the axis. */
static float ray_axis_t(tsu_ray ray, tsu_v3 p, tsu_v3 d) {
    tsu_v3 w0 = sub3(p, ray.o);
    float b = dot3(d, ray.d);          /* d, ray.d are unit -> a = c = 1 */
    float denom = 1.0f - b * b;
    if (denom < 1e-6f) {
        return 0.0f;                   /* axis parallel to the ray */
    }
    return (b * dot3(ray.d, w0) - dot3(d, w0)) / denom;
}

void tsu_gizmo_init(tsu_gizmo *g) {
    g->selected = -1;
    g->dragging = 0;
    g->prev_down = 0;
    g->grab_axis = -1;
    g->grab_center = v3(0.0f, 0.0f, 0.0f);
    g->grab_t = 0.0f;
    g->grab_offset = v3(0.0f, 0.0f, 0.0f);
    g->plane_p = v3(0.0f, 0.0f, 0.0f);
    g->plane_n = v3(0.0f, 0.0f, 1.0f);
}

int tsu_gizmo_update(tsu_gizmo *g, tsu_ray ray, int mouse_down, tsu_v3 cam_fwd,
                     const tsu_target *t, int n, int *out_id, tsu_v3 *moved) {
    int did = 0;
    if (mouse_down && !g->prev_down) {           /* press edge: try to grab */
        int id = tsu_pick(ray, t, n);
        if (id >= 0) {
            tsu_v3 c = v3(0.0f, 0.0f, 0.0f);
            int i;
            for (i = 0; i < n; i++) {
                if (t[i].id == id) c = t[i].center;
            }
            g->selected = id;
            g->dragging = 1;
            g->plane_n = scl3(cam_fwd, -1.0f);   /* face the camera */
            g->plane_p = c;
            g->grab_offset = sub3(c, tsu_ray_plane(ray, c, g->plane_n));
        }
    }
    if (g->dragging && mouse_down) {             /* dragging: follow the ray */
        tsu_v3 hit = tsu_ray_plane(ray, g->plane_p, g->plane_n);
        *moved = add3(hit, g->grab_offset);
        *out_id = g->selected;
        did = 1;
    }
    if (!mouse_down) {
        g->dragging = 0;
    }
    g->prev_down = mouse_down;
    return did;
}
