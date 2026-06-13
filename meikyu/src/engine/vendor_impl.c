/* Vendor library implementations, compiled ONCE into build/vendor_impl.o by
   the full build (build.bat) and linked into every dll rebuild. These never
   change during iteration; keeping them out of the unity translation unit
   cuts the per-save compile time roughly in half. The declarations are still
   included normally by engine/game code -- all symbols here have external
   linkage. */

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"
