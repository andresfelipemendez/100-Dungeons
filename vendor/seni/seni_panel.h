#ifndef SENI_PANEL_H
#define SENI_PANEL_H

/* Engine UI panel showing pending reload disambiguations (the questions
   seni's diff raises when a layout change is ambiguous -- see
   seni_annotations.h). This is the one seni file that depends on the
   engine: it draws through engine/ui/ui.h widgets and reads the
   SeniReloadStatus the platform parks in PlatformMemory.

   Compiled into the game dll (strict c89, unity-buildable). Register from
   the game's cold-rebuild path, after ui_init:

       seni_panel_register(&memory->seni);

   The panel draws nothing while no questions are pending. When the
   platform refuses a reload, it fills memory->seni and the panel lists
   each question with its two annotation answers; fixing the header
   triggers the rebuild that retries the reload and clears the panel. */

#include "abi/abi_platform.h"

b32 seni_panel_register(SeniReloadStatus *status);

#endif
