#ifndef KAJI_H
#define KAJI_H

/* 鍛冶 kaji -- the builder. A purpose-built, cross-platform build library
   for hot-reloading hosts: one build description covers the dev rebuild
   (the dll kansi's watcher asks for), shipping executables, shaders,
   snapshots and cached objects, on Windows and Linux, replacing per-OS
   build scripts entirely.

   Sibling to seni (migrations) and kansi (watching): kansi reports that
   sources changed, kaji turns sources into binaries, the host composes
   them. kaji never watches; kansi never compiles.

   Config file (line-based, '#' comments). Header keys:
       builddir       build            # ${B} on windows
       builddir_linux build-linux      # ${B} on linux
       tool       glslc C:\...\glslc.exe   # ${glslc} on windows
       tool_linux glslc glslc              # ${glslc} on linux
   Targets:
       target <name> <kind>            # kind: copy|shader|object|pch|dll|exe
         dep <name> [<name>...]        # build these first
         in  <path> [<path>...]        # sources (newer than out => rebuild)
         also <path> [<path>...]       # staleness-only inputs (headers)
         out <path>                    # the artifact
         include/flag/define/lib/libdir/obj <values...>   # compile/link
         post copy <from> <to>         # exe only: bundle steps after link
         post copydir <from> <to>
   Any list key may be suffixed _win or _linux to apply on one OS only.
   Values expand ${B}, ${SO} (.dll/.so), ${EXE} (.exe/empty) and ${<tool>}.
   Kind semantics:
       copy    in[0] copied to out when newer
       shader  ${glslc} in[0] -o out
       object  cc -c in[0] -o out (gets -fPIC on linux automatically)
       pch     cc -x c-header in[0] -o out (precompiled header)
       dll     cc -shared in+obj -o <tmp>, then ATOMIC rename onto out --
               a watching host never observes a half-written dll
       exe     cc in+obj -o out, then post steps
   All paths resolve against the host process cwd (the project root). */

typedef enum {
    KAJI_IDLE,     /* no run, or last run already reported */
    KAJI_RUNNING,
    KAJI_DONE,     /* returned exactly once */
    KAJI_FAILED    /* returned exactly once; see kaji_run.log_path */
} kaji_status;

typedef struct kaji kaji;

#define KAJI_RUN_MAX_STEPS 64
#define KAJI_RUN_CMD_MAX   2048

/* One in-flight build: a precomputed queue of commands executed as
   detached child processes, advanced by kaji_run_poll. Several runs may be
   in flight at once (e.g. the dev dll and a ship build). Zero-init. */
typedef struct {
    char cmds[KAJI_RUN_MAX_STEPS][KAJI_RUN_CMD_MAX];
    int  copy_step[KAJI_RUN_MAX_STEPS]; /* 0 = process; 1 = "from|to" file copy;
                                           2 = copydir; 3 = "in0|..|out" concat */
    int  step_count;
    int  step;
    char publish_tmp[512];  /* dll kind: rename tmp -> publish_out at end */
    char publish_out[512];
    char log_path[512];
    void *proc;        /* in-flight child process (compile/link steps) */
    void *copy_thread; /* in-flight worker thread (copy steps) */
    int  sync_ok;      /* result of the last synchronous step (concat) */
    int  active;
    int  exit_code;
} kaji_run;

/* Parses the build description. NULL on error (reason in err). */
kaji *kaji_load(const char *config_path, char *err, int err_size);
void  kaji_free(kaji *k);

/* Plans and starts an async build of `target` (deps first, up-to-date
   steps skipped). Returns 0 on unknown target, plan failure, or busy run
   slot. Poll from the frame loop.
   With force, the named target rebuilds even when its outputs look fresh
   (a unity build's staleness lives in headers the cfg cannot enumerate --
   the watcher's change edge is the truth); dependencies still skip when
   up to date. */
int kaji_build_async(kaji *k, const char *target, kaji_run *run, int force);

/* Non-blocking. DONE/FAILED exactly once, then IDLE. */
kaji_status kaji_run_poll(kaji *k, kaji_run *run);

/* Blocking build for CLI use. Returns 0 on success, 1 on failure. */
int kaji_build(kaji *k, const char *target, int force);

/* Compiler-agnostic compile options. kaji translates these to the active
   backend's flags (gcc today; msvc/tcc later) -- callers never pass raw
   flag strings. */
typedef enum { KAJI_STD_DEFAULT, KAJI_C89, KAJI_C99, KAJI_C11 } kaji_cstd;

typedef struct {
    kaji_cstd std;      /* gcc: -std=c89 ; msvc: /std:c11 ; tcc: -std=... */
    int       pedantic; /* gcc: -pedantic -Wall -Wextra -Werror
                           -Wno-unused-function ; msvc: /W4 /WX */
    int       coverage; /* MC/DC instrumentation. clang: -fcoverage-mcdc
                           -fprofile-instr-generate -fcoverage-mapping ;
                           gcc 14+: --coverage -fcondition-coverage */
} kaji_compile_opts;

/* A cfg-less kaji with default tools (cc=gcc), for ad-hoc compiles that are
   not config targets. Free with kaji_free. NULL on OOM. */
kaji *kaji_new(void);

/* Compile one source to a shared object using k's `cc` tool. Synchronous.
   Removes out_lib first (vnode rule). opts may be NULL (all defaults).
   Returns 0 on success (nonzero on failure, system()-style). */
int kaji_compile_shared(kaji *k, const char *src, const char *out_lib,
                        const char *err_log, const kaji_compile_opts *opts);

#endif /* KAJI_H */
