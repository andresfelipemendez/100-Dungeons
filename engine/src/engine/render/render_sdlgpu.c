#include "engine/render/render.h"
#include "abi/abi_gpu.h"
#include "abi/abi_platform.h" /* PLATFORM_BUILD_DIR */

#include <SDL3/SDL.h>
#include <string.h>

/* SDL GPU backend. All state is dll-static: it is torn down and rebuilt on
   every reload (rnd_init), and handles handed out are table indices, so
   nothing here may ever be stored in hot (seni-migrated) memory. */

#define RND_MAX_BUFFERS   64
#define RND_MAX_TEXTURES  64
#define RND_MAX_PIPELINES 16

#define RND_UI_MAX_VERTS   (6 * 16384) /* 16k quads per frame */
#define RND_UI_MAX_BATCHES 256

typedef struct {
    f32 x, y, u, v;
    f32 r, g, b, a;
} RndUiVertex;

typedef struct {
    u32 first, count;   /* vertex range */
    u32 texture;        /* RndTexture id; 0 = white (untextured) */
    b32 scissor;        /* scissor active for this batch */
    s32 sc_x, sc_y, sc_w, sc_h;
} RndUiBatch;

typedef struct {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    SDL_GPUBuffer           *buffers[RND_MAX_BUFFERS];
    u32                      buffer_count;
    SDL_GPUTexture          *textures[RND_MAX_TEXTURES];
    u32                      texture_count;
    SDL_GPUGraphicsPipeline *pipelines[RND_MAX_PIPELINES];
    u32                      pipeline_count;

    SDL_GPUSampler *sampler;
    SDL_GPUTexture *depth_texture;
    u32             depth_w, depth_h;

    /* UI overlay */
    SDL_GPUGraphicsPipeline *ui_pipeline;
    SDL_GPUBuffer           *ui_vbuf;
    SDL_GPUTransferBuffer   *ui_transfer;
    RndTexture               white;
    RndUiVertex              ui_verts[RND_UI_MAX_VERTS];
    u32                      ui_vert_count;
    RndUiBatch               ui_batches[RND_UI_MAX_BATCHES];
    u32                      ui_batch_count;
    b32                      ui_scissor_on;
    s32                      ui_sc[4];

    /* in-flight frame */
    SDL_GPUCommandBuffer *cmd;
    SDL_GPURenderPass    *pass;
    SDL_GPUTexture       *swapchain;
    u32                   swap_w, swap_h;
} RndState;

static RndState rnd;

static void rnd_release_all(void) {
    if (!rnd.device) {
        return;
    }
    for (u32 i = 0; i < rnd.pipeline_count; i++) {
        if (rnd.pipelines[i]) SDL_ReleaseGPUGraphicsPipeline(rnd.device, rnd.pipelines[i]);
    }
    for (u32 i = 0; i < rnd.texture_count; i++) {
        if (rnd.textures[i]) SDL_ReleaseGPUTexture(rnd.device, rnd.textures[i]);
    }
    for (u32 i = 0; i < rnd.buffer_count; i++) {
        if (rnd.buffers[i]) SDL_ReleaseGPUBuffer(rnd.device, rnd.buffers[i]);
    }
    if (rnd.depth_texture) SDL_ReleaseGPUTexture(rnd.device, rnd.depth_texture);
    if (rnd.sampler)       SDL_ReleaseGPUSampler(rnd.device, rnd.sampler);
    if (rnd.ui_pipeline)   SDL_ReleaseGPUGraphicsPipeline(rnd.device, rnd.ui_pipeline);
    if (rnd.ui_vbuf)       SDL_ReleaseGPUBuffer(rnd.device, rnd.ui_vbuf);
    if (rnd.ui_transfer)   SDL_ReleaseGPUTransferBuffer(rnd.device, rnd.ui_transfer);
}

static b32 rnd_ui_setup(void);

b32 rnd_init(void *gpu_context) {
    GpuContext *ctx = (GpuContext *)gpu_context;

    /* A fresh dll has zeroed statics; after rnd_init within the same dll
       instance, release whatever the previous init created. */
    SDL_WaitForGPUIdle((SDL_GPUDevice *)ctx->device);
    rnd_release_all();
    memset(&rnd, 0, sizeof(rnd));

    rnd.window = (SDL_Window *)ctx->window;
    rnd.device = (SDL_GPUDevice *)ctx->device;

    rnd.sampler = SDL_CreateGPUSampler(rnd.device, &(SDL_GPUSamplerCreateInfo){
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    });
    if (!rnd.sampler) {
        SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
        return 0;
    }
    if (!rnd_ui_setup()) {
        return 0;
    }
    return 1;
}

/* Uploads `data` into a new GPU buffer of the given usage via a transfer buffer. */
static SDL_GPUBuffer *upload_buffer(SDL_GPUBufferUsageFlags usage,
                                    const void *data, u32 size) {
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(rnd.device,
        &(SDL_GPUBufferCreateInfo){ .usage = usage, .size = size });
    if (!buffer) {
        return NULL;
    }
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(rnd.device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size });
    if (!transfer) {
        SDL_ReleaseGPUBuffer(rnd.device, buffer);
        return NULL;
    }
    void *mapped = SDL_MapGPUTransferBuffer(rnd.device, transfer, false);
    memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(rnd.device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(rnd.device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUBuffer(copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer, .offset = 0 },
        &(SDL_GPUBufferRegion){ .buffer = buffer, .offset = 0, .size = size },
        false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(rnd.device, transfer);
    return buffer;
}

static RndBuffer buffer_register(SDL_GPUBuffer *b) {
    if (!b || rnd.buffer_count >= RND_MAX_BUFFERS) {
        if (b) SDL_ReleaseGPUBuffer(rnd.device, b);
        return (RndBuffer){ 0 };
    }
    rnd.buffers[rnd.buffer_count++] = b;
    return (RndBuffer){ rnd.buffer_count }; /* id = index + 1 */
}

RndBuffer rnd_buffer_create_vertex(const void *data, u64 size) {
    return buffer_register(upload_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, data, (u32)size));
}

RndBuffer rnd_buffer_create_index(const void *data, u64 size) {
    return buffer_register(upload_buffer(SDL_GPU_BUFFERUSAGE_INDEX, data, (u32)size));
}

RndTexture rnd_texture_create_rgba8(const void *pixels, u32 w, u32 h) {
    if (rnd.texture_count >= RND_MAX_TEXTURES) {
        return (RndTexture){ 0 };
    }
    u32 size = w * h * 4;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(rnd.device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = w,
            .height = h,
            .layer_count_or_depth = 1,
            .num_levels = 1,
        });
    if (!texture) {
        SDL_Log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        return (RndTexture){ 0 };
    }
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(rnd.device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size });
    if (!transfer) {
        SDL_ReleaseGPUTexture(rnd.device, texture);
        return (RndTexture){ 0 };
    }
    void *mapped = SDL_MapGPUTransferBuffer(rnd.device, transfer, false);
    memcpy(mapped, pixels, size);
    SDL_UnmapGPUTransferBuffer(rnd.device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(rnd.device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUTexture(copy,
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = transfer, .offset = 0,
            .pixels_per_row = w, .rows_per_layer = h },
        &(SDL_GPUTextureRegion){ .texture = texture, .w = w, .h = h, .d = 1 },
        false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(rnd.device, transfer);

    rnd.textures[rnd.texture_count++] = texture;
    return (RndTexture){ rnd.texture_count };
}

static SDL_GPUShader *load_shader_spv(const char *path, b32 is_vertex) {
    size_t size = 0;
    void *code = SDL_LoadFile(path, &size);
    if (!code) {
        SDL_Log("failed to read shader '%s': %s", path, SDL_GetError());
        return NULL;
    }
    SDL_GPUShader *shader = SDL_CreateGPUShader(rnd.device, &(SDL_GPUShaderCreateInfo){
        .code_size = size,
        .code = (const Uint8 *)code,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = is_vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = is_vertex ? 0u : 1u,
        .num_uniform_buffers = is_vertex ? 1u : 0u,
    });
    SDL_free(code);
    if (!shader) {
        SDL_Log("SDL_CreateGPUShader('%s') failed: %s", path, SDL_GetError());
    }
    return shader;
}

RndPipeline rnd_pipeline_create(const char *vs_spv_path, const char *fs_spv_path) {
    if (rnd.pipeline_count >= RND_MAX_PIPELINES) {
        return (RndPipeline){ 0 };
    }
    SDL_GPUShader *vs = load_shader_spv(vs_spv_path, 1);
    SDL_GPUShader *fs = load_shader_spv(fs_spv_path, 0);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(rnd.device, vs);
        if (fs) SDL_ReleaseGPUShader(rnd.device, fs);
        return (RndPipeline){ 0 };
    }

    SDL_GPUColorTargetDescription color_desc = {
        .format = SDL_GetGPUSwapchainTextureFormat(rnd.device, rnd.window),
    };
    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(float) * 8, /* pos3 + normal3 + uv2, interleaved */
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    SDL_GPUVertexAttribute attrs[3] = {
        { .location = 0, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 },
        { .location = 1, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = sizeof(float) * 3 },
        { .location = 2, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = sizeof(float) * 6 },
    };
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vb_desc,
            .num_vertex_buffers = 1,
            .vertex_attributes = attrs,
            .num_vertex_attributes = 3,
        },
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },
        .depth_stencil_state = {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .target_info = {
            .color_target_descriptions = &color_desc,
            .num_color_targets = 1,
            .has_depth_stencil_target = true,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(rnd.device, &info);
    SDL_ReleaseGPUShader(rnd.device, vs);
    SDL_ReleaseGPUShader(rnd.device, fs);
    if (!pipeline) {
        SDL_Log("SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
        return (RndPipeline){ 0 };
    }
    rnd.pipelines[rnd.pipeline_count++] = pipeline;
    return (RndPipeline){ rnd.pipeline_count };
}

/* Depth target must match the swapchain size; recreate it when that changes. */
static b32 ensure_depth(u32 w, u32 h) {
    if (rnd.depth_texture && rnd.depth_w == w && rnd.depth_h == h) {
        return 1;
    }
    if (rnd.depth_texture) {
        SDL_ReleaseGPUTexture(rnd.device, rnd.depth_texture);
    }
    rnd.depth_texture = SDL_CreateGPUTexture(rnd.device, &(SDL_GPUTextureCreateInfo){
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    });
    if (!rnd.depth_texture) {
        SDL_Log("SDL_CreateGPUTexture (depth) failed: %s", SDL_GetError());
        return 0;
    }
    rnd.depth_w = w;
    rnd.depth_h = h;
    return 1;
}

b32 rnd_frame_begin(f32 clear_r, f32 clear_g, f32 clear_b) {
    rnd.cmd = SDL_AcquireGPUCommandBuffer(rnd.device);
    if (!rnd.cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return 0;
    }
    SDL_GPUTexture *swapchain = NULL;
    Uint32 w = 0, h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(rnd.cmd, rnd.window, &swapchain, &w, &h) ||
        !swapchain || !ensure_depth(w, h)) {
        /* error or minimized window; nothing to draw this frame */
        SDL_SubmitGPUCommandBuffer(rnd.cmd);
        rnd.cmd = NULL;
        return 0;
    }
    rnd.swapchain = swapchain;
    rnd.swap_w = w;
    rnd.swap_h = h;
    rnd.ui_vert_count = 0;
    rnd.ui_batch_count = 0;
    rnd.ui_scissor_on = 0;

    SDL_GPUColorTargetInfo color = {
        .texture = swapchain,
        .clear_color = (SDL_FColor){ clear_r, clear_g, clear_b, 1.0f },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo depth = {
        .texture = rnd.depth_texture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = true,
    };
    rnd.pass = SDL_BeginGPURenderPass(rnd.cmd, &color, 1, &depth);
    return 1;
}

void rnd_draw_model(RndPipeline pipeline, RndBuffer vertices, RndBuffer indices,
                    RndTexture texture, u32 index_count, mat4 mvp, mat4 model) {
    if (!rnd.pass || !pipeline.id || !vertices.id || !indices.id || !texture.id) {
        return;
    }
    SDL_BindGPUGraphicsPipeline(rnd.pass, rnd.pipelines[pipeline.id - 1]);
    SDL_BindGPUVertexBuffers(rnd.pass, 0,
        &(SDL_GPUBufferBinding){ .buffer = rnd.buffers[vertices.id - 1], .offset = 0 }, 1);
    SDL_BindGPUIndexBuffer(rnd.pass,
        &(SDL_GPUBufferBinding){ .buffer = rnd.buffers[indices.id - 1], .offset = 0 },
        SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_BindGPUFragmentSamplers(rnd.pass, 0,
        &(SDL_GPUTextureSamplerBinding){
            .texture = rnd.textures[texture.id - 1], .sampler = rnd.sampler }, 1);

    struct { float mvp[16]; float model[16]; } ubo;
    memcpy(ubo.mvp, mvp.m, sizeof(ubo.mvp));
    memcpy(ubo.model, model.m, sizeof(ubo.model));
    SDL_PushGPUVertexUniformData(rnd.cmd, 0, &ubo, sizeof(ubo));

    SDL_DrawGPUIndexedPrimitives(rnd.pass, index_count, 1, 0, 0, 0);
}

static void rnd_ui_draw_pass(void) {
    if (!rnd.ui_vert_count || !rnd.ui_pipeline || !rnd.swapchain) {
        return;
    }

    /* Upload this frame's UI vertices (between the 3D and UI passes). */
    void *mapped = SDL_MapGPUTransferBuffer(rnd.device, rnd.ui_transfer, true);
    if (!mapped) {
        return;
    }
    memcpy(mapped, rnd.ui_verts, rnd.ui_vert_count * sizeof(RndUiVertex));
    SDL_UnmapGPUTransferBuffer(rnd.device, rnd.ui_transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(rnd.cmd);
    SDL_UploadToGPUBuffer(copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = rnd.ui_transfer, .offset = 0 },
        &(SDL_GPUBufferRegion){ .buffer = rnd.ui_vbuf, .offset = 0,
                                .size = rnd.ui_vert_count * sizeof(RndUiVertex) },
        true);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUColorTargetInfo color = {
        .texture = rnd.swapchain,
        .load_op = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(rnd.cmd, &color, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, rnd.ui_pipeline);
    SDL_BindGPUVertexBuffers(pass, 0,
        &(SDL_GPUBufferBinding){ .buffer = rnd.ui_vbuf, .offset = 0 }, 1);

    struct { f32 w, h; } screen = { (f32)rnd.swap_w, (f32)rnd.swap_h };
    SDL_PushGPUVertexUniformData(rnd.cmd, 0, &screen, sizeof(screen));

    for (u32 i = 0; i < rnd.ui_batch_count; i++) {
        RndUiBatch *b = &rnd.ui_batches[i];
        SDL_Rect sc = b->scissor
            ? (SDL_Rect){ b->sc_x, b->sc_y, b->sc_w, b->sc_h }
            : (SDL_Rect){ 0, 0, (int)rnd.swap_w, (int)rnd.swap_h };
        if (sc.x < 0) { sc.w += sc.x; sc.x = 0; }
        if (sc.y < 0) { sc.h += sc.y; sc.y = 0; }
        if (sc.x + sc.w > (int)rnd.swap_w) sc.w = (int)rnd.swap_w - sc.x;
        if (sc.y + sc.h > (int)rnd.swap_h) sc.h = (int)rnd.swap_h - sc.y;
        if (sc.w <= 0 || sc.h <= 0) {
            continue;
        }
        SDL_SetGPUScissor(pass, &sc);
        SDL_GPUTexture *tex = b->texture
            ? rnd.textures[b->texture - 1]
            : rnd.textures[rnd.white.id - 1];
        SDL_BindGPUFragmentSamplers(pass, 0,
            &(SDL_GPUTextureSamplerBinding){ .texture = tex, .sampler = rnd.sampler }, 1);
        SDL_DrawGPUPrimitives(pass, b->count, 1, b->first, 0);
    }
    SDL_EndGPURenderPass(pass);
}

void rnd_frame_end(void) {
    if (rnd.pass) {
        SDL_EndGPURenderPass(rnd.pass);
        rnd.pass = NULL;
    }
    if (rnd.cmd) {
        rnd_ui_draw_pass();
        SDL_SubmitGPUCommandBuffer(rnd.cmd);
        rnd.cmd = NULL;
    }
    rnd.swapchain = NULL;
}

void rnd_swapchain_size(u32 *w, u32 *h) {
    *w = rnd.swap_w ? rnd.swap_w : 1;
    *h = rnd.swap_h ? rnd.swap_h : 1;
}

/* ---- 2D UI overlay -------------------------------------------------- */

static b32 rnd_ui_setup(void) {
    u32 white_pixel = 0xFFFFFFFFu;
    rnd.white = rnd_texture_create_rgba8(&white_pixel, 1, 1);
    if (!rnd.white.id) {
        return 0;
    }

    SDL_GPUShader *vs = load_shader_spv(PLATFORM_BUILD_DIR "/ui.vert.spv", 1);
    SDL_GPUShader *fs = load_shader_spv(PLATFORM_BUILD_DIR "/ui.frag.spv", 0);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(rnd.device, vs);
        if (fs) SDL_ReleaseGPUShader(rnd.device, fs);
        return 0;
    }
    SDL_GPUColorTargetDescription color_desc = {
        .format = SDL_GetGPUSwapchainTextureFormat(rnd.device, rnd.window),
        .blend_state = {
            .enable_blend = true,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        },
    };
    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(RndUiVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    SDL_GPUVertexAttribute attrs[3] = {
        { .location = 0, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0 },
        { .location = 1, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = sizeof(f32) * 2 },
        { .location = 2, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = sizeof(f32) * 4 },
    };
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vb_desc,
            .num_vertex_buffers = 1,
            .vertex_attributes = attrs,
            .num_vertex_attributes = 3,
        },
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
        },
        .target_info = {
            .color_target_descriptions = &color_desc,
            .num_color_targets = 1,
        },
    };
    rnd.ui_pipeline = SDL_CreateGPUGraphicsPipeline(rnd.device, &info);
    SDL_ReleaseGPUShader(rnd.device, vs);
    SDL_ReleaseGPUShader(rnd.device, fs);
    if (!rnd.ui_pipeline) {
        SDL_Log("ui pipeline creation failed: %s", SDL_GetError());
        return 0;
    }

    rnd.ui_vbuf = SDL_CreateGPUBuffer(rnd.device, &(SDL_GPUBufferCreateInfo){
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = RND_UI_MAX_VERTS * sizeof(RndUiVertex) });
    rnd.ui_transfer = SDL_CreateGPUTransferBuffer(rnd.device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = RND_UI_MAX_VERTS * sizeof(RndUiVertex) });
    if (!rnd.ui_vbuf || !rnd.ui_transfer) {
        SDL_Log("ui buffer creation failed: %s", SDL_GetError());
        return 0;
    }
    return 1;
}

static RndUiBatch *rnd_ui_batch_for(u32 texture_id) {
    if (rnd.ui_batch_count) {
        RndUiBatch *b = &rnd.ui_batches[rnd.ui_batch_count - 1];
        b32 same_scissor =
            b->scissor == rnd.ui_scissor_on &&
            (!b->scissor || (b->sc_x == rnd.ui_sc[0] && b->sc_y == rnd.ui_sc[1] &&
                             b->sc_w == rnd.ui_sc[2] && b->sc_h == rnd.ui_sc[3]));
        if (b->texture == texture_id && same_scissor) {
            return b;
        }
    }
    if (rnd.ui_batch_count >= RND_UI_MAX_BATCHES) {
        return NULL;
    }
    RndUiBatch *b = &rnd.ui_batches[rnd.ui_batch_count++];
    b->first = rnd.ui_vert_count;
    b->count = 0;
    b->texture = texture_id;
    b->scissor = rnd.ui_scissor_on;
    b->sc_x = rnd.ui_sc[0]; b->sc_y = rnd.ui_sc[1];
    b->sc_w = rnd.ui_sc[2]; b->sc_h = rnd.ui_sc[3];
    return b;
}

void rnd_ui_quad(f32 x, f32 y, f32 w, f32 h,
                 f32 u0, f32 v0, f32 u1, f32 v1,
                 RndTexture texture, f32 r, f32 g, f32 b, f32 a) {
    if (rnd.ui_vert_count + 6 > RND_UI_MAX_VERTS) {
        return;
    }
    RndUiBatch *batch = rnd_ui_batch_for(texture.id);
    if (!batch) {
        return;
    }
    RndUiVertex *v = &rnd.ui_verts[rnd.ui_vert_count];
    RndUiVertex tl = { x,     y,     u0, v0, r, g, b, a };
    RndUiVertex tr = { x + w, y,     u1, v0, r, g, b, a };
    RndUiVertex bl = { x,     y + h, u0, v1, r, g, b, a };
    RndUiVertex br = { x + w, y + h, u1, v1, r, g, b, a };
    v[0] = tl; v[1] = bl; v[2] = br;
    v[3] = tl; v[4] = br; v[5] = tr;
    rnd.ui_vert_count += 6;
    batch->count += 6;
}

void rnd_ui_scissor(s32 x, s32 y, s32 w, s32 h) {
    rnd.ui_scissor_on = 1;
    rnd.ui_sc[0] = x; rnd.ui_sc[1] = y; rnd.ui_sc[2] = w; rnd.ui_sc[3] = h;
}

void rnd_ui_scissor_clear(void) {
    rnd.ui_scissor_on = 0;
}
