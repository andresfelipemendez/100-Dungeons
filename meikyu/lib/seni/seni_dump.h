#ifndef SENI_DUMP_H
#define SENI_DUMP_H

/* audit log for generated migration TUs. every TU the harness compiles is
   written to build/seni_out/migration_NNN.c and compiled FROM that path, so
   the file on disk is byte-for-byte the source gcc consumed -- when a
   migration does something surprising, the generated code is the forensic
   record. files are never deleted or reused; NNN continues from the highest
   existing number across runs. */

#include <stdio.h>
#include "../dodai/dodai.h"

/* writes code to build/seni_out/migration_NNN.c, prefixed with a one-line
   provenance comment naming the caller's tag. on success returns 0 and puts
   the path in out_path (cap = sizeof of caller's buffer); on failure prints
   the path that could not be written and returns 1. */
static int seni_dump_migration(const char* code, const char* tag,
                               char* out_path, size_t out_cap) {
    static long next = -1;
    FILE* f;
    dodai_make_dir(PATH("build"));
    dodai_make_dir(PATH("build/seni_out"));
    if (next < 0) {
        /* resume numbering after the last file from previous runs */
        char probe[256];
        next = 0;
        for (;;) {
            sprintf(probe, "build/seni_out/migration_%03ld.c", next);
            f = fopen(probe, "rb");
            if (!f) break;
            fclose(f);
            next++;
        }
    }
    snprintf(out_path, out_cap, "build/seni_out/migration_%03ld.c", next);
    f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "seni_dump: cannot write %s\n", out_path);
        return 1;
    }
    fprintf(f, "/* seni migration %03ld: %s */\n", next, tag);
    fputs(code, f);
    fclose(f);
    next++;
    return 0;
}

#endif
