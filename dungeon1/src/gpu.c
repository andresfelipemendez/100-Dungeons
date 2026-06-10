#include "gpu.h"
#include "shaders.h"
#include <string.h>

bool gpu_init(Gpu *g, SDL_Window *window) {
    memset(g, 0, sizeof(*g));
    g->window = window;

    g->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB, true, NULL);
    if (!g->device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(g->device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUShader *vs = shader_compile(g->device, true);
    SDL_GPUShader *fs = shader_compile(g->device, false);
    if (!vs || !fs) {
        return false;
    }

    SDL_GPUColorTargetDescription color_desc = {
        .format = SDL_GetGPUSwapchainTextureFormat(g->device, window),
    };
    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(float) * 8,
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
    g->pipeline = SDL_CreateGPUGraphicsPipeline(g->device, &info);
    SDL_ReleaseGPUShader(g->device, vs);
    SDL_ReleaseGPUShader(g->device, fs);
    if (!g->pipeline) {
        SDL_Log("SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
        return false;
    }

    g->sampler = SDL_CreateGPUSampler(g->device, &(SDL_GPUSamplerCreateInfo){
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    });
    if (!g->sampler) {
        SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void gpu_shutdown(Gpu *g) {
    if (!g->device) {
        return;
    }
    if (g->depth_texture) SDL_ReleaseGPUTexture(g->device, g->depth_texture);
    if (g->sampler)       SDL_ReleaseGPUSampler(g->device, g->sampler);
    if (g->pipeline)      SDL_ReleaseGPUGraphicsPipeline(g->device, g->pipeline);
    if (g->window)        SDL_ReleaseWindowFromGPUDevice(g->device, g->window);
    SDL_DestroyGPUDevice(g->device);
    memset(g, 0, sizeof(*g));
}

/* Depth target must match the swapchain size; recreate it when that changes. */
static bool ensure_depth(Gpu *g, Uint32 w, Uint32 h) {
    if (g->depth_texture && g->depth_w == w && g->depth_h == h) {
        return true;
    }
    if (g->depth_texture) {
        SDL_ReleaseGPUTexture(g->device, g->depth_texture);
    }
    g->depth_texture = SDL_CreateGPUTexture(g->device, &(SDL_GPUTextureCreateInfo){
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    });
    if (!g->depth_texture) {
        SDL_Log("SDL_CreateGPUTexture (depth) failed: %s", SDL_GetError());
        return false;
    }
    g->depth_w = w;
    g->depth_h = h;
    return true;
}

bool gpu_draw(Gpu *g, const Model *model, mat4 viewproj) {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g->device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTexture *swapchain = NULL;
    Uint32 w = 0, h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, g->window, &swapchain, &w, &h)) {
        SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }
    if (!swapchain) {
        /* Window is minimized; nothing to draw this frame. */
        SDL_SubmitGPUCommandBuffer(cmd);
        return true;
    }
    if (!ensure_depth(g, w, h)) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return false;
    }

    SDL_GPUColorTargetInfo color = {
        .texture = swapchain,
        .clear_color = (SDL_FColor){ 0.07f, 0.08f, 0.10f, 1.0f },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo depth = {
        .texture = g->depth_texture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = true,
    };

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, &depth);
    SDL_BindGPUGraphicsPipeline(pass, g->pipeline);
    SDL_BindGPUVertexBuffers(pass, 0,
        &(SDL_GPUBufferBinding){ .buffer = model->vertex_buffer, .offset = 0 }, 1);
    SDL_BindGPUIndexBuffer(pass,
        &(SDL_GPUBufferBinding){ .buffer = model->index_buffer, .offset = 0 },
        SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_BindGPUFragmentSamplers(pass, 0,
        &(SDL_GPUTextureSamplerBinding){ .texture = model->texture, .sampler = g->sampler }, 1);

    struct { float mvp[16]; float model[16]; } ubo;
    mat4 identity = mat4_identity();
    memcpy(ubo.mvp, viewproj.m, sizeof(ubo.mvp));
    memcpy(ubo.model, identity.m, sizeof(ubo.model));
    SDL_PushGPUVertexUniformData(cmd, 0, &ubo, sizeof(ubo));

    SDL_DrawGPUIndexedPrimitives(pass, model->index_count, 1, 0, 0, 0);
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    return true;
}
