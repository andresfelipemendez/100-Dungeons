/* Build panel: runs build profiles through the platform interface. The
   editor never spawns processes itself -- which script, which shell, which
   log file are all platform knowledge behind api->run_build_profile (and
   the kaji job runner under it). Profiles are scripts under editor/build/.
   v1: the ship profile, which packages runtime_main + engine + game
   statically into the ship dir with no editor, kansi, or seni inside. */

#include "editor.h"
#include "engine/ui/ui.h"

void build_panel_draw(PlatformApi *api) {
    ui_label_dim("build", 14);
    ui_row_begin("bld_ship_row");
    if (ui_button("bld_ship", "S")) {
        if (api->run_build_profile) {
            api->run_build_profile("ship");
        } else {
            api->log("editor: host has no build support");
        }
    }
    ui_label("ship build", 14);
    if (api->build_status) {
        switch (api->build_status()) {
        case 1:  ui_label_dim("building...", 14); break;
        case 2:  ui_label_dim("failed (see log)", 14); break;
        default: break;
        }
    }
    ui_row_end();
}
