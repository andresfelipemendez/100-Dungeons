#ifndef KAJI_PLATFORM_H
#define KAJI_PLATFORM_H

/* OS layer for kaji, implemented by platform_windows.c / platform_linux.c. */

/* Spawns cmdline through the shell, output appended to log_path (may be
   NULL). Returns 0 on failure. Never blocks. */
int kaji_platform_spawn(const char *cmdline, const char *log_path,
                        void **out_handle);

/* Returns 1 when the process exited (exit code in *exit_code), else 0. */
int kaji_platform_poll(void *handle, int *exit_code);

/* Releases the handle (after poll returned 1). */
void kaji_platform_close(void *handle);

/* Truncates a file (used to reset the log before a run). */
void kaji_platform_truncate(const char *path);

/* File mtime in ns-ish monotonic units; returns 0 if missing. */
int kaji_platform_mtime(const char *path, unsigned long long *mtime);

/* Byte copy, replacing dst. Returns 0 on failure. */
int kaji_platform_copy_file(const char *from, const char *to);

/* Recursive directory copy (creating dst). Returns 0 on failure. */
int kaji_platform_copy_dir(const char *from, const char *to);

/* mkdir -p for the directory containing `path` (and the path itself when
   it has no file part). Best effort. */
void kaji_platform_make_dirs_for(const char *path);

/* Async copy on a worker thread: file or recursive dir. The frame loop
   polls it exactly like a child process -- big asset copies never block.
   Returns 0 on thread-start failure. */
int kaji_platform_copy_async(const char *from, const char *to, int is_dir,
                             void **out_handle);

/* Returns 1 when the copy finished (*ok = success), else 0. */
int kaji_platform_copy_poll(void *handle, int *ok);

/* Joins and frees (after poll returned 1). */
void kaji_platform_copy_close(void *handle);

/* Replace `to` with `from` (atomic where the OS allows). */
int kaji_platform_rename(const char *from, const char *to);

/* OS switches the cfg keys hang off: exactly one returns 1. (Both 0 means
   windows.) */
int kaji_platform_is_linux(void);
int kaji_platform_is_macos(void);

/* Sleep a few ms (blocking build's poll loop). */
void kaji_platform_sleep_ms(int ms);

#endif /* KAJI_PLATFORM_H */
