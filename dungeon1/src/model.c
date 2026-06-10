#include "model.h"
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <string.h>
#include "cgltf.h"

/* Interleaved vertex layout matching the pipeline in gpu.c: 32 bytes. */
typedef struct {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
} Vertex;

/* glTF / cgltf matrices are column-major; our mat4 is row-major. */
static mat4 mat4_from_cgltf(const cgltf_float cm[16]) {
    mat4 r;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            r.m[row * 4 + col] = (float)cm[col * 4 + row];
        }
    }
    return r;
}

static const cgltf_accessor *find_attr(const cgltf_primitive *prim,
                                       cgltf_attribute_type type) {
    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        if (prim->attributes[i].type == type) {
            return prim->attributes[i].data;
        }
    }
    return NULL;
}

/* Uploads `data` into a new GPU buffer of the given usage via a transfer buffer. */
static SDL_GPUBuffer *upload_buffer(SDL_GPUDevice *device,
                                    SDL_GPUBufferUsageFlags usage,
                                    const void *data, Uint32 size) {
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device,
        &(SDL_GPUBufferCreateInfo){ .usage = usage, .size = size });
    if (!buffer) {
        return NULL;
    }
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size });
    if (!transfer) {
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUBuffer(copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer, .offset = 0 },
        &(SDL_GPUBufferRegion){ .buffer = buffer, .offset = 0, .size = size },
        false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return buffer;
}

/* Uploads an RGBA32 surface into a new sampled GPU texture. */
static SDL_GPUTexture *upload_texture(SDL_GPUDevice *device, SDL_Surface *rgba) {
    Uint32 w = (Uint32)rgba->w;
    Uint32 h = (Uint32)rgba->h;
    Uint32 size = w * h * 4;

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device,
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
        return NULL;
    }
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size });
    if (!transfer) {
        SDL_ReleaseGPUTexture(device, texture);
        return NULL;
    }
    Uint8 *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    for (Uint32 y = 0; y < h; y++) {
        memcpy(mapped + (size_t)y * w * 4,
               (Uint8 *)rgba->pixels + (size_t)y * rgba->pitch,
               (size_t)w * 4);
    }
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUTexture(copy,
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = transfer, .offset = 0,
            .pixels_per_row = w, .rows_per_layer = h },
        &(SDL_GPUTextureRegion){ .texture = texture, .w = w, .h = h, .d = 1 },
        false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return texture;
}

bool model_load(Model *model, SDL_GPUDevice *device,
                const char *glb_path, const char *texture_path) {
    memset(model, 0, sizeof(*model));

    cgltf_options options = { 0 };
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, glb_path, &data) != cgltf_result_success) {
        SDL_Log("cgltf: failed to parse '%s'", glb_path);
        return false;
    }
    if (cgltf_load_buffers(&options, data, glb_path) != cgltf_result_success) {
        SDL_Log("cgltf: failed to load buffers for '%s'", glb_path);
        cgltf_free(data);
        return false;
    }

    /* Pass 1: count vertices and indices across every node's mesh. */
    cgltf_size total_v = 0, total_i = 0;
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_node *node = &data->nodes[n];
        if (!node->mesh) {
            continue;
        }
        for (cgltf_size p = 0; p < node->mesh->primitives_count; p++) {
            const cgltf_primitive *prim = &node->mesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles) {
                continue;
            }
            const cgltf_accessor *pos = find_attr(prim, cgltf_attribute_type_position);
            if (!pos) {
                continue;
            }
            total_v += pos->count;
            total_i += prim->indices ? prim->indices->count : pos->count;
        }
    }
    if (total_v == 0 || total_i == 0) {
        SDL_Log("model '%s' contains no triangle geometry", glb_path);
        cgltf_free(data);
        return false;
    }

    Vertex *verts = malloc(total_v * sizeof(Vertex));
    Uint32 *indices = malloc(total_i * sizeof(Uint32));
    if (!verts || !indices) {
        SDL_Log("out of memory loading '%s'", glb_path);
        free(verts);
        free(indices);
        cgltf_free(data);
        return false;
    }

    /* Pass 2: bake each node's world transform into the vertex data. */
    Uint32 vcount = 0, icount = 0;
    vec3 bmin = vec3_make(1e30f, 1e30f, 1e30f);
    vec3 bmax = vec3_make(-1e30f, -1e30f, -1e30f);
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_node *node = &data->nodes[n];
        if (!node->mesh) {
            continue;
        }
        cgltf_float world_raw[16];
        cgltf_node_transform_world(node, world_raw);
        mat4 world = mat4_from_cgltf(world_raw);

        for (cgltf_size p = 0; p < node->mesh->primitives_count; p++) {
            const cgltf_primitive *prim = &node->mesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles) {
                continue;
            }
            const cgltf_accessor *pos = find_attr(prim, cgltf_attribute_type_position);
            if (!pos) {
                continue;
            }
            const cgltf_accessor *nrm = find_attr(prim, cgltf_attribute_type_normal);
            const cgltf_accessor *uv  = find_attr(prim, cgltf_attribute_type_texcoord);

            Uint32 base = vcount;
            for (cgltf_size i = 0; i < pos->count; i++) {
                float pf[3] = { 0, 0, 0 };
                float nf[3] = { 0, 1, 0 };
                float tf[2] = { 0, 0 };
                cgltf_accessor_read_float(pos, i, pf, 3);
                if (nrm) cgltf_accessor_read_float(nrm, i, nf, 3);
                if (uv)  cgltf_accessor_read_float(uv, i, tf, 2);

                vec3 wp = mat4_mul_point(world, vec3_make(pf[0], pf[1], pf[2]));
                vec3 wn = vec3_norm(mat4_mul_dir(world, vec3_make(nf[0], nf[1], nf[2])));

                Vertex *v = &verts[vcount++];
                v->px = wp.x; v->py = wp.y; v->pz = wp.z;
                v->nx = wn.x; v->ny = wn.y; v->nz = wn.z;
                v->u = tf[0]; v->v = tf[1];

                if (wp.x < bmin.x) bmin.x = wp.x;
                if (wp.y < bmin.y) bmin.y = wp.y;
                if (wp.z < bmin.z) bmin.z = wp.z;
                if (wp.x > bmax.x) bmax.x = wp.x;
                if (wp.y > bmax.y) bmax.y = wp.y;
                if (wp.z > bmax.z) bmax.z = wp.z;
            }
            if (prim->indices) {
                for (cgltf_size i = 0; i < prim->indices->count; i++) {
                    indices[icount++] =
                        base + (Uint32)cgltf_accessor_read_index(prim->indices, i);
                }
            } else {
                for (cgltf_size i = 0; i < pos->count; i++) {
                    indices[icount++] = base + (Uint32)i;
                }
            }
        }
    }
    cgltf_free(data);

    model->index_count = icount;
    model->bounds_min = bmin;
    model->bounds_max = bmax;
    model->vertex_buffer = upload_buffer(device, SDL_GPU_BUFFERUSAGE_VERTEX,
                                         verts, (Uint32)(vcount * sizeof(Vertex)));
    model->index_buffer = upload_buffer(device, SDL_GPU_BUFFERUSAGE_INDEX,
                                        indices, (Uint32)(icount * sizeof(Uint32)));
    free(verts);
    free(indices);
    if (!model->vertex_buffer || !model->index_buffer) {
        SDL_Log("failed to upload geometry buffers: %s", SDL_GetError());
        model_destroy(model, device);
        return false;
    }

    SDL_Surface *surface = IMG_Load(texture_path);
    if (!surface) {
        SDL_Log("failed to load texture '%s': %s", texture_path, SDL_GetError());
        model_destroy(model, device);
        return false;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!rgba) {
        SDL_Log("failed to convert texture '%s': %s", texture_path, SDL_GetError());
        model_destroy(model, device);
        return false;
    }
    model->texture = upload_texture(device, rgba);
    SDL_DestroySurface(rgba);
    if (!model->texture) {
        SDL_Log("failed to upload texture: %s", SDL_GetError());
        model_destroy(model, device);
        return false;
    }
    return true;
}

void model_destroy(Model *model, SDL_GPUDevice *device) {
    if (model->vertex_buffer) SDL_ReleaseGPUBuffer(device, model->vertex_buffer);
    if (model->index_buffer)  SDL_ReleaseGPUBuffer(device, model->index_buffer);
    if (model->texture)       SDL_ReleaseGPUTexture(device, model->texture);
    memset(model, 0, sizeof(*model));
}
