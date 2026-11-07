#include "origin.h"

#include <stdio.h>
#include <string.h>

static char lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static bool starts_with_scheme(const char *uri, const char *scheme, const char **rest) {
    size_t n = strlen(scheme);
    size_t i;
    for (i = 0; i < n; ++i) {
        if (lower_ascii(uri[i]) != scheme[i]) return false;
    }
    if (uri[n] != ':' || uri[n + 1] != '/' || uri[n + 2] != '/') return false;
    *rest = uri + n + 3;
    return true;
}

bool origin_extract(const char *uri, char *out, size_t cap) {
    const char *rest = NULL;
    const char *scheme = NULL;
    const char *host_start;
    const char *host_end;
    const char *cursor;
    int port = -1;
    size_t host_len;
    size_t scheme_len;

    if (uri == NULL || out == NULL || cap < 8) return false;
    if (starts_with_scheme(uri, "https", &rest)) scheme = "https";
    else if (starts_with_scheme(uri, "http", &rest)) scheme = "http";
    else if (starts_with_scheme(uri, "crossweb", &rest)) scheme = "crossweb";
    else return false;

    if (rest[0] == '\0' || rest[0] == '/' || rest[0] == '?' || rest[0] == '#') return false;
    if (strchr(rest, '@') != NULL) {
        const char *slash = strpbrk(rest, "/?#");
        const char *at = strchr(rest, '@');
        if (slash == NULL || at < slash) return false;
    }
    host_start = rest;
    host_end = rest;
    while (*host_end && *host_end != ':' && *host_end != '/' && *host_end != '?' && *host_end != '#') {
        unsigned char c = (unsigned char)*host_end;
        if (c <= 0x20 || c == 0x7f || c == '\\') return false;
        host_end++;
    }
    host_len = (size_t)(host_end - host_start);
    if (host_len == 0 || host_len > 253) return false;

    cursor = host_end;
    if (*cursor == ':') {
        int value = 0;
        int digits = 0;
        cursor++;
        if (*cursor < '0' || *cursor > '9') return false;
        while (*cursor >= '0' && *cursor <= '9') {
            value = value * 10 + (*cursor - '0');
            digits++;
            if (digits > 5 || value > 65535) return false;
            cursor++;
        }
        if (digits == 0 || value < 1 || value > 65535) return false;
        if (*cursor != '\0' && *cursor != '/' && *cursor != '?' && *cursor != '#') return false;
        port = value;
    }

    if (strcmp(scheme, "crossweb") == 0) {
        if (port != -1 || host_len != 3 || memcmp(host_start, "app", 3) != 0) return false;
        if (snprintf(out, cap, "crossweb://app") >= (int)cap) return false;
        return true;
    }

    scheme_len = strlen(scheme);
    if (port == -1) {
        if (scheme_len + 3 + host_len + 1 > cap) return false;
        memcpy(out, scheme, scheme_len);
        memcpy(out + scheme_len, "://", 3);
        memcpy(out + scheme_len + 3, host_start, host_len);
        out[scheme_len + 3 + host_len] = '\0';
        for (size_t i = scheme_len + 3; i < scheme_len + 3 + host_len; ++i) {
            out[i] = lower_ascii(out[i]);
        }
        return true;
    }
    if (snprintf(out, cap, "%s://%.*s:%d", scheme, (int)host_len, host_start, port) >= (int)cap) {
        return false;
    }
    for (char *p = out + scheme_len + 3; *p && *p != ':'; ++p) *p = lower_ascii(*p);
    return true;
}

bool origin_dev_url_allowed(const char *url, char *out, size_t cap) {
    const char *host;
    if (!origin_extract(url, out, cap)) return false;
    if (strncmp(out, "http://", 7) != 0 && strncmp(out, "https://", 8) != 0) return false;
    host = strstr(out, "://");
    if (host == NULL) return false;
    host += 3;
    if (strncmp(host, "localhost:", 10) != 0 && strncmp(host, "127.0.0.1:", 10) != 0) return false;
    return true;
}

bool origin_is_trusted(const char *uri, const char *app_origin, const char *dev_origin) {
    char origin[512];
    if (!origin_extract(uri, origin, sizeof(origin))) return false;
    if (app_origin != NULL && app_origin[0] != '\0' && strcmp(origin, app_origin) == 0) return true;
    if (dev_origin != NULL && dev_origin[0] != '\0' && strcmp(origin, dev_origin) == 0) return true;
    return false;
}
