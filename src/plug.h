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

// Export control for the hotreload DLL.
// - When building the plugin runtime DLL on Windows, we must explicitly export
//   the entrypoints so GetProcAddress can find them.
// - When building the host executable, these should be normal declarations.
#if defined(_WIN32) && defined(CROSSWEB_BUILDING_PLUG)
    #define CROSSWEB_API __declspec(dllexport)
#else
    #define CROSSWEB_API
#endif

// ============================================================================
// AUTO-REGISTRATION SYSTEM
// ============================================================================
// Plugins self-register using constructor functions that run before main().
// This allows plugins to be added to the build without modifying plug.c.
//
// Usage in plugin lib.c:
//   PLUG_REGISTER(my_plugin)
//
// Where my_plugin is a Plugin struct defined in the same file.
// ============================================================================

// Cross-platform constructor attribute
#ifdef _MSC_VER
    // MSVC uses a special section for initialization functions
    #pragma section(".CRT$XCU", read)
    #define PLUG_CONSTRUCTOR(fn) \
        static void fn(void); \
        __declspec(allocate(".CRT$XCU")) void (*fn##_ptr)(void) = fn; \
        static void fn(void)
#else
    // GCC/Clang use __attribute__((constructor))
    #define PLUG_CONSTRUCTOR(fn) \
        __attribute__((constructor)) static void fn(void)
#endif

// Register a plugin at load time. Place this at the end of your plugin's lib.c.
// The plugin struct must be defined before this macro is used.
#define PLUG_REGISTER(plugin_var) \
    PLUG_CONSTRUCTOR(plugin_var##_register_fn) { \
        plug_register(&plugin_var); \
    }

// Internal/non-hot-reload APIs.
// These are intentionally not part of LIST_OF_PLUGS to keep the hotreload DLL
// boundary small, but they remain part of the public header so plugins can
// compile and the core can evolve without breaking source compatibility.
void plug_register(Plugin *plugin);
bool plug_load(const char *path);
void *plug_load_resource(const char *file_path, size_t *size);
void plug_free_resource(void *data);

#define LIST_OF_PLUGS \
    PLUG(plug_init, void, webview_t) \
    PLUG(plug_pre_reload, void*, void) \
    PLUG(plug_post_reload, void, void*) \
    PLUG(plug_update, void, webview_t) \
    PLUG(plug_invoke, void, const char*, const char*, RespondCallback) \
    PLUG(plug_emit, void, const char*, const char*) \
    PLUG(plug_set_host_emit_event, void, void (*)(const char *event, const char *data_json)) \
    PLUG(plug_cleanup, void, webview_t)

#define PLUG(name, ret, ...) typedef ret (name##_t)(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#if defined(CROSSWEB_HOTRELOAD) && !defined(CROSSWEB_BUILDING_PLUG)
    #define PLUG(name, ret, ...) extern name##_t *name;
#else
    #define PLUG(name, ret, ...) CROSSWEB_API ret name(__VA_ARGS__);
#endif
LIST_OF_PLUGS
#undef PLUG

#ifdef ANDROID
#include <jni.h>
extern JNIEnv *global_env;
extern jobject global_obj;
extern jmethodID emitMethodId;
#endif

#endif // PLUG_H_
