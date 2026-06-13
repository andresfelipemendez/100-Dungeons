#ifndef SENI_RELOAD_H
#define SENI_RELOAD_H

/* reload driver: migrates every array the old block's registry describes
   into a fresh block, with zero compiled-in layout knowledge. per entry:
   look up migrate_<name> in the migration dll, cross-check the entry's
   stride against the dll's migrate_<name>_old_size export, carve the new
   array with migrate_<name>_new_size, call the migration, and let the carve
   write the new registry entry. the host (or the new dll's on-reload entry)
   can drive this knowing only the block base pointers.

   a struct present in the old registry but absent from the migration dll
   was dropped from the new layout (diff only emits migrations for structs
   in the new header); its data is dropped with a note on stderr. */

#include <stdio.h>
#include "../dodai/dodai.h"
#include "seni_registry.h"

typedef void (*seni_migrate_sig)(void* old_p, void* new_p, size_t count);

static int seni_migrate_block(void* old_base, void* new_base, size_t new_cap,
                              void *migration) {
    seni_registry* old_reg = seni_registry_get(old_base);
    size_t i;
    if (!old_reg) return 1;
    seni_registry_init(new_base, new_cap);
    for (i = 0; i < old_reg->entry_count; i++) {
        seni_array_desc* e = &old_reg->entries[i];
        char sym[SENI_REGISTRY_NAME_MAX + 32];
        seni_migrate_sig fn;
        const size_t* old_size;
        const size_t* new_size;
        void* new_arr;

        sprintf(sym, "migrate_%s", e->name);
        fn = (seni_migrate_sig)dodai_lib_symbol(migration, sym);
        if (!fn) {
            fprintf(stderr, "seni_migrate_block: no %s in migration dll; "
                    "struct dropped from new layout, dropping array '%s' (%lu elements)\n",
                    sym, e->name, (unsigned long)e->count);
            continue;
        }

        sprintf(sym, "migrate_%s_old_size", e->name);
        old_size = (const size_t*)dodai_lib_symbol(migration, sym);
        if (!old_size) {
            fprintf(stderr, "seni_migrate_block: %s missing from migration dll\n", sym);
            return 1;
        }
        /* the registry entry was written by the code that carved the array;
           the old size was compiled from the layout embedded in that same
           dll. disagreement means the registry and the diffed old layout
           drifted apart -- migrating would reinterpret memory. */
        if (*old_size != e->stride) {
            fprintf(stderr, "seni_migrate_block: array '%s' carved with stride %lu "
                    "but migration dll compiled old element size %lu -- "
                    "registry and old layout disagree, refusing to migrate\n",
                    e->name, (unsigned long)e->stride, (unsigned long)*old_size);
            return 1;
        }

        sprintf(sym, "migrate_%s_new_size", e->name);
        new_size = (const size_t*)dodai_lib_symbol(migration, sym);
        if (!new_size) {
            fprintf(stderr, "seni_migrate_block: %s missing from migration dll\n", sym);
            return 1;
        }

        new_arr = seni_carve(new_base, e->name, e->count, *new_size);
        if (!new_arr) return 1;
        fn((char*)old_base + e->offset, new_arr, e->count);
    }
    return 0;
}

#endif
