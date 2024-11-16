#ifndef ENGINE_H
#define ENGINE_H

#include <export.h>

EXPORT void load_level(struct game *g, const char *c);
EXPORT void asset_reload(struct game *g, const char *c);
EXPORT void init_engine(struct game *g);
EXPORT void update(struct game *g);
EXPORT void draw_editor(struct game *g);

#endif
