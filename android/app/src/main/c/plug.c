#include "plug.h"
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Plugin system
// Example core plugins
void fs_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    // JSON parse payload, read file, respond
    // For now, stub
    printf("FS invoke: %s %s\n", cmd, payload);
    char response[256];
    snprintf(response, sizeof(response), "{\"data\":\"stub data for %s\"}", payload);
    respond(response);
}

void keystore_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    // Simple in-memory keystore stub
    printf("Keystore invoke: %s %s\n", cmd, payload);
    char response[256];
    if (strcmp(cmd, "get") == 0) {
        snprintf(response, sizeof(response), "{\"data\":\"value for %s\"}", payload);
    } else {
        snprintf(response, sizeof(response), "{\"error\":\"unknown keystore command\"}");
    }
    respond(response);
}

static Plugin plugins[] = {
    {"fs", fs_invoke},
    {"keystore", keystore_invoke},
    {NULL, NULL} // Sentinel
};

void plug_init(webview_t wv) {
    if (wv != NULL) {
        ipc_init(wv);  // Set up IPC listener
    }
    // Load plugins (e.g., fs, dialog)
}

void plug_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    // Parse cmd, e.g., "fs.read" -> plugin "fs", subcmd "read"
    char *dot = strchr(cmd, '.');
    if (!dot) {
        respond("{\"error\":\"invalid command format\"}");
        return;
    }
    size_t plugin_len = dot - cmd;
    char plugin_name[256];
    strncpy(plugin_name, cmd, plugin_len);
    plugin_name[plugin_len] = '\0';
    const char *subcmd = dot + 1;

    for (Plugin *p = plugins; p->name; ++p) {
        if (strcmp(p->name, plugin_name) == 0) {
            p->invoke(subcmd, payload, respond);
            return;
        }
    }
    respond("{\"error\":\"unknown plugin\"}");
}

void plug_update(webview_t wv) {
    // Handle IPC messages
    IpcMessage msg;
    while (ipc_receive(&msg)) {
        // Route to plugins
        plug_invoke(msg.cmd, msg.payload, NULL); // For desktop, respond via IPC
    }
}

void *plug_pre_reload(void) { return NULL; }  // Hotreload hooks
void plug_post_reload(void *state) {}
void plug_cleanup(webview_t wv) { ipc_deinit(); }

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