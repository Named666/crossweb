#ifndef SAFE_OPEN_H_
#define SAFE_OPEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define SAFE_OPEN_PATH_MAX 4096

/* Normalize a relative path. Rejects absolute paths, backslashes, empty
   results, and any ".." that would leave the root. */
bool safe_relative_path(const char *input, char *out, size_t cap);

/* Open root/relative without following symlinks. relative must already be
   normalized. Returns a file descriptor or -1 with errno set. */
int safe_open_relative(const char *root, const char *relative, int flags, int mode);

/* Read up to cap bytes and NUL-terminate. Caller frees *data. */
bool safe_read_file(const char *root, const char *relative, unsigned char **data, size_t *len, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
