#include "abi/abi_platform.h"
#include "game_state.h"
#include "engine/render/render.h"
#include "engine/asset/model.h"
#include "engine/ui/ui.h"
#include "engine/theme.h"
#include "seni_panel.h"
#ifdef EDITOR_BUILD
#include "editor.h"
#endif
#include "linalg.h"
#include "camera.h"

/* henshu (編集): the reusable CSG scene editor. The game owns only its camera,
   its demo barrel, the clear colour, and the wiring -- every editor concern
   (the shape model, panels, gizmo, mesh, grips) lives in the lib. */
#include "henshu.h"
#include "henshu_ui.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* build/game_state.h is the snapshot reload pins both the #include (-Ibuild
   first) and this .incbin to, so the embedded layout always matches the layout
   the dll compiled against. With a nested editor struct the snapshot is the
   concatenation henshu_state.h + game_state.h (see the project marker's
   layout_include key); the embed reads those same concatenated bytes. */
#include "seni_embed.h"
SENI_EMBED_LAYOUT(PLATFORM_BUILD_DIR "/game_state.h");

#ifndef ASSETS_DIR
#define ASSETS_DIR "../assets"
#endif

#define DEG2RAD(d) ((d) * 3.14159265358979f / 180.0f)
#define TAU         6.28318530718f
#define BARREL_POS_X 3.5f    /* the demo barrel parked beside the CSG model */
#define BARREL_SPIN  1.2f    /* rad/s */

/* Transient block layout: EngineState at offset 0, UI memory after it, model
   warm cache after that (survives reloads), CSG scratch last. */
#define UI_MEM_OFFSET      (4u << 20)
#define UI_MEM_SIZE        (16u << 20)
#define MODEL_CACHE_OFFSET (24u << 20)
#define MODEL_CACHE_SIZE   (32u << 20)
#define CSG_SCRATCH_OFFSET (56u << 20)  /* henshu's CSG working memory */

/* Cold state: rebuilt from scratch on every reload. Pointers + render handles
   are fine here -- nothing in it has to survive a reload. */
typedef struct {
    b32         ready;
    RndPipeline pipeline;      /* plain model shader: the barrel + gizmo arrows */
    RndPipeline csg_pipeline;  /* triplanar grid shader for the CSG mesh */
    Model       model;
    f32         model_radius;
    f32         horu_radius;   /* camera framing radius for the CSG scene */
    EditorCold  editor;        /* the henshu editor's cold GPU state */
} EngineState;

static mat4 translate_m(f32 x, f32 y, f32 z) {
    mat4 m = mat4_identity();
    m.m[3] = x; m.m[7] = y; m.m[11] = z; /* row-major translation column */
    return m;
}

static mat4 rotate_y_m(f32 a) {
    mat4 m = mat4_identity();
    f32 c = cosf(a), s = sinf(a);
    m.m[0] = c; m.m[2] = s; m.m[8] = -s; m.m[10] = c; /* row-major Y rotation */
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
    es->csg_pipeline = rnd_pipeline_create(PLATFORM_BUILD_DIR "/csg.vert.spv",
                                           PLATFORM_BUILD_DIR "/csg.frag.spv");
    if (!es->csg_pipeline.id) {
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
    es->horu_radius = 3.0f;
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
    /* the editor: cold GPU objects (grid, gizmo arrows) + its panels. The game
       provides the two pipelines and the CSG scratch region. */
    if (!henshu_cold_init(&es->editor, (u8 *)memory->transient + CSG_SCRATCH_OFFSET,
                          es->csg_pipeline, es->pipeline)) {
        return 0;
    }
    {
        game_state *gs = (game_state *)memory->hot;
        henshu_register_panels(&gs->editor);
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
        /* the GPU mesh lives in the (transient) editor cold state the reload just
           rebuilt -- the hot model survived but its uploaded mesh did not. Force
           a re-mesh so the model is visible immediately, not only after an edit. */
        gs->editor.csg_dirty = 1;
    }
    if (!es->ready) {
        return;
    }

    if (!gs->initialized) {
        /* First boot: a starter CSG model + a stable angled view (no spin, so
           viewport dragging is predictable). */
        henshu_default(&gs->editor);
        gs->cam_target_x = 0.0f;
        gs->cam_target_y = 0.0f;
        gs->cam_target_z = 0.0f;
        gs->cam_radius = es->horu_radius * 2.6f;
        gs->cam_angle = 0.7f;
        gs->cam_pitch = 0.5f;
        gs->clear_r = THEME_CLEAR_R;
        gs->clear_g = THEME_CLEAR_G;
        gs->clear_b = THEME_CLEAR_B;
        gs->spin_rate = 0.0f;
        gs->initialized = 1;
    }

    /* wrap to [0, 2pi) so angles never grow large enough to lose fractional
       precision in sin/cos (visible jitter after a long run). */
    gs->cam_angle = fmodf(gs->cam_angle + input->dt * gs->spin_rate, TAU);
    gs->barrel_angle = fmodf(gs->barrel_angle + input->dt * BARREL_SPIN, TAU);

    /* a hot model carried over from an older format is invalid for the flat-list
       fold -- reset it once. */
    if (!henshu_model_ok(&gs->editor)) {
        henshu_default(&gs->editor);
    }

    /* re-mesh when an edit (panel or drag) marked the model dirty */
    if (gs->editor.csg_dirty) {
        henshu_rebuild(&gs->editor, &es->editor);
        gs->editor.csg_dirty = 0;
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

    /* the editor needs the camera basis to pick/drag in the viewport; build it
       from the orbit camera and hand it the mouse + screen size. */
    henshu_view hv;
    {
        f32 cp = cosf(gs->cam_pitch);
        vec3 off = vec3_make(gs->cam_radius * cp * cosf(gs->cam_angle),
                             gs->cam_radius * sinf(gs->cam_pitch),
                             gs->cam_radius * cp * sinf(gs->cam_angle));
        vec3 eye = vec3_add(cam.target, off);
        vec3 fwd = vec3_norm(vec3_sub(cam.target, eye));
        vec3 right = vec3_norm(vec3_cross(fwd, vec3_make(0, 1, 0)));
        vec3 up = vec3_cross(right, fwd);
        hv.eye.x = eye.x; hv.eye.y = eye.y; hv.eye.z = eye.z;
        hv.fwd.x = fwd.x; hv.fwd.y = fwd.y; hv.fwd.z = fwd.z;
        hv.right.x = right.x; hv.right.y = right.y; hv.right.z = right.z;
        hv.up.x = up.x; hv.up.y = up.y; hv.up.z = up.z;
        hv.tan_half_fov = tanf(DEG2RAD(30.0f));
        hv.aspect = aspect;
        hv.mouse_x = input->mouse_x; hv.mouse_y = input->mouse_y;
        hv.screen_w = (f32)w; hv.screen_h = (f32)h;
        hv.mouse_left = input->mouse_left;
    }
    henshu_update(&gs->editor, &es->editor, &hv);

    /* the CSG mesh + the selected shape's move gizmo */
    henshu_draw_scene(&gs->editor, &es->editor, mvp);

    /* the previous rotating barrel, parked beside the CSG model and spinning */
    if (es->model.index_count > 0) {
        mat4 bm = mat4_mul(translate_m(BARREL_POS_X, 0.0f, 0.0f),
                           rotate_y_m(gs->barrel_angle));
        mat4 bmvp = mat4_mul(mvp, bm);
        rnd_draw_model(es->pipeline, es->model.vertex_buffer, es->model.index_buffer,
                       es->model.texture, es->model.index_count, bmvp, bm);
    }

#ifdef EDITOR_BUILD
    editor_frame(memory, api, input, (f32)w, (f32)h);
    /* the resizable panel grips, drawn over the UI so they sit on the boundary */
    henshu_draw_overlay(&gs->editor, &hv);
#endif

    rnd_frame_end();
}

/* lib implementations, compiled into the game dll (unity-style include) */
#include "../../meikyu/lib/horu/horu.c"
#include "../../meikyu/lib/tsumami/tsumami.c"
#include "../../meikyu/lib/henshu/henshu.c"
#include "../../meikyu/lib/henshu/henshu_ui.c"
