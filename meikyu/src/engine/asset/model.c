#include "engine/asset/model.h"

#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "cgltf.h"
#include "stb_image.h"

/* Interleaved vertex layout matching the pipeline in render_sdlgpu.c: 32 bytes. */
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

/* Parses all triangle geometry (node world transforms baked in) into
   malloc'd arrays. Caller frees. */
static b32 parse_geometry(const char *glb_path,
                          Vertex **out_verts, u32 *out_vcount,
                          u32 **out_indices, u32 *out_icount,
                          vec3 *out_bmin, vec3 *out_bmax) {
    cgltf_options options = { 0 };
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, glb_path, &data) != cgltf_result_success) {
        SDL_Log("cgltf: failed to parse '%s'", glb_path);
        return 0;
    }
    if (cgltf_load_buffers(&options, data, glb_path) != cgltf_result_success) {
        SDL_Log("cgltf: failed to load buffers for '%s'", glb_path);
        cgltf_free(data);
        return 0;
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
        return 0;
    }

    Vertex *verts = malloc(total_v * sizeof(Vertex));
    u32 *indices = malloc(total_i * sizeof(u32));
    if (!verts || !indices) {
        SDL_Log("out of memory loading '%s'", glb_path);
        free(verts);
        free(indices);
        cgltf_free(data);
        return 0;
    }

    /* Pass 2: bake each node's world transform into the vertex data. */
    u32 vcount = 0, icount = 0;
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

            u32 base = vcount;
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
                        base + (u32)cgltf_accessor_read_index(prim->indices, i);
                }
            } else {
                for (cgltf_size i = 0; i < pos->count; i++) {
                    indices[icount++] = base + (u32)i;
                }
            }
        }
    }
    cgltf_free(data);

    *out_verts = verts;
    *out_vcount = vcount;
    *out_indices = indices;
    *out_icount = icount;
    *out_bmin = bmin;
    *out_bmax = bmax;
    return 1;
}

/* Uploads CPU-side model data through the render seam. */
static b32 upload_model(Model *model,
                        const Vertex *verts, u32 vcount,
                        const u32 *indices, u32 icount,
                        const void *pixels, u32 tex_w, u32 tex_h,
                        vec3 bmin, vec3 bmax) {
    memset(model, 0, sizeof(*model));
    model->index_count = icount;
    model->bounds_min = bmin;
    model->bounds_max = bmax;
    model->vertex_buffer = rnd_buffer_create_vertex(verts, (u64)vcount * sizeof(Vertex));
    model->index_buffer = rnd_buffer_create_index(indices, (u64)icount * sizeof(u32));
    if (!model->vertex_buffer.id || !model->index_buffer.id) {
        SDL_Log("failed to upload geometry buffers");
        return 0;
    }
    model->texture = rnd_texture_create_rgba8(pixels, tex_w, tex_h);
    if (!model->texture.id) {
        SDL_Log("failed to upload texture");
        return 0;
    }
    return 1;
}

b32 model_load(Model *model, const char *glb_path, const char *texture_path) {
    Vertex *verts;
    u32 *indices;
    u32 vcount, icount;
    vec3 bmin, bmax;
    if (!parse_geometry(glb_path, &verts, &vcount, &indices, &icount, &bmin, &bmax)) {
        return 0;
    }

    int w = 0, h = 0, channels = 0;
    unsigned char *pixels = stbi_load(texture_path, &w, &h, &channels, 4);
    if (!pixels) {
        SDL_Log("failed to load texture '%s': %s", texture_path, stbi_failure_reason());
        free(verts);
        free(indices);
        return 0;
    }

    b32 ok = upload_model(model, verts, vcount, indices, icount,
                          pixels, (u32)w, (u32)h, bmin, bmax);
    stbi_image_free(pixels);
    free(verts);
    free(indices);
    return ok;
}

/* ---- warm cache --------------------------------------------------------
   The transient block survives a hot reload even though cold state is
   rebuilt; this cache exploits that to skip the glb parse + png decode on
   every reload, leaving only the GPU upload (~ms). Layout:
   [ModelCacheHeader][Vertex * vertex_count][u32 * index_count][rgba pixels] */

#define MODEL_CACHE_MAGIC 0x4D444C43u /* 'MDLC' */

typedef struct {
    u32  magic;
    s64  glb_mtime;
    s64  tex_mtime;
    u32  vertex_count;
    u32  index_count;
    u32  tex_w, tex_h;
    vec3 bounds_min, bounds_max;
} ModelCacheHeader;

static s64 asset_mtime(const char *path) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) {
        return 0;
    }
    return (s64)info.modify_time;
}

b32 model_load_cached(Model *model, void *cache_mem, u64 cache_size,
                      const char *glb_path, const char *texture_path) {
    ModelCacheHeader *hdr = (ModelCacheHeader *)cache_mem;
    s64 glb_mtime = asset_mtime(glb_path);
    s64 tex_mtime = asset_mtime(texture_path);

    if (hdr->magic == MODEL_CACHE_MAGIC &&
        hdr->glb_mtime == glb_mtime && hdr->tex_mtime == tex_mtime) {
        u8 *p = (u8 *)cache_mem + sizeof(ModelCacheHeader);
        Vertex *verts = (Vertex *)p;
        u32 *indices = (u32 *)(p + (u64)hdr->vertex_count * sizeof(Vertex));
        u8 *pixels = (u8 *)indices + (u64)hdr->index_count * sizeof(u32);
        return upload_model(model, verts, hdr->vertex_count,
                            indices, hdr->index_count,
                            pixels, hdr->tex_w, hdr->tex_h,
                            hdr->bounds_min, hdr->bounds_max);
    }

    /* Cold path: parse + decode, populate the cache, then upload from it. */
    Vertex *verts;
    u32 *indices;
    u32 vcount, icount;
    vec3 bmin, bmax;
    if (!parse_geometry(glb_path, &verts, &vcount, &indices, &icount, &bmin, &bmax)) {
        return 0;
    }
    int w = 0, h = 0, channels = 0;
    unsigned char *pixels = stbi_load(texture_path, &w, &h, &channels, 4);
    if (!pixels) {
        SDL_Log("failed to load texture '%s': %s", texture_path, stbi_failure_reason());
        free(verts);
        free(indices);
        return 0;
    }

    u64 need = sizeof(ModelCacheHeader)
             + (u64)vcount * sizeof(Vertex)
             + (u64)icount * sizeof(u32)
             + (u64)w * (u64)h * 4;
    b32 ok;
    if (need > cache_size) {
        SDL_Log("model cache too small (%llu > %llu), loading uncached",
                (unsigned long long)need, (unsigned long long)cache_size);
        hdr->magic = 0;
        ok = upload_model(model, verts, vcount, indices, icount,
                          pixels, (u32)w, (u32)h, bmin, bmax);
    } else {
        u8 *p = (u8 *)cache_mem + sizeof(ModelCacheHeader);
        memcpy(p, verts, (u64)vcount * sizeof(Vertex));
        memcpy(p + (u64)vcount * sizeof(Vertex),
               indices, (u64)icount * sizeof(u32));
        memcpy(p + (u64)vcount * sizeof(Vertex) + (u64)icount * sizeof(u32),
               pixels, (u64)w * (u64)h * 4);
        hdr->glb_mtime = glb_mtime;
        hdr->tex_mtime = tex_mtime;
        hdr->vertex_count = vcount;
        hdr->index_count = icount;
        hdr->tex_w = (u32)w;
        hdr->tex_h = (u32)h;
        hdr->bounds_min = bmin;
        hdr->bounds_max = bmax;
        hdr->magic = MODEL_CACHE_MAGIC; /* last: valid only once data is in */
        ok = upload_model(model, verts, vcount, indices, icount,
                          pixels, (u32)w, (u32)h, bmin, bmax);
    }
    stbi_image_free(pixels);
    free(verts);
    free(indices);
    return ok;
}
