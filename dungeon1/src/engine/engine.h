#ifndef ENGINE_H
#define ENGINE_H

#include <export.h>

DECLARE_FUNC_VOID_pGAME_pCHAR(load_level);

DECLARE_FUNC_VOID_pGAME(init_engine);

DECLARE_FUNC_VOID_pGAME(update);

DECLARE_FUNC_VOID_pGAME(hotreloadable_imgui_draw)

#endif
