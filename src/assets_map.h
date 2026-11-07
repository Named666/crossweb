#ifndef ASSETS_MAP_H_
#define ASSETS_MAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Read a bundled file for a URL path such as "/" or "/app.js".
   Rejects traversal, encoded dots, unknown suffixes, and symlinks.
   Caller frees *data. On success *mime is a static string. */
bool assets_read(const char *root, const char *url_path, unsigned char **data, size_t *len, const char **mime, const char **err);

/* Find a directory that contains index.html. Checks CROSSWEB_ASSETS, ./assets,
   and assets next to the executable. */
bool assets_find_root(const char *argv0, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
