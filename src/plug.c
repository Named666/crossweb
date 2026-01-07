// ============================================================================
// plug.c - Core plugin system
// ============================================================================
// Plugins are auto-registered at load time via PLUG_REGISTER() macro.
// No manual includes or registration calls needed here.
// See plug.h for the registration system documentation.
// ============================================================================

#include "plug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ANDROID
#include <jni.h>
#include <stdbool.h>
// android_emit is implemented in android_bridge.c to centralize JNI emission logic
extern void android_emit(const char *event, const char *data);
#endif

// Plugin registry - populated by constructor functions before main()
#define MAX_PLUGINS 100
static Plugin *registered_plugins[MAX_PLUGINS];
static int plugin_count = 0;

static void (*host_emit_event)(const char *event, const char *data_json) = NULL;

void plug_register(Plugin *plugin) {
    if (plugin == NULL) {
        fprintf(stderr, "plug_register: NULL plugin\n");
        return;
    }
    if (plugin_count < MAX_PLUGINS) {
        // Check for duplicate registration (can happen with hotreload)
        for (int i = 0; i < plugin_count; ++i) {
            if (registered_plugins[i] == plugin ||
                (registered_plugins[i]->name && plugin->name &&
                 strcmp(registered_plugins[i]->name, plugin->name) == 0)) {
                return; // Already registered
            }
        }
        registered_plugins[plugin_count++] = plugin;
        printf("Plugin registered: %s (v%d)\n", plugin->name, plugin->version);
    } else {
        fprintf(stderr, "Maximum number of plugins reached\n");
    }
}


bool plug_load(const char *path) {
    (void)path;
    // TODO: Implement dynamic loading with dlopen/LoadLibrary
    fprintf(stderr, "Dynamic plugin loading not implemented yet\n");
    return false;
}

CROSSWEB_API void plug_set_host_emit_event(void (*emit)(const char *event, const char *data_json)) {
    host_emit_event = emit;
}

CROSSWEB_API void plug_init(webview_t wv) {
    // Plugins are already registered via constructors (PLUG_REGISTER macro).
    // We just need to initialize them here.

    // Detect platform for context
    const char *platform = "unknown";
#ifdef _WIN32
    platform = "windows";
#elif defined(__APPLE__)
    platform = "macos";
#elif defined(__ANDROID__)
    platform = "android";
#elif defined(__linux__)
    platform = "linux";
#endif

    // Initialize all registered plugins
    PluginContext ctx = { .webview = wv, .platform = platform, .config = "{}" };
    for (int i = 0; i < plugin_count; ++i) {
        if (registered_plugins[i]->init) {
            registered_plugins[i]->init(&ctx);
        }
    }
}

CROSSWEB_API void plug_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    fprintf(stderr, "plug_invoke: cmd=%s payload=%s\n", cmd ? cmd : "NULL", payload ? payload : "NULL");
    // Parse cmd, e.g., "fs.read" -> plugin "fs", subcmd "read"
    char *dot = strchr(cmd, '.');
    if (!dot) {
        if (respond) respond("{\"error\":\"invalid command format\"}");
        return;
    }
    size_t plugin_len = dot - cmd;
    char plugin_name[256];
    strncpy(plugin_name, cmd, plugin_len);
    plugin_name[plugin_len] = '\0';
    const char *subcmd = dot + 1;

    for (int i = 0; i < plugin_count; ++i) {
        Plugin *p = registered_plugins[i];
        if (strcmp(p->name, plugin_name) == 0) {
            if (p->invoke) {
                p->invoke(subcmd, payload, respond);
            }
            return;
        }
    }
    if (respond) respond("{\"error\":\"unknown plugin\"}");
}

CROSSWEB_API void plug_emit(const char *event, const char *data) {
#ifdef ANDROID
    // Delegate Android emission to android_bridge.c helper
    android_emit(event, data ? data : "null");
#elif defined(_WIN32)
    if (event && host_emit_event) {
        host_emit_event(event, data ? data : "null");
    }
#endif
}

CROSSWEB_API void plug_update(webview_t wv) {
    (void)wv;
    // Intentionally minimal: the host owns IPC and calls plug_invoke directly.
}

CROSSWEB_API void *plug_pre_reload(void) { return NULL; }  // Hotreload hooks
CROSSWEB_API void plug_post_reload(void *state) {
    (void)state;
}
CROSSWEB_API void plug_cleanup(webview_t wv) {
    (void)wv;
    // Cleanup all plugins
    for (int i = 0; i < plugin_count; ++i) {
        if (registered_plugins[i]->cleanup) {
            registered_plugins[i]->cleanup();
        }
    }
}

// Resource loading (keep minimal)
void *plug_load_resource(const char *file_path, size_t *size) {
    (void)file_path;
    (void)size;
    // Stub for bundled resources
    return NULL;
}

void plug_free_resource(void *data) {
    (void)data;
    // Stub
}

#ifndef CROSSWEB_HOTRELOAD
bool reload_libplug(void) { return true; }
bool reload_libplug_changed(void) { return false; }
#endif // CROSSWEB_HOTRELOAD