#include "../../plug.h"
#include "models.h"
#include "error.h"
#include "commands.h"
#include "path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PLATFORM "windows"
#elif defined(__APPLE__)
#define PLATFORM "macos"
#elif defined(__ANDROID__)
#define PLATFORM "android"
#elif defined(__linux__)
#define PLATFORM "linux"
#else
#define PLATFORM "unknown"
#endif

int fs_commands_entered = 0;

bool fs_init(PluginContext *ctx) {
    (void)ctx;
#ifndef __ANDROID__
    if (!fs_ensure_user_root()) {
        fprintf(stderr, "FS plugin: app data directory is unavailable; fs commands fail closed\n");
    }
#endif
    printf("FS plugin initialized on %s\n", PLATFORM);
    return true;
}

bool fs_invoke(const char *command, const char *payload, RespondCallback respond) {
    char *response = NULL;
    bool success = false;
    fs_commands_entered++;
    if (command == NULL) {
        response = strdup("{\"ok\":false,\"error\":\"invalid command\"}");
    } else if (strcmp(command, "read") == 0) {
        success = fs_read_command(payload, &response);
    } else if (strcmp(command, "write") == 0) {
        success = fs_write_command(payload, &response);
    } else {
        response = strdup("{\"ok\":false,\"error\":\"unknown command\"}");
    }
    if (respond && response) {
        respond(response);
        free(response);
    }
    return success;
}

void fs_event(const char *event, const char *data) {
    (void)event;
    (void)data;
}

void fs_cleanup(void) {
    printf("FS plugin cleanup\n");
}

Plugin fs_plugin = {
    .name = "fs",
    .version = 100,
    .init = fs_init,
    .invoke = fs_invoke,
    .event = fs_event,
    .cleanup = fs_cleanup
};

PLUG_REGISTER(fs_plugin)
