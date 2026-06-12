/* Unity build for the dungeon1 game dll: engine first, then game code.
   reload.bat compiles only this file. */

#include "engine/engine_unity.c"

#include "seni_panel.c" /* seni's reload-question panel (engine ui widgets) */
#include "game.c"
