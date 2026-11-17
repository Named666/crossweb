#include "../../../plug.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool nostr_init(PluginContext *ctx) {
    (void)ctx;
    printf("Nostr plugin initialized\n");
    return true;
}

static bool nostr_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    (void)cmd;
    (void)payload;
    if (respond) {
        respond("{\"ok\":false,\"msg\":\"Nostr NIP-07 is exposed through the JavaScript window.nostr API\"}");
    }
    return false;
}

static void nostr_event(const char *event, const char *data) {
    (void)event;
    (void)data;
}

static void nostr_cleanup(void) {
}

Plugin nostr_plugin = {
    .name = "nostr",
    .version = 100,
    .init = nostr_init,
    .invoke = nostr_invoke,
    .event = nostr_event,
    .cleanup = nostr_cleanup
};

PLUG_REGISTER(nostr_plugin)
