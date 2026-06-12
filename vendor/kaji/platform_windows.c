#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int kaji_platform_spawn(const char *cmdline, const char *log_path,
                        void **out_handle) {
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
    *out_handle = pi.hProcess;
    return 1;
}

int kaji_platform_poll(void *handle, int *exit_code) {
    if (WaitForSingleObject((HANDLE)handle, 0) != WAIT_OBJECT_0) {
        return 0;
    }
    DWORD code = 1;
    GetExitCodeProcess((HANDLE)handle, &code);
    *exit_code = (int)code;
    return 1;
}

void kaji_platform_close(void *handle) {
    if (handle) {
        CloseHandle((HANDLE)handle);
    }
}

void kaji_platform_truncate(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) {
        fclose(f);
    }
}

int kaji_platform_mtime(const char *path, unsigned long long *mtime) {
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

int kaji_platform_copy_file(const char *from, const char *to) {
    return CopyFileA(from, to, FALSE) != 0;
}

static int copy_dir_recursive(const char *from, const char *to) {
    CreateDirectoryA(to, NULL);
    char pattern[MAX_PATH * 2];
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
        char src[MAX_PATH * 2], dst[MAX_PATH * 2];
        snprintf(src, sizeof(src), "%s\\%s", from, fd.cFileName);
        snprintf(dst, sizeof(dst), "%s\\%s", to, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ok &= copy_dir_recursive(src, dst);
        } else {
            ok &= CopyFileA(src, dst, FALSE) != 0;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return ok;
}

int kaji_platform_copy_dir(const char *from, const char *to) {
    return copy_dir_recursive(from, to);
}

void kaji_platform_make_dirs_for(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *p = buf; *p; p++) {
        if ((*p == '/' || *p == '\\') && p != buf) {
            *p = 0;
            CreateDirectoryA(buf, NULL);
            *p = '\\';
        }
    }
}

int kaji_platform_rename(const char *from, const char *to) {
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int kaji_platform_is_linux(void) {
    return 0;
}

int kaji_platform_is_macos(void) {
    return 0;
}

void kaji_platform_sleep_ms(int ms) {
    Sleep((DWORD)ms);
}

/* ---- async copy worker ------------------------------------------------ */

typedef struct {
    char from[1024];
    char to[1024];
    int  is_dir;
    volatile LONG done;
    int  ok;
    HANDLE thread;
} kaji_copy_job;

static DWORD WINAPI copy_worker(LPVOID arg) {
    kaji_copy_job *job = (kaji_copy_job *)arg;
    job->ok = job->is_dir ? kaji_platform_copy_dir(job->from, job->to)
                          : kaji_platform_copy_file(job->from, job->to);
    InterlockedExchange(&job->done, 1);
    return 0;
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
    job->thread = CreateThread(NULL, 0, copy_worker, job, 0, NULL);
    if (!job->thread) {
        free(job);
        return 0;
    }
    *out_handle = job;
    return 1;
}

int kaji_platform_copy_poll(void *handle, int *ok) {
    kaji_copy_job *job = (kaji_copy_job *)handle;
    if (!InterlockedCompareExchange(&job->done, 1, 1)) {
        return 0;
    }
    *ok = job->ok;
    return 1;
}

void kaji_platform_copy_close(void *handle) {
    kaji_copy_job *job = (kaji_copy_job *)handle;
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
    free(job);
}
