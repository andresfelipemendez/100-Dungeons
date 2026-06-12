#define _DEFAULT_SOURCE

#include "platform.h"

#ifdef __APPLE__
/* macOS spells the nanosecond mtime field st_mtimespec */
#define st_mtim st_mtimespec
#endif

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int kaji_platform_spawn(const char *cmdline, const char *log_path,
                        void **out_handle) {
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

int kaji_platform_poll(void *handle, int *exit_code) {
    int status = 0;
    pid_t r = waitpid((pid_t)(long)handle, &status, WNOHANG);
    if (r <= 0) {
        return 0;
    }
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return 1;
}

void kaji_platform_close(void *handle) {
    (void)handle; /* reaped by waitpid */
}

void kaji_platform_truncate(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) {
        fclose(f);
    }
}

int kaji_platform_mtime(const char *path, unsigned long long *mtime) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    *mtime = (unsigned long long)st.st_mtim.tv_sec * 1000000000ull
           + (unsigned long long)st.st_mtim.tv_nsec;
    return 1;
}

int kaji_platform_copy_file(const char *from, const char *to) {
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
            ok &= kaji_platform_copy_file(src, dst);
        }
    }
    closedir(d);
    return ok;
}

int kaji_platform_copy_dir(const char *from, const char *to) {
    return copy_dir_recursive(from, to);
}

void kaji_platform_make_dirs_for(const char *path) {
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

int kaji_platform_rename(const char *from, const char *to) {
    return rename(from, to) == 0;
}

int kaji_platform_is_linux(void) {
#ifdef __APPLE__
    return 0;
#else
    return 1;
#endif
}

int kaji_platform_is_macos(void) {
#ifdef __APPLE__
    return 1;
#else
    return 0;
#endif
}

void kaji_platform_sleep_ms(int ms) {
    usleep((useconds_t)ms * 1000);
}

/* ---- async copy worker ------------------------------------------------ */

#include <pthread.h>

typedef struct {
    char from[1024];
    char to[1024];
    int  is_dir;
    int  done; /* accessed with __atomic builtins */
    int  ok;
    pthread_t thread;
} kaji_copy_job;

static void *copy_worker(void *arg) {
    kaji_copy_job *job = (kaji_copy_job *)arg;
    job->ok = job->is_dir ? kaji_platform_copy_dir(job->from, job->to)
                          : kaji_platform_copy_file(job->from, job->to);
    __atomic_store_n(&job->done, 1, __ATOMIC_RELEASE);
    return NULL;
}

int kaji_platform_copy_async(const char *from, const char *to, int is_dir,
                             void **out_handle) {
    kaji_copy_job *job = (kaji_copy_job *)calloc(1, sizeof(*job));
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

int kaji_platform_copy_poll(void *handle, int *ok) {
    kaji_copy_job *job = (kaji_copy_job *)handle;
    if (!__atomic_load_n(&job->done, __ATOMIC_ACQUIRE)) {
        return 0;
    }
    *ok = job->ok;
    return 1;
}

void kaji_platform_copy_close(void *handle) {
    kaji_copy_job *job = (kaji_copy_job *)handle;
    pthread_join(job->thread, NULL);
    free(job);
}
