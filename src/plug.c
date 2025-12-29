#include "config.h"
#include "plug.h"
#include "ipc.h"
#ifdef CROSSWEB_PLUGIN_KEYSTORE
#include "plugins/keystore/src/plugin.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ANDROID
#include <jni.h>
#endif

// Plugin system
#define MAX_PLUGINS 100
static Plugin *registered_plugins[MAX_PLUGINS];
static int plugin_count = 0;

void plug_register(Plugin *plugin) {
    if (plugin_count < MAX_PLUGINS) {
        registered_plugins[plugin_count++] = plugin;
    } else {
        fprintf(stderr, "Maximum number of plugins reached\n");
    }
}


bool plug_load(const char *path) {
    // TODO: Implement dynamic loading with dlopen/LoadLibrary
    fprintf(stderr, "Dynamic plugin loading not implemented yet\n");
    return false;
}

void plug_init(webview_t wv) {
    if (wv != NULL) {
        ipc_init(wv);  // Set up IPC listener
    }
    // Register built-in plugins
#ifdef CROSSWEB_PLUGIN_KEYSTORE
    plug_register(&keystore_plugin);
#endif

    // Initialize all registered plugins
    PluginContext ctx = { .webview = wv, .platform = "unknown", .config = "{}" };  // TODO: detect platform
    for (int i = 0; i < plugin_count; ++i) {
        if (registered_plugins[i]->init) {
            registered_plugins[i]->init(&ctx);
        }
    }
}

void plug_invoke(const char *cmd, const char *payload, RespondCallback respond) {
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

void plug_emit(const char *event, const char *data) {
#ifdef ANDROID
    // Emit event to JS via JNI
    // Assuming global_env and global_obj are set
    extern JNIEnv *global_env;
    extern jobject global_obj;
    extern jmethodID emitMethodId;
    if (global_env && global_obj && emitMethodId) {
        jstring jevent = (*global_env)->NewStringUTF(global_env, event);
        jstring jdata = (*global_env)->NewStringUTF(global_env, data);
        (*global_env)->CallVoidMethod(global_env, global_obj, emitMethodId, jevent, jdata);
        (*global_env)->DeleteLocalRef(global_env, jevent);
        (*global_env)->DeleteLocalRef(global_env, jdata);
    }
#endif
}

void plug_update(webview_t wv) {
    // Handle IPC messages
    IpcMessage msg;
    while (ipc_receive(&msg)) {
        // Route to plugins
        plug_invoke(msg.cmd, msg.payload, NULL); // For desktop, respond via IPC
    }
    // TODO: Handle platform events and call plugin event handlers
}

void *plug_pre_reload(void) { return NULL; }  // Hotreload hooks
void plug_post_reload(void *state) {}
void plug_cleanup(webview_t wv) {
    // Cleanup all plugins
    for (int i = 0; i < plugin_count; ++i) {
        if (registered_plugins[i]->cleanup) {
            registered_plugins[i]->cleanup();
        }
    }
    ipc_deinit();
}

// Resource loading (keep minimal)
void *plug_load_resource(const char *file_path, size_t *size) {
    // Stub for bundled resources
    return NULL;
}

void plug_free_resource(void *data) {
    // Stub
}

#ifndef CROSSWEB_HOTRELOAD
bool reload_libplug(void) { return true; }
bool reload_libplug_changed(void) { return false; }
#endif // CROSSWEB_HOTRELOAD