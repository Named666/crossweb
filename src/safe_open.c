#define _XOPEN_SOURCE 700

#include "safe_open.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#define safe_close _close
#define safe_read _read
#else
#include <fcntl.h>
#include <unistd.h>
#define safe_close close
#define safe_read read
#endif

bool safe_relative_path(const char *input, char *out, size_t cap) {
    char tmp[SAFE_OPEN_PATH_MAX];
    size_t marks[64];
    int count = 0;
    size_t len = 0;
    const char *p;

    if (input == NULL || out == NULL || cap < 2) return false;
    if (input[0] == '\0' || input[0] == '/' || input[0] == '\\') return false;
    if (strchr(input, '\\') != NULL || strchr(input, ':') != NULL) return false;

    p = input;
    while (*p) {
        const char *start;
        size_t n;
        while (*p == '/') p++;
        if (*p == '\0') break;
        start = p;
        while (*p && *p != '/') {
            unsigned char c = (unsigned char)*p;
            if (c < 0x20 || c == 0x7f) return false;
            p++;
        }
        n = (size_t)(p - start);
        if (n == 1 && start[0] == '.') continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            if (count == 0) return false;
            count--;
            len = (count == 0) ? 0 : marks[count - 1];
            tmp[len] = '\0';
            continue;
        }
        if (n == 0 || n >= 256 || count >= 64) return false;
        if (len + (count ? 1 : 0) + n + 1 > sizeof(tmp) || len + (count ? 1 : 0) + n + 1 > cap) {
            return false;
        }
        if (count > 0) tmp[len++] = '/';
        memcpy(tmp + len, start, n);
        len += n;
        tmp[len] = '\0';
        marks[count++] = len;
    }
    if (count == 0) return false;
    memcpy(out, tmp, len + 1);
    return true;
}

#ifdef _WIN32
int safe_open_relative(const char *root, const char *relative, int flags, int mode) {
    char full[SAFE_OPEN_PATH_MAX];
    size_t i;
    HANDLE handle;
    DWORD access = 0;
    DWORD disp;
    int fd;

    if (root == NULL || relative == NULL || relative[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (snprintf(full, sizeof(full), "%s/%s", root, relative) >= (int)sizeof(full)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (i = 0; full[i]; ++i) {
        if (full[i] == '/') full[i] = '\\';
    }
    if (flags & (O_WRONLY | O_RDWR)) access |= GENERIC_WRITE;
    if ((flags & O_WRONLY) == 0) access |= GENERIC_READ;
    if (flags & O_CREAT) disp = (flags & O_TRUNC) ? CREATE_ALWAYS : OPEN_ALWAYS;
    else disp = OPEN_EXISTING;
    /* FILE_FLAG_OPEN_REPARSE_POINT does not follow symlinks or junctions. */
    handle = CreateFileA(full, access, FILE_SHARE_READ, NULL, disp,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EACCES;
        return -1;
    }
    fd = _open_osfhandle((intptr_t)handle, (flags & O_APPEND) ? _O_APPEND : 0);
    if (fd < 0) CloseHandle(handle);
    (void)mode;
    return fd;
}
#else
int safe_open_relative(const char *root, const char *relative, int flags, int mode) {
    char copy[SAFE_OPEN_PATH_MAX];
    int dirfd;
    char *p;

    if (root == NULL || relative == NULL || relative[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (snprintf(copy, sizeof(copy), "%s", relative) >= (int)sizeof(copy)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    dirfd = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) return -1;

    p = copy;
    for (;;) {
        char *slash = strchr(p, '/');
        if (slash != NULL) *slash = '\0';
        if (p[0] == '\0') {
            if (slash == NULL) break;
            p = slash + 1;
            continue;
        }
        if (slash == NULL) {
            int fd = openat(dirfd, p, flags | O_NOFOLLOW | O_CLOEXEC, mode);
            int saved = errno;
            close(dirfd);
            errno = saved;
            return fd;
        }
        {
            int child = openat(dirfd, p, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            close(dirfd);
            if (child < 0) return -1;
            dirfd = child;
            p = slash + 1;
        }
    }
    close(dirfd);
    errno = EINVAL;
    return -1;
}
#endif

bool safe_read_file(const char *root, const char *relative, unsigned char **data, size_t *len, size_t cap) {
    int fd;
    unsigned char *buf;
    size_t got = 0;

    if (data == NULL || cap == 0) {
        errno = EINVAL;
        return false;
    }
    *data = NULL;
    if (len) *len = 0;
    fd = safe_open_relative(root, relative, O_RDONLY, 0);
    if (fd < 0) return false;
    buf = (unsigned char *)malloc(cap + 1);
    if (buf == NULL) {
        safe_close(fd);
        errno = ENOMEM;
        return false;
    }
    while (got < cap + 1) {
        ssize_t n = safe_read(fd, buf + got, cap + 1 - got);
        if (n < 0) {
            int saved = errno;
            free(buf);
            safe_close(fd);
            errno = saved;
            return false;
        }
        if (n == 0) break;
        got += (size_t)n;
    }
    safe_close(fd);
    if (got > cap) {
        free(buf);
        errno = EFBIG;
        return false;
    }
    buf[got] = '\0';
    *data = buf;
    if (len) *len = got;
    return true;
}
