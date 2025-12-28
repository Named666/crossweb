#ifndef FS_ERROR_H
#define FS_ERROR_H

typedef enum {
    FS_ERROR_NONE = 0,
    FS_ERROR_FILE_NOT_FOUND,
    FS_ERROR_PERMISSION_DENIED,
    FS_ERROR_INVALID_PATH,
    FS_ERROR_IO_ERROR,
    FS_ERROR_OUT_OF_MEMORY,
    FS_ERROR_UNKNOWN
} FsError;

const char *fs_error_to_string(FsError error);

#endif // FS_ERROR_H