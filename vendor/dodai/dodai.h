#ifndef DODAI_H
#define DODAI_H

/* dodai (土台): the single OS layer for the monorepo. Used by kansi, kaji,
   the seni test harness, and the engine host. SDL-free on purpose: the lib
   test suites link it without SDL. Implemented by dodai_posix.c
   (linux + mac) and dodai_windows.c -- pick one per build, no #ifdef soup.

   Conventions (inherited from the kansi/kaji platform layers):
   - functions return 1 on success / 0 on failure unless noted
   - nothing blocks: spawn/poll/close, async copy, watch are all poll-based
   - callers log and continue; dodai itself only prints on dlopen / absolute-path failure */

#include <stddef.h>

#ifdef _WIN32
#define DODAI_DLL_SUFFIX ".dll"
#define DODAI_PIC ""
#else
#define DODAI_DLL_SUFFIX ".so"
#define DODAI_PIC " -fPIC"
#endif

/* ---- process: spawn through the shell, output appended to log_path ---- */
int  dodai_spawn(const char *cmdline, const char *log_path, void **out_handle);
/* 1 when exited (exit code in *exit_code), else 0 */
int  dodai_proc_poll(void *handle, int *exit_code);
void dodai_proc_close(void *handle);
/* this process's id (per-instance file suffixes) */
unsigned long dodai_pid(void);

/* ---- files ------------------------------------------------------------ */
/* monotonic-per-OS mtime in ns-ish units (POSIX: epoch ns; windows: FILETIME
   ticks) -- compare only against same-OS values; returns 0 if the file does
   not exist */
int  dodai_mtime_ns(const char *path, unsigned long long *out);
/* replace `to` with `from` (atomic where the OS allows) */
int  dodai_rename(const char *from, const char *to);
/* byte copy. Removes dst first: macOS validates code signatures per vnode,
   overwriting a previously-mapped dylib in place gets the next dlopen
   SIGKILLed -- a fresh vnode per copy avoids that, harmless elsewhere. */
int  dodai_copy_file(const char *from, const char *to);
int  dodai_copy_dir(const char *from, const char *to);
int  dodai_remove(const char *path);
/* delete top-level regular files in dir whose name starts with prefix
   (startup scratch cleanup; deliberately NOT recursive -- build dirs hold
   the huge cmake _deps tree) */
void dodai_remove_prefixed(const char *dir, const char *prefix);
/* 0 on success or already-exists (seni harness convention) */
int  dodai_make_dir(const char *path);
/* mkdir -p for the directory containing `path`; best effort */
void dodai_make_dirs_for(const char *path);
void dodai_truncate(const char *path);
/* whole file, malloc'd, NUL-terminated (*len excludes the NUL); NULL on
   failure. Caller frees. */
void *dodai_read_file(const char *path, size_t *len);
int  dodai_write_file(const char *path, const void *data, size_t len);
/* recursively visits every regular file under dir; 0 if dir won't open */
typedef void (*dodai_walk_fn)(const char *path, unsigned long long mtime_ns,
                              unsigned long long size, void *user);
int  dodai_walk(const char *dir, dodai_walk_fn fn, void *user);
/* absolute path with forward slashes (safe to paste into generated source
   and .incbin); rel must exist; 0 on success (seni harness convention) */
int  dodai_absolute_path(const char *rel, char *out, size_t cap);
/* change working directory; 1 on success */
int  dodai_chdir(const char *path);

/* ---- async copy on a worker thread (file or dir) ----------------------- */
int  dodai_copy_async(const char *from, const char *to, int is_dir,
                      void **out_handle);
int  dodai_copy_poll(void *handle, int *ok);
void dodai_copy_close(void *handle);

/* ---- change notification ----------------------------------------------
   Event-based watching (inotify / ReadDirectoryChangesW). When watch_begin
   returns 0 the facility is unavailable (macOS: always, until an FSEvents
   backend exists) and the caller falls back to polling scans. */
typedef struct { void *handle; } dodai_watch;
typedef void (*dodai_notify_fn)(const char *path, void *user);
int  dodai_watch_begin(dodai_watch *w, const char (*dirs)[512], int dir_count);
int  dodai_watch_poll(dodai_watch *w, dodai_notify_fn fn, void *user);
void dodai_watch_end(dodai_watch *w);

/* ---- dynamic libraries -------------------------------------------------
   open prints the dlopen/LoadLibrary error on failure (seni convention).
   Bare names (no '/') get a "./" prefix on POSIX so dlopen looks in the cwd;
   windows LoadLibraryA uses its own search order. */
void *dodai_lib_open(const char *path);
void *dodai_lib_symbol(void *lib, const char *name);
void  dodai_lib_close(void *lib);
const char *dodai_lib_extension(void); /* "so" / "dll", no dot */

/* ---- shared-library compile (seni e2e harness, engine mig compile) -----
   gcc -shared [DODAI_PIC] [extra_flags] src -o lib, stderr to err_log.
   Removes lib first (same codesign-vnode rule as dodai_copy_file).
   extra_flags may be NULL. Returns 0 on success (system() convention). */
int  dodai_compile_shared(const char *src, const char *lib,
                          const char *err_log, const char *extra_flags);

/* ---- lock file (engine forge lock) -------------------------------------
   Held until release or process death -- flock on POSIX, delete-on-close
   CreateFile on windows -- so a crashed holder never wedges the project. */
int  dodai_lockfile_try(const char *path, void **out_handle);
void dodai_lockfile_release(void *handle);

/* ---- time --------------------------------------------------------------*/
unsigned long long dodai_now_ms(void);
void dodai_sleep_ms(int ms);

/* ---- OS switch (kaji cfg keys hang off these; windows = neither) ------ */
int dodai_is_linux(void);
int dodai_is_macos(void);

#endif /* DODAI_H */
