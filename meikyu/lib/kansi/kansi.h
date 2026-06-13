#ifndef KANSI_H
#define KANSI_H

/* 監視 kansi -- source watcher + dll rebuilder for hot-reloading hosts.

   Watches source trees; when something changes (debounced), runs the build
   pipeline as non-blocking child processes: pre commands (shader compiles,
   header snapshots), then a gcc -shared invocation assembled from the
   config, then an atomic rename of the temp dll onto the output path. The
   host's own dll watcher / migration logic takes over from there -- kansi
   ends where the reload loop begins.

   The host stays generic: all build knowledge lives in a config file next
   to the project (see kansi_config format below). Call kansi_update() once
   per frame; it never blocks.

   Watch-only mode: a config with just watch/ext/debounce_ms (no source,
   out or pre) makes kansi_update report KANSI_CHANGED edges instead of
   building -- for hosts that own a real builder (kaji) and only need the
   watching half.

   Config file format (line-based; '#' starts a comment; keys repeatable):
       watch   src                 # dir tree to watch, recursive
       watch   ../engine/src
       ext     .c .h .vert .frag   # file extensions that trigger rebuilds
       pre     copy /y a b         # raw command, run before compiling
       source  src/game_unity.c    # the single (unity) C file to compile
       include build               # -I
       libdir  ../vendor/.../lib   # -L
       lib     SDL3                # -l
       flag    -g                  # extra gcc flag
       define  ASSETS_DIR="x"      # -D
       out     build/game_new.dll  # final dll path (renamed onto, atomic)
       tmp     build/game_tmp.dll  # gcc writes here first
       log     build/kansi.log     # step output is appended here
       debounce_ms 300
   Relative paths resolve against the host process cwd. */

typedef enum {
    KANSI_IDLE,     /* nothing changed */
    KANSI_WAITING,  /* change seen, debouncing */
    KANSI_BUILDING, /* pipeline step running */
    KANSI_BUILT,    /* returned exactly once per successful rebuild */
    KANSI_ERROR,    /* returned exactly once per failed rebuild (see log) */
    KANSI_CHANGED   /* watch-only mode: debounced change edge; the host
                       hands the rebuild to its builder (e.g. kaji) */
} kansi_status;

typedef struct kansi kansi;

/* Parses the config and snapshots the initial state of the watched trees.
   Returns NULL on error and writes the reason into err (if given). */
kansi *kansi_start(const char *config_path, char *err, int err_size);

/* Call once per frame. Non-blocking. BUILT/ERROR are edge-triggered. */
kansi_status kansi_update(kansi *k);

/* Path of the build log (from config; valid while k lives). */
const char *kansi_log_path(kansi *k);

void kansi_stop(kansi *k);

#endif /* KANSI_H */
