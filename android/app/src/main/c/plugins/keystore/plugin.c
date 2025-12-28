#include "plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Forward declarations for JNI bridge (to be implemented in Android)
bool android_keystore_bio_invoke(const char *cmd, const char *payload, RespondCallback respond);

static bool keystore_bio_init(PluginContext *ctx) {
    // Initialization for Android keystore plugin (if needed)
    printf("Keystore Biometric plugin initialized\n");
    return true;
}

static bool keystore_bio_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    // Forward to JNI/Android implementation
    return android_keystore_bio_invoke(cmd, payload, respond);
}

static void keystore_bio_event(const char *event, const char *data) {
    // No-op for now
}

static void keystore_bio_cleanup(void) {
    // Cleanup if needed
}

Plugin keystore_bio_plugin = {
    .name = "keystore_bio",
    .version = 100,
    .init = keystore_bio_init,
    .invoke = keystore_bio_invoke,
    .event = keystore_bio_event,
    .cleanup = keystore_bio_cleanup
};
