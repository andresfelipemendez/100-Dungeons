#ifndef SENI_EMBED_H
#define SENI_EMBED_H

/* Embeds a header file's bytes into the binary at build time and exports
   them as `const char* seni_layout`. The hot-reloader reads this symbol from
   the currently-loaded dll to obtain the layout it was compiled with -- the
   "old header" for diffing -- since by reload time the header file on disk
   has already been overwritten with the new layout.

   Usage (file scope, ONCE per dll -- the macro defines the global asm label
   seni_layout_data and the exported seni_layout symbol; a second use in the
   same dll is a duplicate-symbol link error):
       #include "seni_embed.h"
       SENI_EMBED_LAYOUT("path/to/structs.h");

   The path is resolved by the assembler relative to the compiler's working
   directory -- NOT relative to the source file like #include -- so pass an
   absolute path. Bytes are byte-identical to the file -- no escaping,
   no codegen.

   The preprocessor reads the layout header (#include) and the assembler
   reads it again (.incbin) at different moments within one compile. If the
   header is saved in between, the dll's embedded layout disagrees with the
   layout it was compiled against -- the exact desync this embed exists to
   prevent. Build scripts must snapshot the header to a path nothing
   overwrites mid-compile and point both the #include and this macro at the
   snapshot (see snapshot_layout in test_e2e.c). */

#if defined(_WIN32)
#define SENI_EMBED_EXPORT __declspec(dllexport)
#define SENI_EMBED_SECTION ".rdata"
#define SENI_EMBED_LABEL "seni_layout_data"
#elif defined(__APPLE__)
/* Mach-O: const data lives in __TEXT,__const (there is no .rodata), and C
   symbols carry a leading underscore at the asm level. */
#define SENI_EMBED_EXPORT
#define SENI_EMBED_SECTION "__TEXT,__const"
#define SENI_EMBED_LABEL "_seni_layout_data"
#else
#define SENI_EMBED_EXPORT
#define SENI_EMBED_SECTION ".rodata"
#define SENI_EMBED_LABEL "seni_layout_data"
#endif

#define SENI_EMBED_LAYOUT(path) \
    __asm__(".section " SENI_EMBED_SECTION "\n" \
            SENI_EMBED_LABEL ":\n" \
            ".incbin \"" path "\"\n" \
            ".byte 0\n" \
            ".text\n"); \
    extern const char seni_layout_data[]; \
    SENI_EMBED_EXPORT const char* seni_layout = seni_layout_data

#endif
