#ifndef RENDER_H
#define RENDER_H

/* Backend-agnostic render API. No SDL (or other backend) types appear here;
   handles are indices into backend-owned tables, never pointers, so they are
   safe by construction under hot-reload rules. The API mirrors only what the
   game actually uses today -- grow it on demand, it is not a GPU abstraction
   layer. Backends: render_sdlgpu.c (today), render_sokol.c (someday). */

#include "base/base_types.h"
#include "linalg.h"

typedef struct { u32 id; } RndBuffer;   /* 0 = invalid */
typedef struct { u32 id; } RndTexture;  /* 0 = invalid */
typedef struct { u32 id; } RndPipeline; /* 0 = invalid */

/* (Re)initializes the backend from the platform's opaque gpu context.
   Called on every dll reload; drops all previously created resources. */
b32 rnd_init(void *gpu_context);

RndBuffer   rnd_buffer_create_vertex(const void *data, u64 size);
RndBuffer   rnd_buffer_create_index(const void *data, u64 size);
RndTexture  rnd_texture_create_rgba8(const void *pixels, u32 w, u32 h);
RndPipeline rnd_pipeline_create(const char *vs_spv_path, const char *fs_spv_path);

/* Frame: begin clears color+depth. Returns false when nothing can be drawn
   this frame (minimized window) -- skip draw/end then. */
b32  rnd_frame_begin(f32 clear_r, f32 clear_g, f32 clear_b);
void rnd_draw_model(RndPipeline pipeline, RndBuffer vertices, RndBuffer indices,
                    RndTexture texture, u32 index_count, mat4 mvp, mat4 model);
void rnd_frame_end(void);

/* Last swapchain size seen by rnd_frame_begin (for aspect ratio). */
void rnd_swapchain_size(u32 *w, u32 *h);

/* 2D UI overlay, drawn after the 3D scene in a second (LOAD) pass during
   rnd_frame_end. Coordinates are pixels, y-down, origin top-left. Quads are
   batched; the batch breaks on texture or scissor change. texture.id == 0
   draws an untextured (solid color) quad. Colors are 0..1. Call only between
   rnd_frame_begin and rnd_frame_end. */
void rnd_ui_quad(f32 x, f32 y, f32 w, f32 h,
                 f32 u0, f32 v0, f32 u1, f32 v1,
                 RndTexture texture, f32 r, f32 g, f32 b, f32 a);
void rnd_ui_scissor(s32 x, s32 y, s32 w, s32 h);
void rnd_ui_scissor_clear(void);

#endif /* RENDER_H */
