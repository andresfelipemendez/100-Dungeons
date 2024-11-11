#ifndef CORE_H
#define CORE_H

#include "export.h"

DECLARE_FUNC_VOID(init)
DECLARE_FUNC_VOID(stop)

void begin_game_loop(game &g);

void waitforreloadsignal(void *hEvent);
void begin_watch_engine_directory(game &g);
void begin_watch_externals_directory(game &g);
void unload_externals(game &g);
void reload_externals(game &g);

#endif
