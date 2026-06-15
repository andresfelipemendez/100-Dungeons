/* henshu render/UI glue -- panels, gizmo, CSG mesh, grips. Compiled into the
   game dll (unity include); depends on the engine (render + ui + theme). */

#include "henshu_ui.h"
#include "engine/theme.h"

#include <math.h>

/* ---- editor layout / tuning -------------------------------------------- */
/* Panel widths are hot state (e->scene_w / e->inspector_w), draggable; these
   bound the drag so the viewport never collapses. */
#define PANEL_W_MIN    140.0f
#define PANEL_W_MAX    460.0f
#define SPLITTER_GRAB  5.0f    /* px band around a panel edge that grabs it */

#define HENSHU_MAX_POLYS 4096
#define HENSHU_MAX_VERTS 16384

/* mesh-build scratch -- file statics, never the stack (a fold can be thousands
   of polys / tens of thousands of verts). */
static horu_poly g_polys[HENSHU_MAX_POLYS];
static float     g_verts[HENSHU_MAX_VERTS * 8]; /* pos3, normal3, uv2 */
static u32       g_index[HENSHU_MAX_VERTS];

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static mat4 giz_translate_m(f32 x, f32 y, f32 z) {
    mat4 m = mat4_identity();
    m.m[3] = x; m.m[7] = y; m.m[11] = z; /* row-major translation column */
    return m;
}

/* Fan-triangulate polygons into the interleaved (pos,normal,uv) vertex buffer,
   flat-shaded (each vertex takes its face's plane normal). Non-indexed: index
   i = i. Returns the vertex/index count. */
static u32 build_buffers(const horu_poly *polys, int np,
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
                vp[6] = 0.5f;           vp[7] = 0.5f; /* flat texel from the tex */
                index[vn] = vn;
                vn++;
            }
        }
    }
    return vn;
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
   proper rotation (preserves winding/normals). */
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

/* one square-grid tile (RGBA8): a dark border band on a light cell. Sampled with
   REPEAT + triplanar UVs, it tiles into a grid across every CSG face. */
static RndTexture make_grid_texture(void) {
#define GRID_N    128
#define GRID_LINE 2   /* thin line band (2/128 of a tile) */
    static u32 px[GRID_N * GRID_N];
    int x, y;
    for (y = 0; y < GRID_N; y++) {
        for (x = 0; x < GRID_N; x++) {
            int edge = x < GRID_LINE || x >= GRID_N - GRID_LINE ||
                       y < GRID_LINE || y >= GRID_N - GRID_LINE;
            px[y * GRID_N + x] = edge ? THEME_U32(THEME_GRID_LINE)
                                      : THEME_U32(THEME_GRID_CELL);
        }
    }
    return rnd_texture_create_rgba8(px, GRID_N, GRID_N);
}

int henshu_cold_init(EditorCold *cold, void *scratch,
                     RndPipeline csg_pipeline, RndPipeline gizmo_pipeline) {
    u32 axis_col[3] = { THEME_U32(THEME_AXIS_X),
                        THEME_U32(THEME_AXIS_Y),
                        THEME_U32(THEME_AXIS_Z) };
    int a;
    cold->csg_pipeline = csg_pipeline;
    cold->gizmo_pipeline = gizmo_pipeline;
    cold->scratch = scratch;
    cold->mesh_vbuf.id = 0;
    cold->mesh_ibuf.id = 0;
    cold->mesh_index_count = 0;
    cold->grid_tex = make_grid_texture();
    if (!cold->grid_tex.id) {
        return 0;
    }
    tsu_gizmo_init(&cold->gizmo);
    for (a = 0; a < 3; a++) { /* one arrow mesh + solid colour texture per axis */
        horu_poly arr[128];
        int na = gizmo_arrow_y(arr, 128);
        int i, np = 0;
        u32 nv;
        for (i = 0; i < na; i++) {
            g_polys[np++] = gizmo_xform(arr[i], a);
        }
        nv = build_buffers(g_polys, np, g_verts, HENSHU_MAX_VERTS, g_index);
        cold->giz_vbuf[a] = rnd_buffer_create_vertex(g_verts, (u64)nv * 8 * sizeof(float));
        cold->giz_ibuf[a] = rnd_buffer_create_index(g_index, (u64)nv * sizeof(u32));
        cold->giz_index_count[a] = nv;
        cold->giz_tex[a] = rnd_texture_create_rgba8(&axis_col[a], 1, 1);
        if (!cold->giz_vbuf[a].id || !cold->giz_ibuf[a].id || !cold->giz_tex[a].id) {
            return 0; /* out of render slots -- fail loudly, don't draw garbage */
        }
    }
    return 1;
}

void henshu_rebuild(EditorState *e, EditorCold *cold) {
    int np = henshu_eval_all(e, g_polys, HENSHU_MAX_POLYS, cold->scratch);
    u32 nv = build_buffers(g_polys, np, g_verts, HENSHU_MAX_VERTS, g_index);
    /* free the previous mesh's buffers -- the editor re-meshes on every edit, so
       without this the fixed buffer table fills up and the next draw faults. */
    rnd_buffer_destroy(cold->mesh_vbuf);
    rnd_buffer_destroy(cold->mesh_ibuf);
    cold->mesh_vbuf.id = 0;
    cold->mesh_ibuf.id = 0;
    cold->mesh_index_count = 0;
    /* an empty fold (e.g. intersecting disjoint shapes) yields no geometry -- a
       0-byte GPU buffer is illegal, so skip the upload and draw nothing. */
    if (nv > 0) {
        cold->mesh_vbuf = rnd_buffer_create_vertex(g_verts, (u64)nv * 8 * sizeof(float));
        cold->mesh_ibuf = rnd_buffer_create_index(g_index, (u64)nv * sizeof(u32));
        cold->mesh_index_count = nv;
    }
}

/* ---- panels ------------------------------------------------------------- */

/* LEFT panel: an "add shape" button + a selectable list of every shape (kind,
   and its fold op for non-base slots). Clicking a row selects that entity. */
static void scene_panel(void *user) {
    EditorState *e = (EditorState *)user;
    static char rid[HENSHU_MAX_SHAPES][8];   /* ids + labels must outlive ui_frame_end */
    static char rlab[HENSHU_MAX_SHAPES][40];
    int i;
    ui_panel_begin(ITO("scene"), e->scene_w);
    ui_label(ITO("SCENE"), 18);
    if (ui_button(ITO("add"), ITO("+ add shape"))) {
        henshu_add(e);
    }
    ui_label_dim(ITO("entities"), 13);
    for (i = 0; i < e->csg_count && i < HENSHU_MAX_SHAPES; i++) {
        ito id  = ito_format(rid[i], sizeof(rid[i]), "r%d", i);
        ito lab = (i == 0)
            ? ito_format(rlab[i], sizeof(rlab[i]), "%d  %s", i,
                         henshu_kind_name(e->csg_kind[i]))
            : ito_format(rlab[i], sizeof(rlab[i]), "%d  %s  %s", i,
                         henshu_op_name(e->csg_op[i]), henshu_kind_name(e->csg_kind[i]));
        if (ui_select_row(id, lab, i == e->csg_selected)) {
            e->csg_selected = i;
        }
    }
    ui_panel_end();
}

/* one editable axis row: "name value" label + [-][+] nudging *val by step
   (clamped to >= minv). buf must be frame-static and unique per row. */
static void inspector_axis(EditorState *e, ito rowid, ito minus, ito plus,
                           const char *name, char *buf, size_t buflen,
                           f32 *val, f32 step, f32 minv) {
    ito lab = ito_format(buf, buflen, "%s %.2f", name, *val);
    ui_row_begin(rowid);
        ui_label(lab, 14);
        if (ui_button(minus, ITO("-"))) {
            *val -= step;
            if (*val < minv) *val = minv;
            e->csg_dirty = 1;
        }
        if (ui_button(plus, ITO("+"))) {
            *val += step;
            e->csg_dirty = 1;
        }
    ui_row_end();
}

/* RIGHT panel: the selected shape's fields -- its primitive type, its fold op
   (the base shape has none), and editable position + size steppers. */
static void inspector_panel(void *user) {
    EditorState *e = (EditorState *)user;
    static char hbuf[48];
    static char bx[24], by[24], bz[24], bsx[24], bsy[24], bsz[24];
    int s = e->csg_selected;
    ui_panel_begin_right(ITO("inspector"), e->inspector_w);
    ui_label(ITO("INSPECTOR"), 18);
    if (s < 0 || s >= e->csg_count) {
        ui_label_dim(ITO("no selection"), 14);
        ui_panel_end();
        return;
    }
    {
        ito h = ito_format(hbuf, sizeof(hbuf), "#%d  %s", s,
                           henshu_kind_name(e->csg_kind[s]));
        ui_label(h, 15);
    }
    ui_label_dim(ITO("shape"), 13);
    ui_row_begin(ITO("kr"));
        if (ui_button(ITO("kb"), ITO("box")))  henshu_set_kind(e, HENSHU_BOX);
        if (ui_button(ITO("ks"), ITO("sph")))  henshu_set_kind(e, HENSHU_SPHERE);
        if (ui_button(ITO("kc"), ITO("cyl")))  henshu_set_kind(e, HENSHU_CYL);
        if (ui_button(ITO("kp"), ITO("ply")))  henshu_set_kind(e, HENSHU_POLY);
    ui_row_end();
    if (s == 0) {
        ui_label_dim(ITO("base shape (no op)"), 12);
    } else {
        ui_label_dim(ITO("operation"), 13);
        ui_row_begin(ITO("opr"));
            if (ui_button(ITO("ou"), ITO("union"))) henshu_set_op(e, HENSHU_UNION);
            if (ui_button(ITO("od"), ITO("diff")))  henshu_set_op(e, HENSHU_DIFF);
            if (ui_button(ITO("oi"), ITO("isect"))) henshu_set_op(e, HENSHU_ISECT);
        ui_row_end();
    }
    ui_label_dim(ITO("position"), 13);
    inspector_axis(e, ITO("px"), ITO("pxm"), ITO("pxp"), "x", bx, sizeof(bx),
                   &e->csg_x[s], 0.25f, -1e9f);
    inspector_axis(e, ITO("py"), ITO("pym"), ITO("pyp"), "y", by, sizeof(by),
                   &e->csg_y[s], 0.25f, -1e9f);
    inspector_axis(e, ITO("pz"), ITO("pzm"), ITO("pzp"), "z", bz, sizeof(bz),
                   &e->csg_z[s], 0.25f, -1e9f);
    ui_label_dim(ITO("size"), 13);
    inspector_axis(e, ITO("sx"), ITO("sxm"), ITO("sxp"), "x", bsx, sizeof(bsx),
                   &e->csg_sx[s], 0.25f, 0.05f);
    inspector_axis(e, ITO("sy"), ITO("sym"), ITO("syp"), "y", bsy, sizeof(bsy),
                   &e->csg_sy[s], 0.25f, 0.05f);
    inspector_axis(e, ITO("sz"), ITO("szm"), ITO("szp"), "z", bsz, sizeof(bsz),
                   &e->csg_sz[s], 0.25f, 0.05f);
    ui_panel_end();
}

void henshu_register_panels(EditorState *e) {
    ui_panel_register(ITO("scene"), scene_panel, e);
    ui_panel_register(ITO("inspector"), inspector_panel, e);
}

/* ---- per-frame interaction --------------------------------------------- */

void henshu_update(EditorState *e, EditorCold *cold, const henshu_view *v) {
    /* resizable panels: grab a panel's inner edge on press, drag to set width.
       the scene edge sits at scene_w + left_pad so a host panel stacked after the
       scene panel does not bury the grip. */
    f32 mx = v->mouse_x;
    f32 scene_edge = e->scene_w + v->left_pad;
    f32 insp_edge = v->screen_w - e->inspector_w;
    int press = v->mouse_left && !e->prev_mouse_left;
    if (!v->mouse_left) {
        e->ui_drag = 0;
    } else if (press) {
        if (fabsf(mx - scene_edge) <= SPLITTER_GRAB)      e->ui_drag = 1;
        else if (fabsf(mx - insp_edge) <= SPLITTER_GRAB)  e->ui_drag = 2;
    }
    if (e->ui_drag == 1) {
        e->scene_w = clampf(mx - v->left_pad, PANEL_W_MIN, PANEL_W_MAX);
    } else if (e->ui_drag == 2) {
        e->inspector_w = clampf(v->screen_w - mx, PANEL_W_MIN, PANEL_W_MAX);
    }
    e->prev_mouse_left = v->mouse_left;

    /* viewport pick + drag via tsumami. Gated to the right of the editor panels
       so button clicks do not pick. Skipped entirely when the host disables the
       viewport gizmo (it owns a single gizmo elsewhere). */
    if (v->viewport_gizmo) {
        tsu_ray ray;
        tsu_target targets[HENSHU_MAX_SHAPES * 4];
        int nt, mouse, out_id = -1;
        tsu_v3 mv;
        ray = tsu_ray_from_screen(v->eye, v->fwd, v->right, v->up,
                                  v->tan_half_fov, v->aspect,
                                  v->mouse_x, v->mouse_y, v->screen_w, v->screen_h);
        nt = henshu_targets(e, targets, HENSHU_MAX_SHAPES * 4);
        /* the targets come back in the solid's LOCAL space; shift them to world
           by the solid's origin so the ray (world) picks correctly. */
        {
            int q;
            for (q = 0; q < nt; q++) {
                targets[q].center.x += v->origin.x; targets[q].center.y += v->origin.y; targets[q].center.z += v->origin.z;
                targets[q].origin.x += v->origin.x; targets[q].origin.y += v->origin.y; targets[q].origin.z += v->origin.z;
            }
        }
        mouse = (v->mouse_left && e->ui_drag == 0 &&
                 v->mouse_x > e->scene_w + v->left_pad &&
                 v->mouse_x < v->screen_w - e->inspector_w) ? 1 : 0;
        if (tsu_gizmo_update(&cold->gizmo, ray, mouse, v->fwd, targets, nt, &out_id, &mv)) {
            int node = out_id / 4; /* id = node*4 + slot */
            e->csg_x[node] = mv.x - v->origin.x; /* world -> local */
            e->csg_y[node] = mv.y - v->origin.y;
            e->csg_z[node] = mv.z - v->origin.z;
            e->csg_dirty = 1;
        }
        /* only let a viewport pick change the selection while the mouse is
           actually driving the gizmo -- otherwise the gizmo's stale `selected`
           overwrites a choice made in the scene tree every frame. */
        if (mouse && cold->gizmo.selected >= 0) {
            e->csg_selected = cold->gizmo.selected / 4;
        }
    }
}

void henshu_draw_scene(const EditorState *e, const EditorCold *cold, mat4 mvp, int draw_gizmo) {
    /* the CSG result mesh, shaded with the triplanar grid (no UVs needed) */
    if (cold->mesh_index_count > 0 && cold->mesh_vbuf.id && cold->mesh_ibuf.id) {
        rnd_draw_model(cold->csg_pipeline, cold->mesh_vbuf, cold->mesh_ibuf,
                       cold->grid_tex, cold->mesh_index_count, mvp, mat4_identity());
    }
    /* the translate gizmo at the selected shape: one solid-coloured arrow per
       axis, on the plain pipeline (no triplanar grid). off when the host owns
       the only gizmo. */
    if (draw_gizmo && e->csg_selected >= 0 && e->csg_selected < e->csg_count) {
        int s = e->csg_selected, a;
        mat4 t = giz_translate_m(e->csg_x[s], e->csg_y[s], e->csg_z[s]);
        mat4 gmvp = mat4_mul(mvp, t);
        for (a = 0; a < 3; a++) {
            if (cold->giz_index_count[a] > 0) {
                rnd_draw_model(cold->gizmo_pipeline, cold->giz_vbuf[a], cold->giz_ibuf[a],
                               cold->giz_tex[a], cold->giz_index_count[a], gmvp, t);
            }
        }
    }
}

/* a full-height resize grip bar centred on x; teal when hot (hovered/dragging),
   subtle slate otherwise. */
static void draw_grip(f32 x, f32 h, int hot) {
    RndTexture notex;
    f32 gw = 4.0f, gx = x - gw * 0.5f;
    notex.id = 0;
    if (hot) {
        rnd_ui_quad(gx, 0.0f, gw, h, 0, 0, 1, 1, notex, THEME_GRIP_HOT, 0.95f);
    } else {
        rnd_ui_quad(gx, 0.0f, gw, h, 0, 0, 1, 1, notex, THEME_GRIP_IDLE, 0.55f);
    }
}

void henshu_draw_overlay(const EditorState *e, const henshu_view *v) {
    f32 mx = v->mouse_x, sh = v->screen_h;
    f32 scene_edge = e->scene_w + v->left_pad;
    f32 insp_edge = v->screen_w - e->inspector_w;
    int scene_hot = e->ui_drag == 1 || fabsf(mx - scene_edge) <= SPLITTER_GRAB;
    int insp_hot  = e->ui_drag == 2 || fabsf(mx - insp_edge) <= SPLITTER_GRAB;
    draw_grip(scene_edge, sh, scene_hot);
    draw_grip(insp_edge, sh, insp_hot);
}
