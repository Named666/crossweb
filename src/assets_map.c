#define _XOPEN_SOURCE 700

#include "assets_map.h"

#include "safe_open.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static const char *mime_for(const char *relative) {
    const char *dot = strrchr(relative, '.');
    if (dot == NULL) return NULL;
    if (strcmp(dot, ".html") == 0) return "text/html;charset=utf-8";
    if (strcmp(dot, ".css") == 0) return "text/css;charset=utf-8";
    if (strcmp(dot, ".js") == 0) return "text/javascript;charset=utf-8";
    if (strcmp(dot, ".svg") == 0) return "image/svg+xml";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".ico") == 0) return "image/x-icon";
    return NULL;
}

static bool map_url_path(const char *url_path, char *relative, size_t cap) {
    const char *path = url_path;
    if (path == NULL || path[0] == '\0' || strcmp(path, "/") == 0) {
        if (cap < 11) return false;
        memcpy(relative, "index.html", 11);
        return true;
    }
    if (strchr(path, '%') != NULL || strchr(path, '?') != NULL || strchr(path, '#') != NULL) {
        return false;
    }
    while (*path == '/') path++;
    if (!safe_relative_path(path, relative, cap)) return false;
    return mime_for(relative) != NULL;
}

bool assets_read(const char *root, const char *url_path, unsigned char **data, size_t *len, const char **mime, const char **err) {
    char relative[SAFE_OPEN_PATH_MAX];
    const char *kind;

    if (err) *err = NULL;
    if (mime) *mime = NULL;
    if (data) *data = NULL;
    if (root == NULL || root[0] == '\0') {
        if (err) *err = "assets root is missing";
        return false;
    }
    if (!map_url_path(url_path, relative, sizeof(relative))) {
        if (err) *err = "asset path rejected";
        return false;
    }
    kind = mime_for(relative);
    if (kind == NULL) {
        if (err) *err = "asset type rejected";
        return false;
    }
    if (!safe_read_file(root, relative, data, len, 1024 * 1024)) {
        if (err) *err = "asset unreadable";
        return false;
    }
    if (mime) *mime = kind;
    return true;
}

static bool has_index(const char *dir) {
    unsigned char *data = NULL;
    size_t len = 0;
    bool ok = assets_read(dir, "/index.html", &data, &len, NULL, NULL);
    free(data);
    return ok && len > 0;
}

bool assets_find_root(const char *argv0, char *out, size_t cap) {
    const char *env = getenv("CROSSWEB_ASSETS");
    char exe[SAFE_OPEN_PATH_MAX];
    char candidate[SAFE_OPEN_PATH_MAX];

    if (out == NULL || cap < 2) return false;
    if (env != NULL && env[0] != '\0' && has_index(env)) {
        if (snprintf(out, cap, "%s", env) >= (int)cap) return false;
        return true;
    }
    if (has_index("assets")) {
        if (snprintf(out, cap, "assets") >= (int)cap) return false;
        return true;
    }
    exe[0] = '\0';
#ifdef _WIN32
    if (GetModuleFileNameA(NULL, exe, sizeof(exe)) == 0) exe[0] = '\0';
#else
    if (argv0 != NULL) {
        char *real = realpath(argv0, NULL);
        if (real != NULL) {
            snprintf(exe, sizeof(exe), "%s", real);
            free(real);
        } else {
            snprintf(exe, sizeof(exe), "%s", argv0);
        }
    }
#endif
    if (exe[0] != '\0') {
        char *slash = strrchr(exe, '/');
#ifdef _WIN32
        char *bslash = strrchr(exe, '\\');
        if (bslash != NULL && (slash == NULL || bslash > slash)) slash = bslash;
#endif
        if (slash != NULL) {
            *slash = '\0';
            if (snprintf(candidate, sizeof(candidate), "%s/assets", exe) < (int)sizeof(candidate) && has_index(candidate)) {
                if (snprintf(out, cap, "%s", candidate) >= (int)cap) return false;
                return true;
            }
            if (snprintf(candidate, sizeof(candidate), "%s/../assets", exe) < (int)sizeof(candidate)) {
#ifdef _WIN32
                char resolved[SAFE_OPEN_PATH_MAX];
                if (GetFullPathNameA(candidate, sizeof(resolved), resolved, NULL) > 0 && has_index(resolved)) {
                    if (snprintf(out, cap, "%s", resolved) >= (int)cap) return false;
                    return true;
                }
#else
                char *real = realpath(candidate, NULL);
                if (real != NULL && has_index(real)) {
                    bool fit = snprintf(out, cap, "%s", real) < (int)cap;
                    free(real);
                    return fit;
                }
                free(real);
#endif
            }
        }
    }
    (void)argv0;
    return false;
}
