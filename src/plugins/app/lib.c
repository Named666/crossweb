#include "../../plug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int app_ping_entered = 0;

static bool nonce_ok(const char *value, size_t len) {
    size_t i;
    if (len == 0 || len > 32) return false;
    for (i = 0; i < len; ++i) {
        char c = value[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

static bool app_init(PluginContext *ctx) {
    (void)ctx;
    printf("App plugin initialized\n");
    return true;
}

static bool app_invoke(const char *command, const char *payload, RespondCallback respond) {
    const char *prefix = "{\"nonce\":\"";
    size_t prefix_len = strlen(prefix);
    const char *start;
    const char *end;
    char token[33];
    char response[96];
    app_ping_entered++;
    if (command == NULL || strcmp(command, "ping") != 0) {
        if (respond) respond("{\"ok\":false,\"error\":\"unknown command\"}");
        return false;
    }
    if (payload == NULL || strncmp(payload, prefix, prefix_len) != 0) {
        if (respond) respond("{\"ok\":false,\"error\":\"invalid payload\"}");
        return false;
    }
    start = payload + prefix_len;
    end = strchr(start, '"');
    if (end == NULL || strcmp(end, "\"}") != 0 || !nonce_ok(start, (size_t)(end - start))) {
        if (respond) respond("{\"ok\":false,\"error\":\"invalid payload\"}");
        return false;
    }
    memcpy(token, start, (size_t)(end - start));
    token[end - start] = '\0';
    snprintf(response, sizeof(response), "{\"ok\":true,\"pong\":\"%s\"}", token);
    if (respond) respond(response);
    return true;
}

static void app_cleanup(void) {}

static Plugin app_plugin = {
    .name = "app",
    .version = 100,
    .init = app_init,
    .invoke = app_invoke,
    .event = NULL,
    .cleanup = app_cleanup
};

PLUG_REGISTER(app_plugin)
