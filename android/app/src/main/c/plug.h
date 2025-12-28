#ifndef PLUG_H_
#define PLUG_H_

#include <stddef.h>
typedef void* webview_t;

typedef void (*RespondCallback)(const char *response);

typedef struct Plugin {
    const char *name;
    void (*invoke)(const char *cmd, const char *payload, RespondCallback respond);
} Plugin;

#define LIST_OF_PLUGS \
    PLUG(plug_init, void, webview_t) \
    PLUG(plug_pre_reload, void*, void) \
    PLUG(plug_post_reload, void, void*) \
    PLUG(plug_load_resource, void*, const char*, size_t*) \
    PLUG(plug_free_resource, void, void*) \
    PLUG(plug_update, void, webview_t) \
    PLUG(plug_cleanup, void, webview_t) \
    PLUG(plug_invoke, void, const char*, const char*, RespondCallback)

#define PLUG(name, ret, ...) typedef ret (name##_t)(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#define PLUG(name, ret, ...) extern ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#endif // PLUG_H_
