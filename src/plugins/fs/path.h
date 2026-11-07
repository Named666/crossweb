#ifndef FS_PATH_H_
#define FS_PATH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "models.h"

#include <stdbool.h>
#include <stddef.h>

/* Number of times fs_invoke was entered. The permission gate must keep this
   at zero for commands that were not granted. */
extern int fs_commands_entered;

bool fs_set_root(const char *root);
const char *fs_get_root(void);
void fs_clear_root(void);
bool fs_ensure_user_root(void);

FsError fs_read_file(const ReadFileRequest *req, char **content, size_t *size);
FsError fs_write_file(const WriteFileRequest *req);

#ifdef __cplusplus
}
#endif

#endif
