#define _XOPEN_SOURCE 700

#include "path.h"

#include "models.h"
#include "../../safe_open.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

static char g_root[SAFE_OPEN_PATH_MAX];

bool fs_set_root(const char *root) {
    char resolved[SAFE_OPEN_PATH_MAX];
    struct stat st;
    if (root == NULL || root[0] == '\0') return false;
#ifdef _WIN32
    if (GetFullPathNameA(root, sizeof(resolved), resolved, NULL) == 0) return false;
#else
    if (realpath(root, resolved) == NULL) return false;
#endif
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    if (snprintf(g_root, sizeof(g_root), "%s", resolved) >= (int)sizeof(g_root)) {
        g_root[0] = '\0';
        return false;
    }
    return true;
}

const char *fs_get_root(void) {
    return g_root[0] ? g_root : NULL;
}

void fs_clear_root(void) {
    g_root[0] = '\0';
}

static bool mkdir_one(const char *path) {
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST) return true;
#else
    if (mkdir(path, 0700) == 0 || errno == EEXIST) return true;
#endif
    return false;
}

static bool mkdir_p(const char *path) {
    char tmp[SAFE_OPEN_PATH_MAX];
    size_t len;
    size_t i;
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) return false;
    len = strlen(tmp);
    if (len == 0) return false;
    for (i = 1; i < len; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            tmp[i] = '\0';
            if (!mkdir_one(tmp)) return false;
            tmp[i] = '/';
        }
    }
    return mkdir_one(tmp);
}

bool fs_ensure_user_root(void) {
    char path[SAFE_OPEN_PATH_MAX];
    const char *base = getenv("XDG_DATA_HOME");
    if (base != NULL && base[0] != '\0') {
        if (snprintf(path, sizeof(path), "%s/crossweb", base) >= (int)sizeof(path)) return false;
    } else {
#ifdef _WIN32
        const char *local = getenv("LOCALAPPDATA");
        if (local == NULL || local[0] == '\0') local = getenv("USERPROFILE");
        if (local == NULL || local[0] == '\0') return false;
        if (snprintf(path, sizeof(path), "%s\\Crossweb", local) >= (int)sizeof(path)) return false;
#else
        const char *home = getenv("HOME");
        if (home == NULL || home[0] == '\0') return false;
        if (snprintf(path, sizeof(path), "%s/.local/share/crossweb", home) >= (int)sizeof(path)) return false;
#endif
    }
    if (!mkdir_p(path)) return false;
    return fs_set_root(path);
}

FsError fs_read_file(const ReadFileRequest *req, char **content, size_t *size) {
    char relative[SAFE_OPEN_PATH_MAX];
    unsigned char *data = NULL;
    size_t len = 0;
    if (content) *content = NULL;
    if (size) *size = 0;
    if (req == NULL || req->path == NULL || content == NULL || size == NULL) return FS_ERROR_INVALID_PATH;
    if (g_root[0] == '\0') return FS_ERROR_PERMISSION_DENIED;
    if (!safe_relative_path(req->path, relative, sizeof(relative))) return FS_ERROR_INVALID_PATH;
    if (!safe_read_file(g_root, relative, &data, &len, 1024 * 1024)) {
        if (errno == ENOENT) return FS_ERROR_FILE_NOT_FOUND;
        if (errno == ELOOP || errno == EACCES || errno == EPERM) return FS_ERROR_PERMISSION_DENIED;
        return FS_ERROR_IO_ERROR;
    }
    *content = (char *)data;
    *size = len;
    return FS_ERROR_NONE;
}

FsError fs_write_file(const WriteFileRequest *req) {
    char relative[SAFE_OPEN_PATH_MAX];
    int fd;
    const char *text;
    size_t len;
    size_t off = 0;
    if (req == NULL || req->path == NULL || req->content == NULL) return FS_ERROR_INVALID_PATH;
    if (g_root[0] == '\0') return FS_ERROR_PERMISSION_DENIED;
    if (!safe_relative_path(req->path, relative, sizeof(relative))) return FS_ERROR_INVALID_PATH;
    fd = safe_open_relative(g_root, relative, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0) {
        if (errno == ELOOP || errno == EACCES || errno == EPERM) return FS_ERROR_PERMISSION_DENIED;
        return FS_ERROR_IO_ERROR;
    }
    text = req->content;
    len = strlen(text);
    while (off < len) {
#ifdef _WIN32
        int n = _write(fd, text + off, (unsigned)(len - off));
#else
        ssize_t n = write(fd, text + off, len - off);
#endif
        if (n < 0) {
#ifdef _WIN32
            _close(fd);
#else
            close(fd);
#endif
            return FS_ERROR_IO_ERROR;
        }
        off += (size_t)n;
    }
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
    return FS_ERROR_NONE;
}
