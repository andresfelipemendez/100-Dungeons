#ifndef BUILD_MANIFEST_H
#define BUILD_MANIFEST_H

/* Single source of truth for the engine-exe build + per-OS facts. Parses
   meikyu/build.manifest into a BuildPlatform for the running OS. Pure core:
   depends only on ito + the arena, does no filesystem or OS detection (probing
   is injected), so it is unit-testable without a window or real files.

   All BuildPlatform / BuildManifest string fields are ito VIEWS into the
   manifest text the caller arena-copied before calling build_manifest_parse;
   they stay valid as long as that arena does. (michi was avoided here on
   purpose: michi_view points into a michi_buf's storage, which would dangle
   once a local row buffer went out of scope.)

   See docs/superpowers/specs/2026-06-12-build-manifest-platform-registry-design.md */

#include "base/base_types.h"
#include "../../lib/seni/arena.h"
#include "../../lib/ito/ito.h"

#define BM_MAX_HOST_SRC      32
#define BM_MAX_INCLUDE_ROOT  16
#define BM_MAX_GLSLC         8
#define BM_MAX_PLATFORM      8

/* The selected per-OS row: views into the parsed manifest text. The resolved
   shader compiler is NOT here -- it is written to a caller buffer by
   build_manifest_resolve_glslc, since it is composed, not a manifest substring. */
typedef struct {
    ito builddir;      /* "build-mac" / "build-linux" / "build" */
    ito exe_suffix;    /* "" / ".exe" */
    ito dll_suffix;    /* ".so" / ".dll" */
    ito dodai_src;     /* "dodai_posix.c" / "dodai_windows.c" (under lib/dodai) */
    ito rpath;         /* "@loader_path" / "$ORIGIN" / "" */
    ito sdl_strategy;  /* "fetch" / "mingw-prebuilt" */
} BuildPlatform;

/* The whole parsed manifest. Host build lists are OS-independent, so they live
   here, not in BuildPlatform. */
typedef struct {
    ito host_src[BM_MAX_HOST_SRC];          int host_src_count;
    ito include_root[BM_MAX_INCLUDE_ROOT];  int include_root_count;
    ito sdl_version;
    ito vulkan_pin;
    ito glslc_candidate[BM_MAX_GLSLC];      int glslc_count;
    ito cc_candidate[BM_MAX_GLSLC];         int cc_count;
    struct { ito name, builddir, exe, dll, dodai, rpath, sdl; } platform[BM_MAX_PLATFORM];
    int platform_count;
} BuildManifest;

/* Parse manifest text (arena-copied by the caller). Returns 0 and sets *err
   (arena-allocated) on malformed input: unknown key, bad platform row,
   capacity exceeded, no platform rows. */
b32 build_manifest_parse(arena *a, ito text, BuildManifest *out, ito *err);

/* Pick the row named `os` ("mac"/"linux"/"windows") into *out. Returns 0 and
   sets *err (a static message) if no such row. */
b32 build_manifest_select(const BuildManifest *m, ito os, BuildPlatform *out, ito *err);

/* Resolve glslc from the candidate list: expand a leading $VAR via getenv_fn,
   take the first path exists_fn() accepts, write it to out_buf. Returns 0 if
   none exist (caller then tries bare 'glslc' / refuses). getenv_fn/exists_fn
   are injected for testing. */
b32 build_manifest_resolve_glslc(const BuildManifest *m,
                                 const char *(*getenv_fn)(const char *name),
                                 b32 (*exists_fn)(const char *path),
                                 char *out_buf, size_t cap);

/* Same, for the C compiler (cc_candidate list). */
b32 build_manifest_resolve_cc(const BuildManifest *m,
                              const char *(*getenv_fn)(const char *name),
                              b32 (*exists_fn)(const char *path),
                              char *out_buf, size_t cap);

#endif /* BUILD_MANIFEST_H */
