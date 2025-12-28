#include "models.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mobile-specific implementations (JNI for Android, Swift for iOS)
// For Android, use standard C file operations with app's data directory

FsError fs_read_file(const ReadFileRequest *req, char **content, size_t *size) {
    FILE *file = fopen(req->path, req->binary ? "rb" : "r");
    if (!file) {
        return FS_ERROR_FILE_NOT_FOUND;
    }
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *content = malloc(*size + 1);
    if (!*content) {
        fclose(file);
        return FS_ERROR_OUT_OF_MEMORY;
    }
    fread(*content, 1, *size, file);
    if (!req->binary) {
        (*content)[*size] = '\0';
    }
    fclose(file);
    return FS_ERROR_NONE;
}

FsError fs_write_file(const WriteFileRequest *req) {
    FILE *file = fopen(req->path, req->binary ? "wb" : "w");
    if (!file) {
        return FS_ERROR_PERMISSION_DENIED;
    }
    size_t written = fwrite(req->content, 1, strlen(req->content), file);
    fclose(file);
    return written == strlen(req->content) ? FS_ERROR_NONE : FS_ERROR_IO_ERROR;
}

FsError fs_get_file_info(const char *path, FileInfo *info) {
    // Stub for now
    info->path = strdup(path);
    info->size = 0;
    info->is_directory = false;
    info->modified_time = 0;
    return FS_ERROR_NONE;
}