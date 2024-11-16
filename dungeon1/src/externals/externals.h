#ifndef EXTERNALS_H
#define EXTERNALS_H

#include <export.h>

EXPORT int init_externals(struct game *g);
EXPORT void update_externals(struct game *g);
EXPORT void end_externals(struct game *g);
EXPORT void load_mesh(struct game *g);

#endif
