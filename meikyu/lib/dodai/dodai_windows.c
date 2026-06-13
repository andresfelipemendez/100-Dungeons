/* dodai: Win32 implementation, consolidated verbatim from the former
   platform_windows.c files (kaji, kansi, seni).

   Path params arrive as `ito` views; each public entry copies them into a
   NUL char[MICHI_MAX] local via ED_COPY before the Win32 call.
   Internal helpers keep char*. (Untested box -- mirrors dodai_posix.c.) */

#include "dodai.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ED_COPY (ito path -> NUL local, logs + bails on overflow) lives in dodai.h */

/* ---- process -------------------------------------------------------------- */

int dodai_spawn(ito cmdline, michi log_path, void **out_handle) {
    char cmd[4096];
    if (!ito_copy(cmd, sizeof(cmd), cmdline)) {
        return 0;
    }
    char full[4096];
    if (log_path.s.len) {
        char log[MICHI_MAX];
        if (!ito_copy(log, sizeof(log), log_path.s)) {
            return 0;
        }
        snprintf(full, sizeof(full), "cmd /c \"%s >> \"%s\" 2>&1\"", cmd, log);
    } else {
        snprintf(full, sizeof(full), "cmd /c \"%s\"", cmd);
    }

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessA(NULL, full, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        return 0;
    }
    CloseHandle(pi.hThread);
    *out_handle = pi.hProcess;
    return 1;
}

int dodai_proc_poll(void *handle, int *exit_code) {
    if (WaitForSingleObject((HANDLE)handle, 0) != WAIT_OBJECT_0) {
        return 0;
    }
    DWORD code = 1;
    GetExitCodeProcess((HANDLE)handle, &code);
    *exit_code = (int)code;
    return 1;
}

void dodai_proc_close(void *handle) {
    if (handle) {
        CloseHandle((HANDLE)handle);
    }
}

/* ---- files ---------------------------------------------------------------- */

void dodai_truncate(michi path) {
    ED_COPY(p, path, );
    FILE *f = fopen(p, "w");
    if (f) {
        fclose(f);
    }
}

int dodai_mtime_ns(michi path, unsigned long long *out) {
    ED_COPY(p, path, 0);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(p, GetFileExInfoStandard, &fa)) {
        return 0;
    }
    ULARGE_INTEGER t;
    t.LowPart = fa.ftLastWriteTime.dwLowDateTime;
    t.HighPart = fa.ftLastWriteTime.dwHighDateTime;
    *out = t.QuadPart;
    return 1;
}

static int copy_file_c(const char *from, const char *to) {
    DeleteFileA(to); /* fresh vnode parity with posix impl */
    return CopyFileA(from, to, FALSE) != 0;
}

int dodai_copy_file(michi from, michi to) {
    ED_COPY(f, from, 0);
    ED_COPY(t, to, 0);
    return copy_file_c(f, t);
}

static int copy_dir_recursive(const char *from, const char *to) {
    CreateDirectoryA(to, NULL);
    char pattern[MICHI_MAX + 4]; /* michi path + "\*" */
    snprintf(pattern, sizeof(pattern), "%s\\*", from);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    int ok = 1;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        char src[MICHI_MAX + MAX_PATH + 4], dst[MICHI_MAX + MAX_PATH + 4];
        snprintf(src, sizeof(src), "%s\\%s", from, fd.cFileName);
        snprintf(dst, sizeof(dst), "%s\\%s", to, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ok &= copy_dir_recursive(src, dst);
        } else {
            ok &= copy_file_c(src, dst);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return ok;
}

int dodai_copy_dir(michi from, michi to) {
    ED_COPY(f, from, 0);
    ED_COPY(t, to, 0);
    return copy_dir_recursive(f, t);
}

int dodai_remove(michi path) {
    ED_COPY(p, path, 0);
    return DeleteFileA(p) != 0;
}

void dodai_remove_prefixed(michi dir, ito prefix) {
    char dirc[MICHI_MAX], pre[256];
    if (!ito_copy(dirc, sizeof(dirc), dir.s) ||
        !ito_copy(pre, sizeof(pre), prefix)) {
        return;
    }
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\%s*", dirc, pre);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char path[1024];
            snprintf(path, sizeof(path), "%s\\%s", dirc, fd.cFileName);
            DeleteFileA(path);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

int dodai_make_dir(michi path) {
    ED_COPY(p, path, 1);
    if (CreateDirectoryA(p, NULL)) return 0;
    return GetLastError() == ERROR_ALREADY_EXISTS ? 0 : 1;
}

void dodai_make_dirs_for(michi path) {
    char buf[MICHI_MAX];
    if (!ito_copy(buf, sizeof(buf), path.s)) {
        fprintf(stderr, "dodai: path too long (%d-byte cap), dirs not made: "
                "%.96s...\n", MICHI_MAX, buf);
        return;
    }
    for (char *p = buf; *p; p++) {
        if ((*p == '/' || *p == '\\') && p != buf) {
            *p = 0;
            CreateDirectoryA(buf, NULL);
            *p = '\\';
        }
    }
}

int dodai_rename(michi from, michi to) {
    ED_COPY(f, from, 0);
    ED_COPY(t, to, 0);
    return MoveFileExA(f, t, MOVEFILE_REPLACE_EXISTING) != 0;
}

void *dodai_read_file(michi path, size_t *len) {
    char p[MICHI_MAX];
    if (!ito_copy(p, sizeof(p), path.s)) {
        return NULL;
    }
    FILE *f = fopen(p, "rb");
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

int dodai_write_file(michi path, const void *data, size_t len) {
    ED_COPY(p, path, 0);
    FILE *f = fopen(p, "wb");
    if (!f) {
        return 0;
    }
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return written == len;
}

static int walk_recursive(const char *dir, dodai_walk_fn fn, void *user) {
    char pattern[MICHI_MAX + 4]; /* michi path + "\*" */
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        char path[MICHI_MAX + MAX_PATH + 4];
        snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk_recursive(path, fn, user);
        } else {
            ULARGE_INTEGER mtime, size;
            mtime.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            mtime.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            size.LowPart = fd.nFileSizeLow;
            size.HighPart = fd.nFileSizeHigh;
            fn(michi_from_cstr(path), mtime.QuadPart, size.QuadPart, user);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 1;
}

int dodai_walk(michi dir, dodai_walk_fn fn, void *user) {
    ED_COPY(d, dir, 0);
    return walk_recursive(d, fn, user);
}

int dodai_absolute_path(michi rel, michi_buf *out) {
    ED_COPY(relc, rel, 1);
    char full[MICHI_MAX];
    char *p;
    DWORD n = GetFullPathNameA(relc, (DWORD)sizeof(full), full, NULL);
    if (n == 0 || n >= sizeof(full)) {
        fprintf(stderr, "GetFullPathName failed for %s (need %lu, error %lu)\n",
                relc, (unsigned long)n, GetLastError());
        return 1;
    }
    /* forward slashes: the path gets pasted into #include and .incbin
       strings, where backslashes would be parsed as escapes */
    for (p = full; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    ito_buf_append_c(&out->b, full);
    return out->b.overflow ? 1 : 0;
}

int dodai_chdir(michi path) {
    ED_COPY(p, path, 0);
    return _chdir(p) == 0;
}

/* ---- async copy worker ---------------------------------------------------- */

typedef struct {
    char from[1024];
    char to[1024];
    int  is_dir;
    volatile LONG done;
    int  ok;
    HANDLE thread;
} dodai_copy_job;

static DWORD WINAPI copy_worker(LPVOID arg) {
    dodai_copy_job *job = (dodai_copy_job *)arg;
    job->ok = job->is_dir ? copy_dir_recursive(job->from, job->to)
                          : copy_file_c(job->from, job->to);
    InterlockedExchange(&job->done, 1);
    return 0;
}

int dodai_copy_async(michi from, michi to, int is_dir, void **out_handle) {
    dodai_copy_job *job = (dodai_copy_job *)calloc(1, sizeof(*job));
    if (!job) {
        return 0;
    }
    if (!ito_copy(job->from, sizeof(job->from), from.s) ||
        !ito_copy(job->to, sizeof(job->to), to.s)) {
        free(job);
        return 0;
    }
    job->is_dir = is_dir;
    job->thread = CreateThread(NULL, 0, copy_worker, job, 0, NULL);
    if (!job->thread) {
        free(job);
        return 0;
    }
    *out_handle = job;
    return 1;
}

int dodai_copy_poll(void *handle, int *ok) {
    dodai_copy_job *job = (dodai_copy_job *)handle;
    if (!InterlockedCompareExchange(&job->done, 1, 1)) {
        return 0;
    }
    *ok = job->ok;
    return 1;
}

void dodai_copy_close(void *handle) {
    dodai_copy_job *job = (dodai_copy_job *)handle;
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
    free(job);
}

/* ---- change notification: ReadDirectoryChangesW --------------------------- */

#define DODAI_WATCH_MAX_DIRS 16
#define DODAI_WATCH_BUF_SIZE (16 * 1024)

typedef struct {
    HANDLE dir;
    HANDLE event;
    OVERLAPPED ov;
    char buf[DODAI_WATCH_BUF_SIZE];
} dodai_watch_dir;

typedef struct {
    dodai_watch_dir dirs[DODAI_WATCH_MAX_DIRS];
    int dir_count;
} dodai_watch_state;

static int watch_issue(dodai_watch_dir *wd) {
    memset(&wd->ov, 0, sizeof(wd->ov));
    wd->ov.hEvent = wd->event;
    return ReadDirectoryChangesW(wd->dir, wd->buf, DODAI_WATCH_BUF_SIZE,
        TRUE /* recursive */,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
        NULL, &wd->ov, NULL) != 0;
}

int dodai_watch_begin(dodai_watch *w, const char (*dirs)[512], int dir_count) {
    if (dir_count > DODAI_WATCH_MAX_DIRS) {
        return 0;
    }
    dodai_watch_state *st = (dodai_watch_state *)calloc(1, sizeof(*st));
    if (!st) {
        return 0;
    }
    for (int i = 0; i < dir_count; i++) {
        dodai_watch_dir *wd = &st->dirs[st->dir_count];
        wd->dir = CreateFileA(dirs[i], FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
        if (wd->dir == INVALID_HANDLE_VALUE) {
            goto fail;
        }
        wd->event = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!wd->event || !watch_issue(wd)) {
            goto fail;
        }
        st->dir_count++;
    }
    w->handle = st;
    return 1;
fail:
    w->handle = st;
    dodai_watch_end(w);
    w->handle = NULL;
    return 0;
}

int dodai_watch_poll(dodai_watch *w, dodai_notify_fn fn, void *user) {
    dodai_watch_state *st = (dodai_watch_state *)w->handle;
    if (!st) {
        return 0;
    }
    int delivered = 0;
    for (int i = 0; i < st->dir_count; i++) {
        dodai_watch_dir *wd = &st->dirs[i];
        if (WaitForSingleObject(wd->event, 0) != WAIT_OBJECT_0) {
            continue;
        }
        DWORD bytes = 0;
        if (GetOverlappedResult(wd->dir, &wd->ov, &bytes, FALSE) && bytes > 0) {
            char *p = wd->buf;
            for (;;) {
                FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)p;
                char path[1024];
                int n = WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                    (int)(info->FileNameLength / sizeof(WCHAR)),
                    path, sizeof(path) - 1, NULL, NULL);
                path[n > 0 ? n : 0] = 0;
                if (fn && n > 0) {
                    fn(michi_from_cstr(path), user);
                }
                delivered++;
                if (info->NextEntryOffset == 0) {
                    break;
                }
                p += info->NextEntryOffset;
            }
        } else {
            /* overflow (bytes == 0): events were dropped; report a generic
               change so the caller still reacts */
            if (fn) {
                fn(michi_from_cstr(""), user);
            }
            delivered++;
        }
        ResetEvent(wd->event);
        watch_issue(wd);
    }
    return delivered;
}

void dodai_watch_end(dodai_watch *w) {
    dodai_watch_state *st = (dodai_watch_state *)w->handle;
    if (!st) {
        return;
    }
    for (int i = 0; i < DODAI_WATCH_MAX_DIRS; i++) {
        if (st->dirs[i].dir && st->dirs[i].dir != INVALID_HANDLE_VALUE) {
            CancelIo(st->dirs[i].dir);
            CloseHandle(st->dirs[i].dir);
        }
        if (st->dirs[i].event) {
            CloseHandle(st->dirs[i].event);
        }
    }
    free(st);
    w->handle = NULL;
}

/* ---- dynamic libraries ---------------------------------------------------- */

void *dodai_lib_open(michi path) {
    ED_COPY(p, path, NULL);
    HMODULE m = LoadLibraryA(p);
    if (!m) {
        fprintf(stderr, "LoadLibrary failed for %s (error %lu)\n", p,
                GetLastError());
    }
    return (void *)m;
}

void *dodai_lib_symbol(void *lib, const char *name) {
    /* c89 forbids function-pointer -> object-pointer casts; launder through
       an integer (size_t is pointer-sized on win32/win64) */
    return (void *)(size_t)GetProcAddress((HMODULE)lib, name);
}

void dodai_lib_close(void *lib) {
    if (lib) FreeLibrary((HMODULE)lib);
}

const char *dodai_lib_extension(void) {
    return "dll";
}

/* ---- lock file ------------------------------------------------------------ */

int dodai_lockfile_try(michi path, void **out_handle) {
    ED_COPY(p, path, 0);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
                           NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    *out_handle = (void *)h;
    return 1;
}

void dodai_lockfile_release(void *handle) {
    CloseHandle((HANDLE)handle);
}

unsigned long dodai_pid(void) {
    return (unsigned long)GetCurrentProcessId();
}

/* ---- time ----------------------------------------------------------------- */

unsigned long long dodai_now_ms(void) {
    return (unsigned long long)GetTickCount64();
}

void dodai_sleep_ms(int ms) {
    Sleep((DWORD)ms);
}

/* ---- OS switch ------------------------------------------------------------ */

int dodai_is_linux(void) {
    return 0;
}

int dodai_is_macos(void) {
    return 0;
}
