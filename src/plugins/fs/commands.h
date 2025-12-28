#ifndef FS_COMMANDS_H
#define FS_COMMANDS_H

#include "models.h"
#include "error.h"

bool fs_read_command(const char *payload, char **response);
bool fs_write_command(const char *payload, char **response);

// Platform-specific file operations
FsError fs_read_file(const ReadFileRequest *req, char **content, size_t *size);
FsError fs_write_file(const WriteFileRequest *req);

#endif // FS_COMMANDS_H