/* Unity build for the engine half of the reloadable dll. A game's unity
   file includes this, then its own game code -- one compiler invocation
   produces the whole reloadable unit.

   Vendor implementations (cgltf, stb, clay) are NOT here: they live in
   vendor_impl.c, compiled once to build/vendor_impl.o by the full build and
   linked into every dll rebuild (kansi `obj` / reload script). */

/* NOTE: engine/ui/ui.c is NOT here -- it is the one engine file that speaks
   clay (C99 macro API) and is compiled into its own object (build/ui.o),
   rebuilt by kansi only when it changes. */
#include "engine/render/render_sdlgpu.c"
#include "engine/asset/model.c"
