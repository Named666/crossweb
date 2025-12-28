#include "error.h"

const char *fs_error_to_string(FsError error) {
    switch (error) {
        case FS_ERROR_NONE: return "No error";
        case FS_ERROR_FILE_NOT_FOUND: return "File not found";
        case FS_ERROR_PERMISSION_DENIED: return "Permission denied";
        case FS_ERROR_INVALID_PATH: return "Invalid path";
        case FS_ERROR_IO_ERROR: return "I/O error";
        default: return "Unknown error";
    }
}