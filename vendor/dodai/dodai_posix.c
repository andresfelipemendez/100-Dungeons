/* dodai: POSIX implementation (linux + macOS). Bodies consolidated verbatim
   from the former kaji/kansi/seni platform_linux.c files. */

#define _DEFAULT_SOURCE
/* realpath needs XSI on glibc; on macOS it is visible by default, and
   _XOPEN_SOURCE strict mode would HIDE Darwin extensions other TUs need
   if this file is ever #included into a test build. */
#ifndef __APPLE__
#define _XOPEN_SOURCE 700
#endif

#include "dodai.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/vfs.h>
#endif

#ifdef __APPLE__
/* macOS spells the nanosecond mtime field st_mtimespec */
#define st_mtim st_mtimespec
#endif

/* ---- process -------------------------------------------------------------- */

int dodai_spawn(const char *cmdline, const char *log_path, void **out_handle) {
    char full[4096];
    if (log_path && log_path[0]) {
        snprintf(full, sizeof(full), "%s >> '%s' 2>&1", cmdline, log_path);
    } else {
        snprintf(full, sizeof(full), "%s", cmdline);
    }
    pid_t pid = fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", full, (char *)NULL);
        _exit(127);
    }
    *out_handle = (void *)(long)pid;
    return 1;
}

int dodai_proc_poll(void *handle, int *exit_code) {
    int status = 0;
    pid_t r = waitpid((pid_t)(long)handle, &status, WNOHANG);
    if (r <= 0) {
        return 0;
    }
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return 1;
}

void dodai_proc_close(void *handle) {
    (void)handle; /* reaped by waitpid */
}

/* ---- files ---------------------------------------------------------------- */

void dodai_truncate(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) {
        fclose(f);
    }
}

int dodai_mtime_ns(const char *path, unsigned long long *out) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    *out = (unsigned long long)st.st_mtim.tv_sec * 1000000000ull
         + (unsigned long long)st.st_mtim.tv_nsec;
    return 1;
}

int dodai_copy_file(const char *from, const char *to) {
    remove(to); /* fresh vnode: see header (macOS codesign) */
    FILE *src = fopen(from, "rb");
    if (!src) {
        return 0;
    }
    FILE *dst = fopen(to, "wb");
    if (!dst) {
        fclose(src);
        return 0;
    }
    char buf[1 << 16];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            ok = 0;
            break;
        }
    }
    fclose(src);
    fclose(dst);
    return ok;
}

static int copy_dir_recursive(const char *from, const char *to) {
    mkdir(to, 0755);
    DIR *d = opendir(from);
    if (!d) {
        return 0;
    }
    int ok = 1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        char src[2048], dst[2048];
        snprintf(src, sizeof(src), "%s/%s", from, e->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", to, e->d_name);
        struct stat st;
        if (stat(src, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            ok &= copy_dir_recursive(src, dst);
        } else if (S_ISREG(st.st_mode)) {
            ok &= dodai_copy_file(src, dst);
        }
    }
    closedir(d);
    return ok;
}

int dodai_copy_dir(const char *from, const char *to) {
    return copy_dir_recursive(from, to);
}

int dodai_remove(const char *path) {
    return remove(path) == 0;
}

void dodai_remove_prefixed(const char *dir, const char *prefix) {
    DIR *d = opendir(dir);
    if (!d) {
        return;
    }
    size_t plen = strlen(prefix);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, prefix, plen) != 0) {
            continue;
        }
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            remove(path);
        }
    }
    closedir(d);
}

int dodai_make_dir(const char *path) {
    if (mkdir(path, 0755) == 0) return 0;
    return errno == EEXIST ? 0 : 1;
}

void dodai_make_dirs_for(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *p = buf; *p; p++) {
        if (*p == '/' && p != buf) {
            *p = 0;
            mkdir(buf, 0755);
            *p = '/';
        }
    }
}

int dodai_rename(const char *from, const char *to) {
    return rename(from, to) == 0;
}

void *dodai_read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[n] = 0;
    if (len) {
        *len = (size_t)n;
    }
    return buf;
}

int dodai_write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return 0;
    }
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return written == len;
}

static int walk_recursive(const char *dir, dodai_walk_fn fn, void *user) {
    DIR *d = opendir(dir);
    if (!d) {
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            walk_recursive(path, fn, user);
        } else if (S_ISREG(st.st_mode)) {
            unsigned long long ns = (unsigned long long)st.st_mtim.tv_sec
                                  * 1000000000ull
                                  + (unsigned long long)st.st_mtim.tv_nsec;
            fn(path, ns, (unsigned long long)st.st_size, user);
        }
    }
    closedir(d);
    return 1;
}

int dodai_walk(const char *dir, dodai_walk_fn fn, void *user) {
    return walk_recursive(dir, fn, user);
}

int dodai_absolute_path(const char *rel, char *out, size_t cap) {
    char full[PATH_MAX];
    size_t n;
    if (!realpath(rel, full)) {
        fprintf(stderr, "realpath failed for %s: %s\n", rel, strerror(errno));
        return 1;
    }
    n = strlen(full);
    if (n + 1 > cap) {
        fprintf(stderr, "absolute path for %s is %lu bytes, cap %lu\n",
                rel, (unsigned long)n + 1, (unsigned long)cap);
        return 1;
    }
    memcpy(out, full, n + 1);
    return 0;
}

int dodai_chdir(const char *path) {
    return chdir(path) == 0;
}

/* ---- async copy worker ---------------------------------------------------- */

typedef struct {
    char from[1024];
    char to[1024];
    int  is_dir;
    int  done; /* accessed with __atomic builtins */
    int  ok;
    pthread_t thread;
} dodai_copy_job;

static void *copy_worker(void *arg) {
    dodai_copy_job *job = (dodai_copy_job *)arg;
    job->ok = job->is_dir ? dodai_copy_dir(job->from, job->to)
                          : dodai_copy_file(job->from, job->to);
    __atomic_store_n(&job->done, 1, __ATOMIC_RELEASE);
    return NULL;
}

int dodai_copy_async(const char *from, const char *to, int is_dir,
                     void **out_handle) {
    dodai_copy_job *job = (dodai_copy_job *)calloc(1, sizeof(*job));
    if (!job) {
        return 0;
    }
    snprintf(job->from, sizeof(job->from), "%s", from);
    snprintf(job->to, sizeof(job->to), "%s", to);
    job->is_dir = is_dir;
    if (pthread_create(&job->thread, NULL, copy_worker, job) != 0) {
        free(job);
        return 0;
    }
    *out_handle = job;
    return 1;
}

int dodai_copy_poll(void *handle, int *ok) {
    dodai_copy_job *job = (dodai_copy_job *)handle;
    if (!__atomic_load_n(&job->done, __ATOMIC_ACQUIRE)) {
        return 0;
    }
    *ok = job->ok;
    return 1;
}

void dodai_copy_close(void *handle) {
    dodai_copy_job *job = (dodai_copy_job *)handle;
    pthread_join(job->thread, NULL);
    free(job);
}

/* ---- change notification: inotify ---------------------------------------- */

#ifdef __linux__

#include <sys/inotify.h>

#define DODAI_WATCH_MAX_WDS 256

typedef struct {
    int fd;
    int wd_count;
} dodai_inotify_state;

static void inotify_add_tree(dodai_inotify_state *st, const char *dir) {
    if (st->wd_count >= DODAI_WATCH_MAX_WDS) {
        return;
    }
    if (inotify_add_watch(st->fd, dir,
            IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO |
            IN_MOVED_FROM) >= 0) {
        st->wd_count++;
    }
    DIR *d = opendir(dir);
    if (!d) {
        return;
    }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat stt;
        if (stat(path, &stt) == 0 && S_ISDIR(stt.st_mode)) {
            inotify_add_tree(st, path);
        }
    }
    closedir(d);
}

int dodai_watch_begin(dodai_watch *w, const char (*dirs)[512], int dir_count) {
    /* inotify silently delivers nothing on 9p mounts (WSL /mnt/c drvfs):
       inotify_add_watch succeeds but no events ever arrive. Refuse event
       mode there so the caller falls back to polling. */
    int i;
    for (i = 0; i < dir_count; i++) {
        struct statfs sfs;
        if (statfs(dirs[i], &sfs) == 0 && sfs.f_type == 0x01021997 /* V9FS */) {
            return 0;
        }
    }
    dodai_inotify_state *st = calloc(1, sizeof(*st));
    if (!st) {
        return 0;
    }
    st->fd = inotify_init1(IN_NONBLOCK);
    if (st->fd < 0) {
        free(st);
        return 0;
    }
    for (int i = 0; i < dir_count; i++) {
        inotify_add_tree(st, dirs[i]);
    }
    if (st->wd_count == 0) {
        close(st->fd);
        free(st);
        return 0;
    }
    /* Note: subdirectories created after begin are not auto-watched; the
       host can restart the watcher, or rely on existing-dir events. */
    w->handle = st;
    return 1;
}

int dodai_watch_poll(dodai_watch *w, dodai_notify_fn fn, void *user) {
    dodai_inotify_state *st = (dodai_inotify_state *)w->handle;
    if (!st) {
        return 0;
    }
    char buf[4096];
    int delivered = 0;
    for (;;) {
        ssize_t len = read(st->fd, buf, sizeof(buf));
        if (len <= 0) {
            break;
        }
        char *p = buf;
        while (p < buf + len) {
            struct inotify_event *ev = (struct inotify_event *)p;
            if (fn) {
                fn(ev->len ? ev->name : "", user);
            }
            delivered++;
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    return delivered;
}

void dodai_watch_end(dodai_watch *w) {
    dodai_inotify_state *st = (dodai_inotify_state *)w->handle;
    if (!st) {
        return;
    }
    close(st->fd);
    free(st);
    w->handle = NULL;
}

#else /* !__linux__ (macOS): no inotify -- decline event mode, the caller
         falls back to polling scans. FSEvents could fill this in later. */

int dodai_watch_begin(dodai_watch *w, const char (*dirs)[512], int dir_count) {
    (void)dirs;
    (void)dir_count;
    w->handle = NULL;
    return 0;
}

int dodai_watch_poll(dodai_watch *w, dodai_notify_fn fn, void *user) {
    (void)w;
    (void)fn;
    (void)user;
    return 0;
}

void dodai_watch_end(dodai_watch *w) {
    w->handle = NULL;
}

#endif /* __linux__ */

/* ---- dynamic libraries ---------------------------------------------------- */

void *dodai_lib_open(const char *path) {
    /* "./" prefix for bare names: dlopen searches LD_LIBRARY_PATH for
       names without a slash, not the cwd (seni harness behavior) */
    char full[1024];
    if (!strchr(path, '/')) {
        snprintf(full, sizeof(full), "./%s", path);
        path = full;
    }
    void *m = dlopen(path, RTLD_NOW);
    if (!m) {
        fprintf(stderr, "dlopen failed for %s: %s\n", path, dlerror());
    }
    return m;
}

void *dodai_lib_symbol(void *lib, const char *name) {
    return dlsym(lib, name);
}

void dodai_lib_close(void *lib) {
    if (lib) dlclose(lib);
}

const char *dodai_lib_extension(void) {
    return "so";
}

/* ---- shared-library compile ----------------------------------------------- */

int dodai_compile_shared(const char *src, const char *lib,
                         const char *err_log, const char *extra_flags) {
    char cmd[2048];
    remove(lib); /* fresh vnode: see header (macOS codesign) */
    snprintf(cmd, sizeof(cmd), "gcc -shared" DODAI_PIC " %s %s -o %s 2> %s",
             extra_flags ? extra_flags : "", src, lib, err_log);
    return system(cmd);
}

/* ---- lock file ------------------------------------------------------------ */

int dodai_lockfile_try(const char *path, void **out_handle) {
    /* the fd stays open for the process lifetime; the OS drops the flock
       on death, so a crashed holder never wedges the project */
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        return 0;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return 0;
    }
    *out_handle = (void *)(long)fd;
    return 1;
}

void dodai_lockfile_release(void *handle) {
    close((int)(long)handle);
}

unsigned long dodai_pid(void) {
    return (unsigned long)getpid();
}

/* ---- time ----------------------------------------------------------------- */

unsigned long long dodai_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ull + tv.tv_usec / 1000ull;
}

void dodai_sleep_ms(int ms) {
    usleep((useconds_t)ms * 1000);
}

/* ---- OS switch ------------------------------------------------------------ */

int dodai_is_linux(void) {
#ifdef __APPLE__
    return 0;
#else
    return 1;
#endif
}

int dodai_is_macos(void) {
#ifdef __APPLE__
    return 1;
#else
    return 0;
#endif
}
