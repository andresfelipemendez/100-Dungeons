#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

int kansi_platform_spawn(const char *cmdline, const char *log_path,
                         kansi_proc *out) {
    /* Route through cmd so raw config commands (copy, redirects) work and
       output can be appended to the log without inheriting our handles. */
    char full[4096];
    if (log_path && log_path[0]) {
        snprintf(full, sizeof(full), "cmd /c \"%s >> \"%s\" 2>&1\"",
                 cmdline, log_path);
    } else {
        snprintf(full, sizeof(full), "cmd /c \"%s\"", cmdline);
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
    out->handle = pi.hProcess;
    return 1;
}

int kansi_platform_proc_poll(kansi_proc *p, int *exit_code) {
    if (WaitForSingleObject((HANDLE)p->handle, 0) != WAIT_OBJECT_0) {
        return 0;
    }
    DWORD code = 1;
    GetExitCodeProcess((HANDLE)p->handle, &code);
    *exit_code = (int)code;
    return 1;
}

void kansi_platform_proc_close(kansi_proc *p) {
    if (p->handle) {
        CloseHandle((HANDLE)p->handle);
        p->handle = NULL;
    }
}

unsigned long long kansi_platform_now_ms(void) {
    return (unsigned long long)GetTickCount64();
}

static int walk_recursive(const char *dir, kansi_walk_fn fn, void *user) {
    char pattern[MAX_PATH * 2];
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
        char path[MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk_recursive(path, fn, user);
        } else {
            ULARGE_INTEGER mtime, size;
            mtime.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            mtime.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            size.LowPart = fd.nFileSizeLow;
            size.HighPart = fd.nFileSizeHigh;
            fn(path, mtime.QuadPart, size.QuadPart, user);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 1;
}

int kansi_platform_walk(const char *dir, kansi_walk_fn fn, void *user) {
    return walk_recursive(dir, fn, user);
}

int kansi_platform_rename(const char *from, const char *to) {
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int kansi_platform_mtime(const char *path, unsigned long long *mtime) {
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) {
        return 0;
    }
    ULARGE_INTEGER t;
    t.LowPart = fa.ftLastWriteTime.dwLowDateTime;
    t.HighPart = fa.ftLastWriteTime.dwHighDateTime;
    *mtime = t.QuadPart;
    return 1;
}

/* ---- change notification: ReadDirectoryChangesW ---------------------- */

#define KANSI_WATCH_MAX_DIRS 16
#define KANSI_WATCH_BUF_SIZE (16 * 1024)

typedef struct {
    HANDLE dir;
    HANDLE event;
    OVERLAPPED ov;
    char buf[KANSI_WATCH_BUF_SIZE];
} kansi_watch_dir;

typedef struct {
    kansi_watch_dir dirs[KANSI_WATCH_MAX_DIRS];
    int dir_count;
} kansi_watch_state;

static int watch_issue(kansi_watch_dir *wd) {
    memset(&wd->ov, 0, sizeof(wd->ov));
    wd->ov.hEvent = wd->event;
    return ReadDirectoryChangesW(wd->dir, wd->buf, KANSI_WATCH_BUF_SIZE,
        TRUE /* recursive */,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
        NULL, &wd->ov, NULL) != 0;
}

int kansi_platform_watch_begin(kansi_watch *w,
                               const char (*dirs)[512], int dir_count) {
    if (dir_count > KANSI_WATCH_MAX_DIRS) {
        return 0;
    }
    kansi_watch_state *st = (kansi_watch_state *)calloc(1, sizeof(*st));
    if (!st) {
        return 0;
    }
    for (int i = 0; i < dir_count; i++) {
        kansi_watch_dir *wd = &st->dirs[st->dir_count];
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
    kansi_platform_watch_end(w);
    w->handle = NULL;
    return 0;
}

int kansi_platform_watch_poll(kansi_watch *w, kansi_notify_fn fn, void *user) {
    kansi_watch_state *st = (kansi_watch_state *)w->handle;
    if (!st) {
        return 0;
    }
    int delivered = 0;
    for (int i = 0; i < st->dir_count; i++) {
        kansi_watch_dir *wd = &st->dirs[i];
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
                    fn(path, user);
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
                fn("", user);
            }
            delivered++;
        }
        ResetEvent(wd->event);
        watch_issue(wd);
    }
    return delivered;
}

void kansi_platform_watch_end(kansi_watch *w) {
    kansi_watch_state *st = (kansi_watch_state *)w->handle;
    if (!st) {
        return;
    }
    for (int i = 0; i < KANSI_WATCH_MAX_DIRS; i++) {
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
