#ifndef HENSHU_UI_H
#define HENSHU_UI_H

/* henshu (編集) -- reusable CSG scene editor, RENDER/UI GLUE.

   The half that sits on the engine (render + ui + theme): the scene/inspector
   panels, the move gizmo (pick/drag + arrow meshes), the CSG mesh upload/draw,
   the grid texture, and the resizable panel grips. Unlike the core (henshu.h),
   this is NOT a leaf lib -- it is compiled into the game dll (unity include) and
   is not unit-tested. A game wires it in five calls: cold_init, register_panels,
   then per-frame rebuild (on dirty) / update / draw_scene / draw_overlay.

   Shaders are project assets, so the game owns the pipelines and hands them in:
   a triplanar-grid pipeline for the CSG mesh and a plain solid pipeline for the
   gizmo arrows (kept plain so the grid does not bleed onto the gizmo). */

#include "henshu.h"
#include "engine/render/render.h"
#include "engine/ui/ui.h"
#include "linalg.h"

/* The editor's COLD (transient) GPU state -- rebuilt every reload, never
   migrated. The game holds one in its own EngineState and passes its address to
   every glue call. */
typedef struct {
    RndPipeline csg_pipeline;    /* triplanar grid, for the CSG mesh (game-built) */
    RndPipeline gizmo_pipeline;  /* plain solid, for the gizmo arrows (game-built) */
    RndTexture  grid_tex;        /* the square-grid tile, sampled triplanar */
    RndBuffer   mesh_vbuf;       /* the folded CSG mesh, re-uploaded on edits */
    RndBuffer   mesh_ibuf;
    u32         mesh_index_count;
    tsu_gizmo   gizmo;           /* viewport pick/drag state */
    RndBuffer   giz_vbuf[3];     /* one arrow mesh per axis (X/Y/Z), built once */
    RndBuffer   giz_ibuf[3];
    u32         giz_index_count[3];
    RndTexture  giz_tex[3];      /* solid axis colours (red/green/blue) */
    void       *scratch;         /* horu CSG working memory (>= HORU_CSG_SCRATCH) */
} EditorCold;

/* The per-frame view the editor needs to pick/drag in the viewport and place
   its panel grips: the camera basis + projection half-angle, plus the mouse and
   screen size. The game fills this from its own camera. */
typedef struct {
    tsu_v3 eye, fwd, right, up;
    float  tan_half_fov, aspect;
    float  mouse_x, mouse_y, screen_w, screen_h;
    int    mouse_left;
} henshu_view;

/* build the editor's cold GPU objects (grid texture, gizmo arrow meshes +
   colours, gizmo state). The game supplies the two pipelines and the CSG scratch
   region. Returns 0 if any GPU resource could not be created. */
int  henshu_cold_init(EditorCold *cold, void *scratch,
                      RndPipeline csg_pipeline, RndPipeline gizmo_pipeline);

/* register the scene + inspector panels with the engine UI, bound to `e` (the
   panel callbacks receive it as their user pointer). Call after ui_init (which
   wipes the panel registry). */
void henshu_register_panels(EditorState *e);

/* re-fold the shape list and re-upload the CSG GPU buffers. Call when
   e->csg_dirty (then clear it). */
void henshu_rebuild(EditorState *e, EditorCold *cold);

/* one frame of editor interaction: panel-edge resize + viewport pick/drag.
   Mutates `e` (panel widths, selection, shape transforms, dirty). */
void henshu_update(EditorState *e, EditorCold *cold, const henshu_view *v);

/* draw the CSG mesh + the selected shape's move gizmo (3D, needs the MVP). */
void henshu_draw_scene(const EditorState *e, const EditorCold *cold, mat4 mvp);

/* draw the resizable panel grips (2D overlay -- call after the UI panels so the
   grips sit on the panel boundary). */
void henshu_draw_overlay(const EditorState *e, const henshu_view *v);

#endif /* HENSHU_UI_H */
