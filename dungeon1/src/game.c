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
} EngineState;

/* ---- horu composite mesh: a Menger sponge built from boxes --------------- */

#define HORU_MAX_POLYS 4096
#define HORU_MAX_VERTS 16384

static horu_poly g_horu_polys[HORU_MAX_POLYS];
static float     g_horu_verts[HORU_MAX_VERTS * 8]; /* pos3, normal3, uv2 */
static u32       g_horu_index[HORU_MAX_VERTS];

/* Emit the surviving cubes of a Menger sponge: subdivide into 3x3x3 and drop
   the center cube + the 6 face-centers (cells with >= 2 zero coords), recurse
   on the rest. Additive -- the kept cubes ARE the sponge, no CSG subtraction. */
static int sponge(horu_poly *out, int cap, int n,
                  float cx, float cy, float cz, float s, int depth) {
    int i, j, k;
    if (depth == 0) {
        return n + horu_box_polys(cx, cy, cz, s, s, s, out + n, cap - n);
    }
    {
        float t = s / 3.0f;
        for (i = -1; i <= 1; i++) {
            for (j = -1; j <= 1; j++) {
                for (k = -1; k <= 1; k++) {
                    int zeros = (i == 0) + (j == 0) + (k == 0);
                    if (zeros >= 2) {
                        continue; /* removed cell */
                    }
                    n = sponge(out, cap, n,
                               cx + (float)i * t, cy + (float)j * t,
                               cz + (float)k * t, t, depth - 1);
                }
            }
        }
    }
    return n;
}

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

    /* build the horu composite mesh: a depth-2 Menger sponge of boxes */
    {
        int np = sponge(g_horu_polys, HORU_MAX_POLYS, 0,
                        0.0f, 0.0f, 0.0f, 2.0f, 2);
        u32 nv = horu_build_buffers(g_horu_polys, np, g_horu_verts,
                                    HORU_MAX_VERTS, g_horu_index);
        es->horu_vbuf = rnd_buffer_create_vertex(g_horu_verts,
                                                 (u64)nv * 8 * sizeof(float));
        es->horu_ibuf = rnd_buffer_create_index(g_horu_index,
                                                (u64)nv * sizeof(u32));
        es->horu_index_count = nv;
        es->horu_radius = (f32)sqrt(3.0); /* half-diagonal of the size-2 cube */
        if (!es->horu_vbuf.id || !es->horu_ibuf.id) {
            return 0;
        }
        api->log("game: horu Menger sponge = %d polys, %u verts", np, nv);
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
        /* First boot of this process: frame the camera around the sponge
           (centred at the origin). */
        gs->cam_target_x = 0.0f;
        gs->cam_target_y = 0.0f;
        gs->cam_target_z = 0.0f;
        gs->cam_radius = es->horu_radius * 2.4f;
        gs->cam_angle = 0.0f;
        gs->cam_pitch = 0.5f;
        gs->clear_r = 0.10f;
        gs->clear_g = 0.05f;
        gs->clear_b = 0.18f;
        gs->spin_rate = 1.8f;
        gs->initialized = 1;
    }

    gs->cam_angle += input->dt * gs->spin_rate;

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

    /* the horu composite mesh (Menger sponge), textured with the model's
       colormap and lit by face normals */
    rnd_draw_model(es->pipeline, es->horu_vbuf, es->horu_ibuf,
                   es->model.texture, es->horu_index_count, mvp, mat4_identity());

#ifdef EDITOR_BUILD
    editor_frame(memory, api, input, (f32)w, (f32)h);
#endif

    rnd_frame_end();
}

/* horu implementation, compiled into the game dll (unity-style include) */
#include "../../meikyu/lib/horu/horu.c"



