#include "host_config.h"

#include "origin.h"
#include "plug.h"

#include <string.h>

int host_prepare(const char *grant_path, const char *dev_url, char *dev_origin, size_t cap, const char **err) {
    if (err) *err = NULL;
    if (dev_origin != NULL && cap > 0) dev_origin[0] = '\0';
    plug_revoke_all();
    if (grant_path == NULL || !plug_load_grants(grant_path)) {
        if (err) *err = "capability file is missing or rejected";
        return 1;
    }
    if (dev_url != NULL && dev_url[0] != '\0') {
        if (dev_origin == NULL || cap < 16 || !origin_dev_url_allowed(dev_url, dev_origin, cap)) {
            plug_revoke_all();
            if (dev_origin != NULL && cap > 0) dev_origin[0] = '\0';
            if (err) *err = "dev URL is not an explicit localhost origin";
            return 1;
        }
    }
    return 0;
}
