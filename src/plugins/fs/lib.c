#include "../../plug.h"
#include "models.h"
#include "error.h"
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform detection
#ifdef _WIN32
#define PLATFORM "windows"
#include "desktop.c"
#elif defined(__APPLE__)
#define PLATFORM "macos"
#include "desktop.c"  // For now, use desktop on macOS
#elif defined(__ANDROID__)
#define PLATFORM "android"
#include "mobile.c"
#elif defined(__linux__)
#define PLATFORM "linux"
#include "desktop.c"
#else
#define PLATFORM "unknown"
#include "desktop.c"
#endif

// Plugin functions
bool fs_init(PluginContext *ctx) {
    printf("FS plugin initialized on %s\n", PLATFORM);
    return true;
}

bool fs_invoke(const char *command, const char *payload, RespondCallback respond) {
    char *response = NULL;
    bool success = false;
    if (strcmp(command, "read") == 0) {
        success = fs_read_command(payload, &response);
    } else if (strcmp(command, "write") == 0) {
        success = fs_write_command(payload, &response);
    } else {
        response = strdup("{\"error\":\"unknown command\"}");
    }
    if (respond && response) {
        respond(response);
        free(response);
    }
    return success;
}

void fs_event(const char *event, const char *data) {
    // Handle file system events
    printf("FS event: %s %s\n", event, data);
}

void fs_cleanup(void) {
    printf("FS plugin cleanup\n");
}

// Plugin struct
Plugin fs_plugin = {
    .name = "fs",
    .version = 100,
    .init = fs_init,
    .invoke = fs_invoke,
    .event = fs_event,
    .cleanup = fs_cleanup
};