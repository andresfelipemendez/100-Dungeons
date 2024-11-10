#ifndef ENGINE_H
#define ENGINE_H

#include <export.h>

DECLARE_FUNC_VOID_pGAME_pCHAR(load_level)

extern "C" __declspec(dllexport) __stdcall void init_engine(game *g);

DECLARE_FUNC_VOID_pGAME(hotreloadable_imgui_draw)

#endif
