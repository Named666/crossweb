#ifndef PLUG_H_
#define PLUG_H_

#include <stddef.h>
#include <stdbool.h>
typedef void* webview_t;

typedef void (*RespondCallback)(const char *response);

typedef struct PluginContext {
    webview_t webview;
    const char *platform;  // "android", "ios", "windows", "macos", "linux"
    const char *config;    // JSON config string
} PluginContext;

typedef struct Plugin {
    const char *name;      // Plugin identifier (e.g., "fs", "dialog")
    int version;           // Semantic version (e.g., 100 for 1.0.0)
    bool (*init)(PluginContext *ctx);  // Initialize plugin, return true on success
    bool (*invoke)(const char *command, const char *payload, RespondCallback respond);  // Handle commands
    void (*event)(const char *event, const char *data);  // Handle events
    void (*cleanup)(void);  // Cleanup resources
} Plugin;

#define LIST_OF_PLUGS \
    PLUG(plug_init, void, webview_t) \
    PLUG(plug_register, void, Plugin*) \
    PLUG(plug_load, bool, const char*) \
    PLUG(plug_pre_reload, void*, void) \
    PLUG(plug_post_reload, void, void*) \
    PLUG(plug_load_resource, void*, const char*, size_t*) \
    PLUG(plug_free_resource, void, void*) \
    PLUG(plug_update, void, webview_t) \
    PLUG(plug_cleanup, void, webview_t) \
    PLUG(plug_invoke, void, const char*, const char*, RespondCallback) \
    PLUG(plug_emit, void, const char*, const char*)

#define PLUG(name, ret, ...) typedef ret (name##_t)(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#define PLUG(name, ret, ...) extern ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#ifdef ANDROID
#include <jni.h>
extern JNIEnv *global_env;
extern jobject global_obj;
extern jmethodID emitMethodId;
#endif

#endif // PLUG_H_
