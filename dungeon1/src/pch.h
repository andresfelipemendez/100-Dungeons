/* Precompiled header for the game dll build: the big, never-changing vendor
   headers. kansi snapshots this to build/pch.h, precompiles it to
   build/pch.h.gch, and force-includes it (-include) into game_unity.c --
   header parsing was ~60% of compile time. The .gch is flag-sensitive: it
   must be produced with EXACTLY the same flags + include dirs as the dll
   compile (kansi.cfg keeps them in sync). Do not include project headers
   here; they change too often. */

#include <SDL3/SDL.h>
#include "cgltf.h"
#include "stb_image.h"
