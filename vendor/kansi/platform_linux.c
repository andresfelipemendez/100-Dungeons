/* st_mtim / statfs / fork live behind feature macros under strict -std;
   _DEFAULT_SOURCE implies POSIX.1-2008 plus the BSD/glibc extras */
#define _DEFAULT_SOURCE

#include "platform.h"

#include <dirent.h>
#ifdef __linux__
#include <sys/vfs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
/* macOS spells the nanosecond mtime field st_mtimespec */
#define st_mtim st_mtimespec
#endif

int kansi_platform_spawn(const char *cmdline, const char *log_path,
                         kansi_proc *out) {
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
    out->handle = (void *)(long)pid;
    return 1;
}

int kansi_platform_proc_poll(kansi_proc *p, int *exit_code) {
    int status = 0;
    pid_t r = waitpid((pid_t)(long)p->handle, &status, WNOHANG);
    if (r <= 0) {
        return 0;
    }
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return 1;
}

void kansi_platform_proc_close(kansi_proc *p) {
    p->handle = NULL; /* reaped by waitpid */
}

unsigned long long kansi_platform_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ull + tv.tv_usec / 1000ull;
}

static int walk_recursive(const char *dir, kansi_walk_fn fn, void *user) {
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

int kansi_platform_walk(const char *dir, kansi_walk_fn fn, void *user) {
    return walk_recursive(dir, fn, user);
}

int kansi_platform_rename(const char *from, const char *to) {
    return rename(from, to) == 0;
}

int kansi_platform_mtime(const char *path, unsigned long long *mtime) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    /* nanoseconds: 1-second st_mtime would make pre_newer and the polling
       stamp blind to anything happening within the same second */
    *mtime = (unsigned long long)st.st_mtim.tv_sec * 1000000000ull
           + (unsigned long long)st.st_mtim.tv_nsec;
    return 1;
}

/* ---- change notification: inotify ------------------------------------ */

#ifdef __linux__

#include <sys/inotify.h>
#include <fcntl.h>

#define KANSI_WATCH_MAX_WDS 256

typedef struct {
    int fd;
    int wd_count;
} kansi_inotify_state;

static void inotify_add_tree(kansi_inotify_state *st, const char *dir) {
    if (st->wd_count >= KANSI_WATCH_MAX_WDS) {
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

int kansi_platform_watch_begin(kansi_watch *w,
                               const char (*dirs)[512], int dir_count) {
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
    kansi_inotify_state *st = calloc(1, sizeof(*st));
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

int kansi_platform_watch_poll(kansi_watch *w, kansi_notify_fn fn, void *user) {
    kansi_inotify_state *st = (kansi_inotify_state *)w->handle;
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

void kansi_platform_watch_end(kansi_watch *w) {
    kansi_inotify_state *st = (kansi_inotify_state *)w->handle;
    if (!st) {
        return;
    }
    close(st->fd);
    free(st);
    w->handle = NULL;
}

#else /* !__linux__ (macOS): no inotify -- decline event mode, the caller
         falls back to polling scans. FSEvents could fill this in later. */

int kansi_platform_watch_begin(kansi_watch *w,
                               const char (*dirs)[512], int dir_count) {
    (void)dirs;
    (void)dir_count;
    w->handle = NULL;
    return 0;
}

int kansi_platform_watch_poll(kansi_watch *w, kansi_notify_fn fn, void *user) {
    (void)w;
    (void)fn;
    (void)user;
    return 0;
}

void kansi_platform_watch_end(kansi_watch *w) {
    w->handle = NULL;
}

#endif /* __linux__ */
