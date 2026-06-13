#ifndef MODEL_H
#define MODEL_H

#include "base/base_types.h"
#include "linalg.h"
#include "engine/render/render.h"

/* A single drawable mesh uploaded to the GPU: all primitives of all nodes in
   the glTF file are baked (with node world transforms applied) into one
   interleaved vertex buffer plus a 32-bit index buffer. Holds only render
   handles (indices), so a Model in cold state is reload-safe. */
typedef struct {
    RndBuffer  vertex_buffer;
    RndBuffer  index_buffer;
    RndTexture texture;
    u32        index_count;
    vec3       bounds_min;
    vec3       bounds_max;
} Model;

/* Loads geometry from `glb_path` and the color texture from `texture_path`
   through the render seam. Returns 0 (and logs the cause) on any failure. */
b32 model_load(Model *model, const char *glb_path, const char *texture_path);

/* Same, but keeps the parsed geometry + decoded pixels in `cache_mem`
   (caller-owned, e.g. a region of the transient block). On a hot reload the
   cache survives, so only the GPU upload reruns -- the glb parse and png
   decode are skipped. The cache self-validates (magic + source file mtimes);
   editing an asset on disk invalidates it. */
b32 model_load_cached(Model *model, void *cache_mem, u64 cache_size,
                      const char *glb_path, const char *texture_path);

#endif /* MODEL_H */
