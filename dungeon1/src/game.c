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
#include "mono.h"    /* the entity model */

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
#define CSG_SCRATCH_SIZE   (8u << 20)   /* 56..64 MB: the tail of the transient block */
/* the editor's eval scratch (BSP + fold buffers) must fit the CSG region;
   bumping HENSHU_MAX_POLYS past this is a compile error, not a silent overrun. */
typedef char henshu_scratch_fits[(HENSHU_SCRATCH_SIZE <= CSG_SCRATCH_SIZE) ? 1 : -1];

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
    tsu_gizmo   ent_gizmo;     /* viewport pick/drag over mono entities (reuses
                                  the editor's arrow meshes for drawing) */
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

static mat4 scale_m(f32 x, f32 y, f32 z) {
    mat4 m = mat4_identity();
    m.m[0] = x; m.m[5] = y; m.m[10] = z;
    return m;
}

/* an entity's world matrix: translate * Y-rotate * scale (rotation in euler
   degrees; only yaw is applied for now -- enough to place + spin the demo). */
static mat4 entity_matrix(const Entity *e) {
    mat4 t = translate_m(e->px, e->py, e->pz);
    mat4 r = rotate_y_m(DEG2RAD(e->ry));
    mat4 s = scale_m(e->sx, e->sy, e->sz);
    return mat4_mul(t, mat4_mul(r, s));
}

/* ---- entity gizmo: pick/drag mono entities in the viewport via tsumami ----
   targets mirror henshu's scheme (id = index*4 + slot; slot 0 = body, 1/2/3 =
   the X/Y/Z arrow handles, built only for the selection). */
#define ENT_BODY_R 0.6f   /* barrel body pick radius */
#define ENT_GIZ_LEN 0.7f  /* arrow half-length */
#define ENT_GIZ_R   0.22f /* arrow pick radius */

static int ent_targets(game_state *gs, tsu_target *t, int cap) {
    mono_id selh = (mono_id)gs->entity_sel;
    int i, a, n = 0;
    for (i = 0; i < MONO_MAX && n < cap; i++) {
        Entity *p = &gs->world.ents[i];
        tsu_v3 c;
        if (p->kind == MONO_NONE) {
            continue;
        }
        c.x = p->px; c.y = p->py; c.z = p->pz;
        t[n].id = i * 4; t[n].center = c; t[n].origin = c; t[n].axis = -1;
        t[n].half.x = t[n].half.y = t[n].half.z = ENT_BODY_R;
        n++;
        if (MONO_ID(i, p->generation) != selh) {
            continue; /* only the selected entity gets axis handles */
        }
        for (a = 0; a < 3 && n < cap; a++) {
            tsu_v3 ac = c;
            if (a == 0) ac.x += ENT_GIZ_LEN; else if (a == 1) ac.y += ENT_GIZ_LEN;
            else ac.z += ENT_GIZ_LEN;
            t[n].id = i * 4 + a + 1;
            t[n].center = ac; t[n].origin = c; t[n].axis = a;
            t[n].half.x = (a == 0) ? ENT_GIZ_LEN : ENT_GIZ_R;
            t[n].half.y = (a == 1) ? ENT_GIZ_LEN : ENT_GIZ_R;
            t[n].half.z = (a == 2) ? ENT_GIZ_LEN : ENT_GIZ_R;
            n++;
        }
    }
    return n;
}

/* ---- entity outliner: a minimal game-side scene panel over the mono table.
   lists every live entity, click-selects (handle stored in gs->entity_sel), and
   moves the selection with position steppers. separate from the henshu CSG
   editor's "scene" list (which edits the one CSG solid's primitives). */
#define ENT_PANEL_W  190.0f
#define ENT_LIST_MAX 64

static void ent_axis(ito rowid, ito minus, ito plus, const char *name,
                     char *buf, size_t buflen, f32 *val, f32 step) {
    ito lab = ito_format(buf, buflen, "%s %.2f", name, *val);
    ui_row_begin(rowid);
        ui_label(lab, 14);
        if (ui_button(minus, ITO("-"))) *val -= step;
        if (ui_button(plus,  ITO("+"))) *val += step;
    ui_row_end();
}

static void entities_panel(void *user) {
    game_state *gs = (game_state *)user;
    static char rid[ENT_LIST_MAX][8];
    static char rlab[ENT_LIST_MAX][32];
    static char bx[24], by[24], bz[24];
    Entity *sel;
    int i, shown = 0;
    ui_panel_begin(ITO("ents"), ENT_PANEL_W);
    ui_label(ITO("ENTITIES"), 18);
    for (i = 0; i < MONO_MAX && shown < ENT_LIST_MAX; i++) {
        Entity *p = &gs->world.ents[i];
        mono_id h;
        ito id, lab;
        if (p->kind == MONO_NONE) {
            continue;
        }
        h   = MONO_ID(i, p->generation);
        id  = ito_format(rid[shown], sizeof(rid[shown]), "e%d", i);
        lab = ito_format(rlab[shown], sizeof(rlab[shown]), "%d  %s", i,
                         p->kind == MONO_CSG ? "csg" : "prop");
        if (ui_select_row(id, lab, (mono_id)gs->entity_sel == h)) {
            gs->entity_sel = (int)h;
            gs->editor.csg_selected = -1; /* entity level: deselect any CSG solid */
        }
        shown++;
    }
    sel = mono_get(&gs->world, (mono_id)gs->entity_sel);
    if (sel) {
        ui_label_dim(ITO("position"), 13);
        ent_axis(ITO("ex"), ITO("exm"), ITO("exp"), "x", bx, sizeof bx, &sel->px, 0.25f);
        ent_axis(ITO("ey"), ITO("eym"), ITO("eyp"), "y", by, sizeof by, &sel->py, 0.25f);
        ent_axis(ITO("ez"), ITO("ezm"), ITO("ezp"), "z", bz, sizeof bz, &sel->pz, 0.25f);
    } else {
        ui_label_dim(ITO("no selection"), 13);
    }
    ui_panel_end();
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
        ui_panel_register(ITO("ents"), entities_panel, gs); /* entity outliner */
    }
    tsu_gizmo_init(&es->ent_gizmo);
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
        /* seed a row of entities (props) -- the entity model, live. */
        mono_init(&gs->world);
        {
            int k;
            for (k = 0; k < 5; k++) {
                mono_id id = mono_spawn(&gs->world, MONO_PROP);
                Entity *p = mono_get(&gs->world, id);
                if (p) {
                    p->px = (f32)(k - 2) * 1.8f;
                    p->py = 0.0f;
                    p->pz = -2.5f;
                    p->spin = 0.5f + (f32)k * 0.4f; /* each prop spins differently */
                }
            }
        }
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
    /* a trivial system over the entity table: advance each entity's yaw by its
       own spin rate (rad/s -> degrees, wrapped). per-entity behaviour data. */
    {
        int k;
        for (k = 0; k < MONO_MAX; k++) {
            Entity *p = &gs->world.ents[k];
            if (p->kind == MONO_NONE) continue;
            p->ry = fmodf(p->ry + p->spin * input->dt * (180.0f / 3.14159265358979f), 360.0f);
        }
    }

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

    /* the CSG solid is a MONO_CSG entity: its transform positions the whole
       solid. ensure exactly one exists (idempotent -- also covers hot state that
       was initialized before this entity kind), then read its world origin. */
    tsu_v3 csg_origin;
    mono_id csg_handle = MONO_INVALID;
    csg_origin.x = csg_origin.y = csg_origin.z = 0.0f;
    {
        int k, found = 0;
        for (k = 0; k < MONO_MAX; k++) {
            if (gs->world.ents[k].kind == MONO_CSG) { found = 1; break; }
        }
        if (!found) {
            mono_id id = mono_spawn(&gs->world, MONO_CSG);
            Entity *p = mono_get(&gs->world, id);
            if (p) { p->px = p->py = p->pz = 0.0f; k = id == MONO_INVALID ? MONO_MAX : (int)(id & 0xFFFFu); }
        }
        if (k < MONO_MAX) {
            csg_origin.x = gs->world.ents[k].px;
            csg_origin.y = gs->world.ents[k].py;
            csg_origin.z = gs->world.ents[k].pz;
            csg_handle = MONO_ID(k, gs->world.ents[k].generation);
        }
    }

    /* two selection levels: an ENTITY (entities list -> the entity gizmo moves the
       whole thing, a CSG group as one) OR a CSG SOLID (the CSG-solids list, i.e.
       henshu's scene panel -> henshu's shape gizmo edits that primitive). a solid
       being selected is what puts the viewport into CSG-edit; picking an entity
       clears it (see the entities panel). */
    int editing_csg = (gs->editor.csg_selected >= 0 &&
                       gs->editor.csg_selected < gs->editor.csg_count);
    /* keep the entities list highlighting the CSG entity while one of its solids
       is being edited. */
    if (editing_csg) {
        gs->entity_sel = (int)csg_handle;
    }

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
        hv.origin = csg_origin;   /* the CSG solid's world position */
        hv.viewport_gizmo = editing_csg; /* shape gizmo only while editing the CSG */
        hv.left_pad = ENT_PANEL_W; /* the entities panel sits between scene + view,
                                      so the scene grip belongs at its far edge */
    }
    henshu_update(&gs->editor, &es->editor, &hv);

    /* entity gizmo: pick/drag mono entities in the viewport. OFF while editing a
       CSG group (henshu's shape gizmo owns the viewport then). gated to the right
       of both left panels (scene + entities). */
    if (!editing_csg) {
        tsu_ray ray = tsu_ray_from_screen(hv.eye, hv.fwd, hv.right, hv.up,
                                          hv.tan_half_fov, hv.aspect,
                                          hv.mouse_x, hv.mouse_y, hv.screen_w, hv.screen_h);
        tsu_target targets[ENT_LIST_MAX * 4];
        int nt = ent_targets(gs, targets, ENT_LIST_MAX * 4);
        int out_id = -1;
        tsu_v3 mv;
        int mouse = (input->mouse_left && gs->editor.ui_drag == 0 &&
                     input->mouse_x > gs->editor.scene_w + ENT_PANEL_W &&
                     input->mouse_x < (f32)w - gs->editor.inspector_w) ? 1 : 0;
        if (tsu_gizmo_update(&es->ent_gizmo, ray, mouse, hv.fwd, targets, nt, &out_id, &mv)) {
            Entity *p = &gs->world.ents[out_id / 4]; /* id = index*4 + slot */
            if (p->kind != MONO_NONE) { p->px = mv.x; p->py = mv.y; p->pz = mv.z; }
        }
        if (mouse && es->ent_gizmo.selected >= 0) {
            int idx = es->ent_gizmo.selected / 4;
            Entity *p = &gs->world.ents[idx];
            if (p->kind != MONO_NONE) gs->entity_sel = (int)MONO_ID(idx, p->generation);
        }
    }

    /* the CSG mesh + the selected shape's move gizmo, drawn at the CSG entity's
       world origin (pre-multiply the MVP -- the CSG model is built in local). */
    henshu_draw_scene(&gs->editor, &es->editor,
                      mat4_mul(mvp, translate_m(csg_origin.x, csg_origin.y, csg_origin.z)),
                      editing_csg /* shape gizmo only while editing the CSG group */);

    /* the entity gizmo's arrows at the selected entity (reusing the editor's
       axis arrow meshes on the plain pipeline). hidden while editing the CSG. */
    if (!editing_csg) {
        Entity *sp = mono_get(&gs->world, (mono_id)gs->entity_sel);
        if (sp) {
            mat4 gt = translate_m(sp->px, sp->py, sp->pz);
            mat4 gmvp = mat4_mul(mvp, gt);
            int a;
            for (a = 0; a < 3; a++)
                if (es->editor.giz_index_count[a] > 0)
                    rnd_draw_model(es->editor.gizmo_pipeline, es->editor.giz_vbuf[a],
                                   es->editor.giz_ibuf[a], es->editor.giz_tex[a],
                                   es->editor.giz_index_count[a], gmvp, gt);
        }
    }

    /* draw every live entity as the barrel model at its transform -- the entity
       model driving the scene (one draw call per entity; instancing later). */
    if (es->model.index_count > 0) {
        int k;
        for (k = 0; k < MONO_MAX; k++) {
            const Entity *p = &gs->world.ents[k];
            mat4 em, emvp;
            if (p->kind == MONO_NONE || p->kind == MONO_CSG) {
                continue; /* the CSG entity renders as its own mesh, not a barrel */
            }
            em = entity_matrix(p);
            emvp = mat4_mul(mvp, em);
            rnd_draw_model(es->pipeline, es->model.vertex_buffer, es->model.index_buffer,
                           es->model.texture, es->model.index_count, emvp, em);
        }
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
#include "../../meikyu/lib/mono/mono.c"
