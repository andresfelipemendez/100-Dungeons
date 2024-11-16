#ifndef CORE_H
#define CORE_H

#include "export.h"

EXPORT void init();
EXPORT void stop();

void begin_game_loop(game &g);
void waitforreloadsignal(void *hEvent);
void unload_externals(game &g);
void reload_externals(game &g);

#endif
