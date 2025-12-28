#ifndef FS_MODELS_H
#define FS_MODELS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct FileInfo {
    char *path;
    size_t size;
    bool is_directory;
    long modified_time;
} FileInfo;

typedef struct ReadFileRequest {
    const char *path;
    bool binary;
} ReadFileRequest;

typedef struct WriteFileRequest {
    const char *path;
    const char *content;
    bool binary;
} WriteFileRequest;

#endif // FS_MODELS_H