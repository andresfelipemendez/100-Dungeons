#ifndef KANSI_PLATFORM_H
#define KANSI_PLATFORM_H

/* OS layer for kansi: process spawning, directory walking, time, rename.
   Implemented by platform_windows.c / platform_linux.c. */

typedef struct {
    void *handle; /* HANDLE / pid */
} kansi_proc;

/* Spawns `cmdline` through the shell, appending stdout+stderr to log_path.
   Returns 0 on spawn failure. Never blocks. */
int kansi_platform_spawn(const char *cmdline, const char *log_path,
                         kansi_proc *out);

/* Returns 1 when the process has exited (exit code in *exit_code), else 0. */
int kansi_platform_proc_poll(kansi_proc *p, int *exit_code);

/* Releases the process handle (call after poll returns 1). */
void kansi_platform_proc_close(kansi_proc *p);

unsigned long long kansi_platform_now_ms(void);

/* Recursively visits every regular file under dir. Returns 0 if the dir
   cannot be opened. */
typedef void (*kansi_walk_fn)(const char *path, unsigned long long mtime,
                              unsigned long long size, void *user);
int kansi_platform_walk(const char *dir, kansi_walk_fn fn, void *user);

/* Replace `to` with `from` (atomic where the OS allows). */
int kansi_platform_rename(const char *from, const char *to);

/* File mtime; returns 0 if the file does not exist. */
int kansi_platform_mtime(const char *path, unsigned long long *mtime);

/* ---- change notification ----------------------------------------------
   Event-based directory watching (ReadDirectoryChangesW / inotify). When
   kansi_platform_watch_begin returns 0 the OS facility is unavailable and
   the caller must fall back to polling scans. */

typedef struct {
    void *handle; /* backend-private state */
} kansi_watch;

/* Called once per changed file as events are drained. `path` is the path
   as reported by the OS (relative to the watched dir); it is only valid
   for the duration of the call. */
typedef void (*kansi_notify_fn)(const char *path, void *user);

/* Starts watching every dir tree (recursive). 1 = event mode active. */
int kansi_platform_watch_begin(kansi_watch *w,
                               const char (*dirs)[512], int dir_count);

/* Non-blocking: drains pending events, invoking fn per changed file.
   Returns the number of events delivered. */
int kansi_platform_watch_poll(kansi_watch *w, kansi_notify_fn fn, void *user);

void kansi_platform_watch_end(kansi_watch *w);

#endif /* KANSI_PLATFORM_H */
