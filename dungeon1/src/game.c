#include "abi/abi_platform.h"
#include "game_state.h"
#include "engine/render/render.h"
#include "engine/asset/model.h"
#include "engine/ui/ui.h"
#include "seni_panel.h"
#ifdef EDITOR_BUILD
#include "editor.h"
#endif
#include "linalg.h"
#include "camera.h"

/* horu (彫): first-party CSG lib. Pulled in by relative path -- lib/horu is
   not a game include root -- and compiled into the dll (impl #included at the
   bottom of this file). */
#include "../../meikyu/lib/horu/horu.h"
#include "../../meikyu/lib/tsumami/tsumami.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* build/game_state.h is reload.bat's snapshot of src/game_state.h; both the
   #include above (-Ibuild first) and this .incbin read the snapshot so the
   embedded layout always matches the layout the dll compiled against. The
   path is resolved by the assembler relative to the compiler's cwd, which
   reload.bat pins to the project root. */
#include "seni_embed.h"
SENI_EMBED_LAYOUT(PLATFORM_BUILD_DIR "/game_state.h");

#ifndef ASSETS_DIR
#define ASSETS_DIR "../assets"
#endif

#define DEG2RAD(d) ((d) * 3.14159265358979f / 180.0f)

/* Transient block layout: EngineState at offset 0, UI memory after it,
   model warm cache after that (survives reloads; parse/decode skipped). */
#define UI_MEM_OFFSET    (4u << 20)
#define UI_MEM_SIZE      (16u << 20)
#define MODEL_CACHE_OFFSET (24u << 20)
#define MODEL_CACHE_SIZE   (32u << 20)
#define CSG_SCRATCH_OFFSET (56u << 20)  /* horu's working memory (HORU_CSG_SCRATCH) */

/* Cold state: lives in the transient block, rebuilt from scratch on every
   reload. Pointers and render handles are allowed here precisely because
   nothing in it ever has to survive a reload. */
typedef struct {
    b32         ready;
    RndPipeline pipeline;
    Model       model;
    f32         model_radius;
    /* horu composite mesh (a Menger sponge of boxes) */
    RndBuffer   horu_vbuf;
    RndBuffer   horu_ibuf;
    u32         horu_index_count;
    f32         horu_radius;
    tsu_gizmo   gizmo;        /* viewport pick/drag state */
    RndBuffer   giz_vbuf;     /* the 3-axis arrow gizmo mesh (built once) */
    RndBuffer   giz_ibuf;
    u32         giz_index_count;
    void       *csg_scratch;  /* horu CSG working memory, in the transient block */
} EngineState;

/* ---- horu composite mesh: a Menger sponge built from boxes --------------- */

#define HORU_MAX_POLYS 4096
#define HORU_MAX_VERTS 16384

static horu_poly g_horu_polys[HORU_MAX_POLYS];
static float     g_horu_verts[HORU_MAX_VERTS * 8]; /* pos3, normal3, uv2 */
static u32       g_horu_index[HORU_MAX_VERTS];

/* Fan-triangulate polygons into the interleaved (pos,normal,uv) vertex buffer,
   flat-shaded (each vertex takes its face's plane normal). Non-indexed: index
   i = i. Returns the vertex/index count. */
static u32 horu_build_buffers(const horu_poly *polys, int np,
                              float *verts, u32 vcap, u32 *index) {
    u32 vn = 0;
    int i, k, t;
    for (i = 0; i < np; i++) {
        const horu_poly *p = &polys[i];
        for (k = 0; k + 2 < p->n; k++) {
            int fan[3];
            fan[0] = 0; fan[1] = k + 1; fan[2] = k + 2;
            for (t = 0; t < 3; t++) {
                float *vp;
                if (vn >= vcap) {
                    return vn;
                }
                vp = verts + vn * 8;
                vp[0] = p->v[fan[t]].x; vp[1] = p->v[fan[t]].y; vp[2] = p->v[fan[t]].z;
                vp[3] = p->plane.nx;    vp[4] = p->plane.ny;    vp[5] = p->plane.nz;
                vp[6] = 0.5f;           vp[7] = 0.5f; /* flat texel from the model tex */
                index[vn] = vn;
                vn++;
            }
        }
    }
    return vn;
}

/* ---- CSG editor: hot-state tree -> polygons -> mesh --------------------- */

/* csg_kind: 0 box, 1 sphere, 2 cylinder, 3 polygon (primitives);
            4 union, 5 difference, 6 intersection (ops over csg_a, csg_b). */
enum { CSG_BOX, CSG_SPHERE, CSG_CYL, CSG_POLY, CSG_UNION, CSG_DIFF, CSG_ISECT };

static int csg_is_op(int k) { return k >= CSG_UNION; }

/* evaluate a node into polygons (recursive). stack scratch per level; models
   here are small. */
static int csg_eval(const game_state *gs, int node, horu_poly *out, int cap,
                    void *scratch) {
    int k;
    if (node < 0 || node >= gs->csg_count) {
        return 0;
    }
    k = gs->csg_kind[node];
    if (k == CSG_BOX) {
        return horu_box_polys(gs->csg_x[node], gs->csg_y[node], gs->csg_z[node],
                              gs->csg_sx[node], gs->csg_sy[node], gs->csg_sz[node],
                              out, cap);
    }
    if (k == CSG_SPHERE) {
        /* keep the facet count low: each polygon adds a BSP split, and the
           recursive clip keeps big scratch arrays on the stack (see the note
           on csg_eval). ~32 polys keeps the recursion well within the stack. */
        return horu_sphere_polys(gs->csg_x[node], gs->csg_y[node], gs->csg_z[node],
                                 gs->csg_sx[node], 8, 4, out, cap);
    }
    if (k == CSG_CYL || k == CSG_POLY) {
        return horu_prism_polys(gs->csg_x[node], gs->csg_y[node], gs->csg_z[node],
                                gs->csg_sx[node], gs->csg_sy[node],
                                (k == CSG_CYL) ? 12 : 6, out, cap);
    }
    {   /* op: evaluate the two children, then boolean them. horu's BSP does
           all its work in `scratch` (the engine's transient block), so it
           never touches the stack -- a deep/dense model can't fault. */
        horu_poly la[256], lb[256];
        horu_op op = (k == CSG_UNION) ? HORU_UNION
                   : (k == CSG_DIFF)  ? HORU_DIFFERENCE : HORU_INTERSECTION;
        int na = csg_eval(gs, gs->csg_a[node], la, 256, scratch);
        int nb = csg_eval(gs, gs->csg_b[node], lb, 256, scratch);
        return horu_csg_polys(op, la, na, lb, nb, out, cap,
                              scratch, HORU_CSG_SCRATCH);
    }
}

/* a leaf primitive's AABB half-extent (boxes use size, spheres/columns radius) */
static void csg_half(const game_state *gs, int node, f32 *hx, f32 *hy, f32 *hz) {
    int k = gs->csg_kind[node];
    if (k == CSG_BOX) {
        *hx = gs->csg_sx[node] * 0.5f; *hy = gs->csg_sy[node] * 0.5f;
        *hz = gs->csg_sz[node] * 0.5f;
    } else if (k == CSG_SPHERE) {
        *hx = *hy = *hz = gs->csg_sx[node];
    } else { /* cyl / poly */
        *hx = *hz = gs->csg_sx[node]; *hy = gs->csg_sy[node] * 0.5f;
    }
}

/* pick targets = the primitive leaves (ops aren't draggable). */
static int csg_targets(const game_state *gs, tsu_target *t, int cap) {
    int i, n = 0;
    for (i = 0; i < gs->csg_count && n < cap; i++) {
        if (!csg_is_op(gs->csg_kind[i])) {
            f32 hx, hy, hz;
            csg_half(gs, i, &hx, &hy, &hz);
            t[n].id = i;
            t[n].center.x = gs->csg_x[i];
            t[n].center.y = gs->csg_y[i];
            t[n].center.z = gs->csg_z[i];
            t[n].half.x = hx; t[n].half.y = hy; t[n].half.z = hz;
            n++;
        }
    }
    return n;
}

/* re-evaluate the root tree and re-upload the GPU buffers. (Creating fresh
   buffers each rebuild leaks the old ones -- bounded per edit; a
   rnd_buffer_destroy would fix it.) */
static void csg_rebuild(game_state *gs, EngineState *es) {
    int np = csg_eval(gs, gs->csg_root, g_horu_polys, HORU_MAX_POLYS,
                      es->csg_scratch);
    u32 nv = horu_build_buffers(g_horu_polys, np, g_horu_verts,
                                HORU_MAX_VERTS, g_horu_index);
    /* free the previous mesh's buffers -- the editor re-meshes on every edit,
       so without this the fixed buffer table fills up and the next draw faults */
    rnd_buffer_destroy(es->horu_vbuf);
    rnd_buffer_destroy(es->horu_ibuf);
    es->horu_vbuf = rnd_buffer_create_vertex(g_horu_verts,
                                             (u64)nv * 8 * sizeof(float));
    es->horu_ibuf = rnd_buffer_create_index(g_horu_index, (u64)nv * sizeof(u32));
    es->horu_index_count = nv;
}

/* add a primitive; chain it into the model with a union node so it shows. */
static void csg_add(game_state *gs, int kind) {
    int p, o;
    if (gs->csg_count + 2 > 32) {
        return;
    }
    p = gs->csg_count++;
    gs->csg_kind[p] = kind;
    gs->csg_x[p] = 0.0f; gs->csg_y[p] = 2.0f; gs->csg_z[p] = 0.0f;
    gs->csg_sx[p] = 1.0f; gs->csg_sy[p] = 1.0f; gs->csg_sz[p] = 1.0f;
    gs->csg_a[p] = -1; gs->csg_b[p] = -1;
    if (gs->csg_count == 1) {
        gs->csg_root = p;
    } else {
        o = gs->csg_count++;
        gs->csg_kind[o] = CSG_UNION;
        gs->csg_a[o] = gs->csg_root;
        gs->csg_b[o] = p;
        gs->csg_root = o;
    }
    gs->csg_selected = p;
    gs->csg_dirty = 1;
}

/* set the combining op of the node whose right child is the selected leaf. */
static void csg_set_op(game_state *gs, int op) {
    int i, s = gs->csg_selected;
    for (i = 0; i < gs->csg_count; i++) {
        if (csg_is_op(gs->csg_kind[i]) && gs->csg_b[i] == s) {
            gs->csg_kind[i] = op;
            gs->csg_dirty = 1;
            return;
        }
    }
}

/* a fresh starter model: a box with a sphere subtracted. */
static void csg_default(game_state *gs) {
    int i;
    for (i = 0; i < 32; i++) { gs->csg_a[i] = -1; gs->csg_b[i] = -1; }
    gs->csg_kind[0] = CSG_BOX;
    gs->csg_x[0] = 0; gs->csg_y[0] = 0; gs->csg_z[0] = 0;
    gs->csg_sx[0] = 2; gs->csg_sy[0] = 2; gs->csg_sz[0] = 2;
    gs->csg_kind[1] = CSG_SPHERE;
    gs->csg_x[1] = 1; gs->csg_y[1] = 1; gs->csg_z[1] = 1; gs->csg_sx[1] = 1.1f;
    gs->csg_kind[2] = CSG_DIFF; gs->csg_a[2] = 0; gs->csg_b[2] = 1;
    gs->csg_count = 3; gs->csg_root = 2; gs->csg_selected = 1;
    gs->drag_node = -1; gs->csg_dirty = 1;
}

/* the editor panel (registered with ui_panel_register; user = game_state). */
static void csg_panel(void *user) {
    game_state *gs = (game_state *)user;
    ui_panel_begin(ITO("csg"), 200.0f);
    ui_label(ITO("CSG EDITOR"), 18);
    ui_row_begin(ITO("add"));
        if (ui_button(ITO("ab"), ITO("+box")))  csg_add(gs, CSG_BOX);
        if (ui_button(ITO("as"), ITO("+sph")))  csg_add(gs, CSG_SPHERE);
        if (ui_button(ITO("ac"), ITO("+cyl")))  csg_add(gs, CSG_CYL);
        if (ui_button(ITO("ap"), ITO("+poly"))) csg_add(gs, CSG_POLY);
    ui_row_end();
    ui_label_dim(ITO("combine selected:"), 13);
    ui_row_begin(ITO("ops"));
        if (ui_button(ITO("ou"), ITO("union")))  csg_set_op(gs, CSG_UNION);
        if (ui_button(ITO("od"), ITO("diff")))   csg_set_op(gs, CSG_DIFF);
        if (ui_button(ITO("oi"), ITO("isect")))  csg_set_op(gs, CSG_ISECT);
    ui_row_end();
    if (gs->csg_selected >= 0 && gs->csg_selected < gs->csg_count &&
        !csg_is_op(gs->csg_kind[gs->csg_selected])) {
        int s = gs->csg_selected;
        ui_label_dim(ITO("size:"), 13);
        ui_row_begin(ITO("sz"));
            if (ui_button(ITO("sm"), ITO("-"))) {
                gs->csg_sx[s] *= 0.8f; gs->csg_sy[s] *= 0.8f; gs->csg_sz[s] *= 0.8f;
                gs->csg_dirty = 1;
            }
            if (ui_button(ITO("sp"), ITO("+"))) {
                gs->csg_sx[s] *= 1.25f; gs->csg_sy[s] *= 1.25f; gs->csg_sz[s] *= 1.25f;
                gs->csg_dirty = 1;
            }
        ui_row_end();
        ui_label_dim(ITO("drag in viewport to move"), 12);
    }
    ui_panel_end();
}

/* ---- the 3-axis translate gizmo: cylinder shafts + cone tips ------------ */

/* a +Y arrow: a thin cylinder shaft (0..L) topped by a cone tip. */
static int gizmo_arrow_y(horu_poly *out, int cap) {
    int n = 0;
    float sr = 0.06f, sl = 1.0f, tr = 0.18f, th = 0.4f;
    n += horu_prism_polys(0.0f, sl * 0.5f, 0.0f, sr, sl, 12, out + n, cap - n);
    n += horu_cone_polys(0.0f, sl + th * 0.5f, 0.0f, tr, th, 12, out + n, cap - n);
    return n;
}

/* rotate a +Y-built polygon onto axis 0(+X), 1(+Y, identity) or 2(+Z) with a
   proper rotation (preserves winding/normals); d is unchanged about origin. */
static horu_poly gizmo_xform(horu_poly p, int axis) {
    int i;
    for (i = 0; i < p.n; i++) {
        float x = p.v[i].x, y = p.v[i].y, z = p.v[i].z;
        if (axis == 0) { p.v[i].x = y;  p.v[i].y = -x; p.v[i].z = z; }
        else if (axis == 2) { p.v[i].x = x; p.v[i].y = -z; p.v[i].z = y; }
    }
    {
        float nx = p.plane.nx, ny = p.plane.ny, nz = p.plane.nz;
        if (axis == 0) { p.plane.nx = ny; p.plane.ny = -nx; p.plane.nz = nz; }
        else if (axis == 2) { p.plane.nx = nx; p.plane.ny = -nz; p.plane.nz = ny; }
    }
    return p;
}

/* three arrows (X, Y, Z) emanating from the origin. */
static int gizmo_polys(horu_poly *out, int cap) {
    horu_poly arr[128];
    int na = gizmo_arrow_y(arr, 128);
    int n = 0, a, i;
    for (a = 0; a < 3; a++) {
        for (i = 0; i < na && n < cap; i++) {
            out[n++] = gizmo_xform(arr[i], a);
        }
    }
    return n;
}

static mat4 translate_m(f32 x, f32 y, f32 z) {
    mat4 m = mat4_identity();
    m.m[3] = x; m.m[7] = y; m.m[11] = z; /* row-major translation column */
    return m;
}

static b32 cold_rebuild(EngineState *es, PlatformMemory *memory, PlatformApi *api) {
    memset(es, 0, sizeof(*es));
    if (!rnd_init(api->gpu_context)) {
        return 0;
    }
    es->pipeline = rnd_pipeline_create(PLATFORM_BUILD_DIR "/model.vert.spv",
                                       PLATFORM_BUILD_DIR "/model.frag.spv");
    if (!es->pipeline.id) {
        return 0;
    }
    if (!model_load_cached(&es->model,
                           (u8 *)memory->transient + MODEL_CACHE_OFFSET,
                           MODEL_CACHE_SIZE,
                           ASSETS_DIR "/Models/GLB format/barrel.glb",
                           ASSETS_DIR "/Models/GLB format/Textures/colormap.png")) {
        return 0;
    }
    es->model_radius = 0.5f * vec3_len(vec3_sub(es->model.bounds_max,
                                                es->model.bounds_min));
    if (es->model_radius < 0.001f) {
        es->model_radius = 1.0f;
    }
    if (!ui_init((u8 *)memory->transient + UI_MEM_OFFSET, UI_MEM_SIZE,
                 1280.0f, 720.0f)) {
        return 0;
    }
    /* ui_init wiped the panel registry; re-register extension panels */
    if (!seni_panel_register(&memory->seni)) {
        return 0;
    }
#ifdef EDITOR_BUILD
    editor_init(memory);
#endif

    /* the CSG mesh is (re)built from the hot-state model whenever it is dirty
       (see game_update_and_render). Register the editor panel; the model
       persists across reloads, so it is initialised once on first boot. */
    es->horu_radius = 3.0f;
    es->horu_index_count = 0;
    es->csg_scratch = (u8 *)memory->transient + CSG_SCRATCH_OFFSET;
    tsu_gizmo_init(&es->gizmo);
    if (!ui_panel_register(ITO("csg"), csg_panel, memory->hot)) {
        return 0;
    }

    {   /* the 3-axis arrow gizmo mesh, built once */
        int np = gizmo_polys(g_horu_polys, HORU_MAX_POLYS);
        u32 nv = horu_build_buffers(g_horu_polys, np, g_horu_verts,
                                    HORU_MAX_VERTS, g_horu_index);
        es->giz_vbuf = rnd_buffer_create_vertex(g_horu_verts,
                                                (u64)nv * 8 * sizeof(float));
        es->giz_ibuf = rnd_buffer_create_index(g_horu_index, (u64)nv * sizeof(u32));
        es->giz_index_count = nv;
    }

    es->ready = 1;
    return 1;
}

GAME_EXPORT GAME_UPDATE_AND_RENDER(game_update_and_render) {
    game_state *gs = (game_state *)memory->hot;
    EngineState *es = (EngineState *)memory->transient;

    if (memory->reloaded) {
        if (!cold_rebuild(es, memory, api)) {
            api->log("game: cold state rebuild failed");
            return;
        }
        api->log("game: dll (re)loaded, cold state rebuilt");
    }
    if (!es->ready) {
        return;
    }

    if (!gs->initialized) {
        /* First boot: a starter CSG model + a stable angled view (no spin, so
           viewport dragging is predictable). */
        csg_default(gs);
        gs->cam_target_x = 0.0f;
        gs->cam_target_y = 0.0f;
        gs->cam_target_z = 0.0f;
        gs->cam_radius = es->horu_radius * 2.6f;
        gs->cam_angle = 0.7f;
        gs->cam_pitch = 0.5f;
        gs->clear_r = 0.10f;
        gs->clear_g = 0.05f;
        gs->clear_b = 0.18f;
        gs->spin_rate = 0.0f;
        gs->initialized = 1;
    }

    gs->cam_angle += input->dt * gs->spin_rate;

    /* re-mesh the model when an edit (panel or drag) marked it dirty */
    if (gs->csg_dirty) {
        csg_rebuild(gs, es);
        gs->csg_dirty = 0;
    }

    if (!rnd_frame_begin(gs->clear_r, gs->clear_g, gs->clear_b)) {
        return;
    }

    u32 w, h;
    rnd_swapchain_size(&w, &h);
    f32 aspect = (f32)w / (f32)h;

    OrbitCamera cam = {
        .target = vec3_make(gs->cam_target_x, gs->cam_target_y, gs->cam_target_z),
        .radius = gs->cam_radius,
        .angle = gs->cam_angle,
        .pitch = gs->cam_pitch,
    };
    mat4 proj = mat4_perspective(DEG2RAD(60.0f), aspect,
                                 es->horu_radius * 0.05f + 0.01f,
                                 es->horu_radius * 20.0f + 10.0f);
    mat4 view = camera_view(&cam);
    mat4 mvp = mat4_mul(proj, view);

    /* viewport pick + drag via tsumami. Build the camera basis + a pick ray
       from the mouse, feed the primitive AABBs, apply the drag to the model.
       Gated to the right of the editor panel so button clicks don't pick. */
    {
        f32 cp = cosf(gs->cam_pitch);
        vec3 off = vec3_make(gs->cam_radius * cp * cosf(gs->cam_angle),
                             gs->cam_radius * sinf(gs->cam_pitch),
                             gs->cam_radius * cp * sinf(gs->cam_angle));
        vec3 eye = vec3_add(cam.target, off);
        vec3 fwd = vec3_norm(vec3_sub(cam.target, eye));
        vec3 right = vec3_norm(vec3_cross(fwd, vec3_make(0, 1, 0)));
        vec3 up = vec3_cross(right, fwd);
        tsu_v3 te, tf, tr, tu;
        tsu_ray ray;
        tsu_target targets[32];
        int nt, mouse, out_id = -1;
        tsu_v3 mv;
        te.x = eye.x; te.y = eye.y; te.z = eye.z;
        tf.x = fwd.x; tf.y = fwd.y; tf.z = fwd.z;
        tr.x = right.x; tr.y = right.y; tr.z = right.z;
        tu.x = up.x; tu.y = up.y; tu.z = up.z;
        ray = tsu_ray_from_screen(te, tf, tr, tu, tanf(DEG2RAD(30.0f)), aspect,
                                  input->mouse_x, input->mouse_y, (f32)w, (f32)h);
        nt = csg_targets(gs, targets, 32);
        mouse = (input->mouse_left && input->mouse_x > 215.0f) ? 1 : 0;
        if (tsu_gizmo_update(&es->gizmo, ray, mouse, tf, targets, nt, &out_id, &mv)) {
            gs->csg_x[out_id] = mv.x;
            gs->csg_y[out_id] = mv.y;
            gs->csg_z[out_id] = mv.z;
            gs->csg_dirty = 1;
        }
        if (es->gizmo.selected >= 0) {
            gs->csg_selected = es->gizmo.selected;
        }
    }

    /* the CSG result mesh, lit by face normals (textured with the model tex) */
    if (es->horu_index_count > 0 && es->horu_vbuf.id && es->horu_ibuf.id) {
        rnd_draw_model(es->pipeline, es->horu_vbuf, es->horu_ibuf,
                       es->model.texture, es->horu_index_count, mvp,
                       mat4_identity());
    }

    /* the translate gizmo at the selected primitive */
    if (gs->csg_selected >= 0 && gs->csg_selected < gs->csg_count &&
        !csg_is_op(gs->csg_kind[gs->csg_selected]) && es->giz_index_count > 0) {
        int s = gs->csg_selected;
        mat4 t = translate_m(gs->csg_x[s], gs->csg_y[s], gs->csg_z[s]);
        mat4 gmvp = mat4_mul(mvp, t);
        rnd_draw_model(es->pipeline, es->giz_vbuf, es->giz_ibuf,
                       es->model.texture, es->giz_index_count, gmvp, t);
    }

#ifdef EDITOR_BUILD
    editor_frame(memory, api, input, (f32)w, (f32)h);
#endif

    rnd_frame_end();
}

/* lib implementations, compiled into the game dll (unity-style include) */
#include "../../meikyu/lib/horu/horu.c"
#include "../../meikyu/lib/tsumami/tsumami.c"



