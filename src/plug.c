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

#define PLUG_GRANT_CAP 128
typedef struct PlugGrant {
    char plugin[PLUG_NAME_CAP];
    char command[PLUG_NAME_CAP];
} PlugGrant;
static PlugGrant grants[PLUG_GRANT_CAP];
static int grant_count = 0;

static void (*host_emit_event)(const char *event, const char *data_json) = NULL;

static bool plug_token_ok(const char *s) {
    if (s == NULL || s[0] == '\0') return false;
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        unsigned char c = *p;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
        n++;
        if (n >= PLUG_NAME_CAP) return false;
    }
    return true;
}

static void plug_respond_err(RespondCallback respond, const char *json) {
    if (respond) respond(json);
}

void plug_register(Plugin *plugin) {
    if (plugin == NULL || !plug_token_ok(plugin->name)) {
        fprintf(stderr, "plug_register: rejected plugin with a missing or invalid name\n");
        return;
    }
    if (plugin_count < MAX_PLUGINS) {
        // Check for duplicate registration (can happen with hotreload)
        for (int i = 0; i < plugin_count; ++i) {
            if (registered_plugins[i] == plugin ||
                (registered_plugins[i]->name &&
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

bool plug_has(const char *name) {
    if (name == NULL) return false;
    for (int i = 0; i < plugin_count; ++i) {
        if (registered_plugins[i]->name &&
            strcmp(registered_plugins[i]->name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool plug_grant(const char *plugin, const char *command) {
    if (!plug_token_ok(plugin) || !plug_token_ok(command)) return false;
    for (int i = 0; i < grant_count; ++i) {
        if (strcmp(grants[i].plugin, plugin) == 0 &&
            strcmp(grants[i].command, command) == 0) {
            return true;
        }
    }
    if (grant_count >= PLUG_GRANT_CAP) return false;
    memcpy(grants[grant_count].plugin, plugin, strlen(plugin) + 1);
    memcpy(grants[grant_count].command, command, strlen(command) + 1);
    grant_count++;
    return true;
}

void plug_revoke_all(void) {
    grant_count = 0;
}

bool plug_is_allowed(const char *plugin, const char *command) {
    if (plugin == NULL || command == NULL) return false;
    for (int i = 0; i < grant_count; ++i) {
        if (strcmp(grants[i].plugin, plugin) == 0 &&
            strcmp(grants[i].command, command) == 0) {
            return true;
        }
    }
    return false;
}

bool plug_load_grants(const char *path) {
    if (path == NULL) return false;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;

    int saved = grant_count;
    bool ok = true;
    char line[512];
    while (fgets(line, (int)sizeof(line), f) != NULL) {
        size_t n = strlen(line);
        if (n == sizeof(line) - 1 && line[n - 1] != '\n') {
            ok = false;
            break;
        }
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n == 0 || line[0] == '#') continue;
        if (strchr(line, ' ') != NULL || strchr(line, '\t') != NULL) {
            ok = false;
            break;
        }
        char *dot = strchr(line, '.');
        if (dot == NULL || dot == line || strchr(dot + 1, '.') != NULL) {
            ok = false;
            break;
        }
        *dot = '\0';
        if (!plug_grant(line, dot + 1)) {
            ok = false;
            break;
        }
    }
    if (ferror(f)) ok = false;
    fclose(f);
    if (!ok) {
        grant_count = saved;
        return false;
    }
    return true;
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
    // Untrusted web content reaches plugins only through this function.
    // Commands are denied unless the host granted that exact plugin.command.
    if (cmd == NULL) {
        plug_respond_err(respond, "{\"ok\":false,\"error\":\"invalid command format\"}");
        return;
    }
    const char *dot = strchr(cmd, '.');
    if (dot == NULL || dot == cmd || strchr(dot + 1, '.') != NULL) {
        plug_respond_err(respond, "{\"ok\":false,\"error\":\"invalid command format\"}");
        return;
    }
    size_t plugin_len = (size_t)(dot - cmd);
    size_t sub_len = strlen(dot + 1);
    if (plugin_len == 0 || plugin_len >= PLUG_NAME_CAP || sub_len == 0 || sub_len >= PLUG_NAME_CAP) {
        plug_respond_err(respond, "{\"ok\":false,\"error\":\"invalid command format\"}");
        return;
    }
    char plugin_name[PLUG_NAME_CAP];
    char subcmd[PLUG_NAME_CAP];
    memcpy(plugin_name, cmd, plugin_len);
    plugin_name[plugin_len] = '\0';
    memcpy(subcmd, dot + 1, sub_len + 1);
    if (!plug_token_ok(plugin_name) || !plug_token_ok(subcmd)) {
        plug_respond_err(respond, "{\"ok\":false,\"error\":\"invalid command format\"}");
        return;
    }
    if (!plug_is_allowed(plugin_name, subcmd)) {
        fprintf(stderr, "plug_invoke: denied %s.%s\n", plugin_name, subcmd);
        plug_respond_err(respond, "{\"ok\":false,\"error\":\"permission denied\"}");
        return;
    }

    for (int i = 0; i < plugin_count; ++i) {
        Plugin *p = registered_plugins[i];
        if (p->name && strcmp(p->name, plugin_name) == 0) {
            if (p->invoke) {
                p->invoke(subcmd, payload ? payload : "", respond);
            } else {
                plug_respond_err(respond, "{\"ok\":false,\"error\":\"plugin has no invoke\"}");
            }
            return;
        }
    }
    plug_respond_err(respond, "{\"ok\":false,\"error\":\"unknown plugin\"}");
}

CROSSWEB_API void plug_emit(const char *event, const char *data) {
#ifdef ANDROID
    // Delegate Android emission to android_bridge.c helper
    android_emit(event, data ? data : "null");
#elif defined(_WIN32)
    if (event && host_emit_event) {
        host_emit_event(event, data ? data : "null");
    }
#else
    (void)event;
    (void)data;
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

#if !CROSSWEB_HOTRELOAD
bool reload_libplug(void) { return true; }
bool reload_libplug_changed(void) { return false; }
#endif // CROSSWEB_HOTRELOAD