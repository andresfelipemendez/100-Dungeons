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

#include <stdio.h>
#include <string.h>

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
} EngineState;

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
        /* First boot of this process: frame the camera around the model. */
        vec3 center = vec3_scale(vec3_add(es->model.bounds_min,
                                          es->model.bounds_max), 0.5f);
        gs->cam_target_x = center.x;
        gs->cam_target_y = center.y;
        gs->cam_target_z = center.z;
        gs->cam_radius = es->model_radius * 2.4f;
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
                                 es->model_radius * 0.05f + 0.01f,
                                 es->model_radius * 20.0f + 10.0f);
    mat4 view = camera_view(&cam);
    mat4 mvp = mat4_mul(proj, view);

    rnd_draw_model(es->pipeline, es->model.vertex_buffer, es->model.index_buffer,
                   es->model.texture, es->model.index_count, mvp, mat4_identity());

#ifdef EDITOR_BUILD
    editor_frame(memory, api, input, (f32)w, (f32)h);
#endif

    rnd_frame_end();
}



