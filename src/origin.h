#ifndef ORIGIN_H_
#define ORIGIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define CROSSWEB_APP_ORIGIN "crossweb://app"
#define CROSSWEB_APP_PAGE "crossweb://app/index.html"
#define CROSSWEB_WINDOWS_APP_ORIGIN "https://crossweb.localhost"
#define CROSSWEB_ANDROID_APP_ORIGIN "https://appassets.androidplatform.net"

/* Origin without a path: "https://127.0.0.1:5173" or "crossweb://app".
   crossweb URIs are accepted only for host "app". http(s) userinfo is rejected. */
bool origin_extract(const char *uri, char *out, size_t cap);

/* Dev content must be an explicit http(s) localhost or 127.0.0.1 URL with a port. */
bool origin_dev_url_allowed(const char *url, char *out, size_t cap);

/* True when uri's origin equals app_origin, or dev_origin when that is non-empty. */
bool origin_is_trusted(const char *uri, const char *app_origin, const char *dev_origin);

#ifdef __cplusplus
}
#endif

#endif
