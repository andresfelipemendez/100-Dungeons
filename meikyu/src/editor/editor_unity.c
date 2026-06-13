/* Unity build for the editor: compiled to build/editor.o and linked into
   the reloadable unit only when EDITOR_BUILD is defined (kansi cfg /
   reload scripts). Ship builds never touch this file.

   seni's parser is compiled in here so the inspector can parse the
   embedded layout inside the dll -- a second copy of seni, independent of
   the one in the platform exe; they never cross the boundary. */

#include "../../../vendor/seni/seni.c"
#include "../../../vendor/seni/arena.c"

#include "editor.c"
#include "panels/inspector.c"
#include "panels/build_panel.c"
