#ifndef HOST_CONFIG_H_
#define HOST_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Load crossweb.permissions and, when dev_url is non-empty, accept it only as
   an explicit localhost dev origin. Returns 0 on success. On failure *err is
   a static message and no grants from this call remain. */
int host_prepare(const char *grant_path, const char *dev_url, char *dev_origin, size_t cap, const char **err);

#ifdef __cplusplus
}
#endif

#endif
