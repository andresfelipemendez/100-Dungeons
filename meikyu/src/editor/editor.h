#ifndef EDITOR_H
#define EDITOR_H

/* The editor: dev-only tooling compiled into the reloadable unit when
   EDITOR_BUILD is defined (its own object, build/editor.o -- the unity
   build never includes editor sources). NOTHING under src/editor ships:
   the ship build links runtime_main + engine + game and this header is
   never seen. src/engine must never include src/editor -- if a diff
   shows it, the architecture has been violated.

   The editor owns the UI frame: the game calls editor_frame once per frame
   between its 3D draw and rnd_frame_end, and the editor builds every panel
   (its own + anything in ui_panel_register's registry, e.g. seni's reload
   questions). It edits the game's hot state WITHOUT including game headers:
   it parses the layout string the dll already embeds for seni
   (`seni_layout`), so the inspector derives itself from game_state.h. */

#include "base/base_types.h"
#include "abi/abi_platform.h"

/* Call from the game's cold-rebuild path, after ui_init. Parses the
   embedded hot-state layout for the inspector. */
b32 editor_init(PlatformMemory *memory);

/* Builds the whole editor UI for this frame. Call between the game's 3D
   draws and rnd_frame_end. */
void editor_frame(PlatformMemory *memory, PlatformApi *api, GameInput *input,
                  f32 screen_w, f32 screen_h);

#endif /* EDITOR_H */
