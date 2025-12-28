#include "commands.h"
#include "models.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON parsing stub (replace with proper JSON lib)
static char *parse_json_string(const char *json, const char *key) {
    // Very basic parser for "key":"value"
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"([^\"]+)\"", key);
    // For simplicity, assume json is {"path":"value"}
    const char *start = strstr(json, "\"");
    if (!start) return NULL;
    start = strstr(start + 1, "\"");
    if (!start) return NULL;
    start++;
    const char *end = strstr(start, "\"");
    if (!end) return NULL;
    size_t len = end - start;
    char *value = malloc(len + 1);
    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

bool fs_read_command(const char *payload, char **response) {
    char *path = parse_json_string(payload, "path");
    if (!path) {
        *response = strdup("{\"error\":\"invalid payload\"}");
        return false;
    }
    ReadFileRequest req = { .path = path, .binary = false };
    char *content;
    size_t size;
    FsError err = fs_read_file(&req, &content, &size);
    free(path);
    if (err != FS_ERROR_NONE) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", fs_error_to_string(err));
        *response = strdup(buf);
        return false;
    }
    // JSON encode content
    char *json = malloc(size + 256);
    snprintf(json, size + 256, "{\"data\":\"%s\"}", content);
    free(content);
    *response = json;
    return true;
}

bool fs_write_command(const char *payload, char **response) {
    char *path = parse_json_string(payload, "path");
    char *content = parse_json_string(payload, "content");
    if (!path || !content) {
        free(path);
        free(content);
        *response = strdup("{\"error\":\"invalid payload\"}");
        return false;
    }
    WriteFileRequest req = { .path = path, .content = content, .binary = false };
    FsError err = fs_write_file(&req);
    free(path);
    free(content);
    if (err != FS_ERROR_NONE) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", fs_error_to_string(err));
        *response = strdup(buf);
        return false;
    }
    *response = strdup("{\"success\":true}");
    return true;
}