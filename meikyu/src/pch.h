/* Precompiled header for the game dll build: the big, never-changing vendor
   headers. The generated kaji config snapshots this to <build>/pch.h,
   precompiles it to <build>/pch.h.gch, and force-includes it (-include)
   into the generated unity file -- header parsing was ~60% of compile
   time. The .gch is flag-sensitive: the generated pch target uses EXACTLY
   the dll compile's flags + include dirs. Never include project headers
   here; they change too often. */

#include <SDL3/SDL.h>
#include "cgltf.h"
#include "stb_image.h"
