/* Host-independent regression tests for the IPC framer and the deny-by-default
   plugin gate. This file does not open a WebView and does not grant filesystem
   or keystore commands. */
#include "ipc.h"
#include "plug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static int invoke_count = 0;
static int payload_was_null = 0;
static char seen_cmd[PLUG_NAME_CAP];
static char seen_payload[IPC_MAX_PAYLOAD_LEN];
static const char *last_response = NULL;

static void capture(const char *response) {
    last_response = response;
}

static bool test_init(PluginContext *ctx) {
    (void)ctx;
    return true;
}

static bool test_invoke(const char *command, const char *payload, RespondCallback respond) {
    invoke_count++;
    snprintf(seen_cmd, sizeof(seen_cmd), "%s", command ? command : "");
    if (payload == NULL) {
        payload_was_null = 1;
        seen_payload[0] = '\0';
    } else {
        snprintf(seen_payload, sizeof(seen_payload), "%s", payload);
    }
    if (respond) respond("{\"ok\":true}");
    return true;
}

static Plugin test_plugin = {
    .name = "testplug",
    .version = 1,
    .init = test_init,
    .invoke = test_invoke,
    .event = NULL,
    .cleanup = NULL,
};

PLUG_REGISTER(test_plugin)

static void reset_case(void) {
    plug_revoke_all();
    invoke_count = 0;
    payload_was_null = 0;
    seen_cmd[0] = '\0';
    seen_payload[0] = '\0';
    last_response = NULL;
    ipc_init(NULL);
}

static bool write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) return false;
    size_t n = strlen(text);
    bool ok = fwrite(text, 1, n, f) == n;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static void expect_response(const char *expected) {
    CHECK(last_response != NULL && strcmp(last_response, expected) == 0);
}

static void test_registry(void) {
    CHECK(plug_has("testplug"));
    CHECK(!plug_has("missing"));
    CHECK(!plug_has(NULL));

    Plugin spaced = {.name = "has space", .version = 1};
    Plugin empty = {.name = "", .version = 1};
    plug_register(NULL);
    plug_register(&spaced);
    plug_register(&empty);
    CHECK(!plug_has("has space"));
    CHECK(!plug_has(""));
    CHECK(plug_has("testplug"));

    plug_register(&test_plugin);
    reset_case();
    plug_grant("testplug", "ping");
    plug_invoke("testplug.ping", "once", capture);
    CHECK(invoke_count == 1);
}

static void test_deny_by_default(void) {
    reset_case();
    plug_invoke("testplug.ping", "secret", capture);
    CHECK(invoke_count == 0);
    expect_response("{\"ok\":false,\"error\":\"permission denied\"}");

    plug_invoke("missing.nope", "secret", capture);
    CHECK(invoke_count == 0);
    expect_response("{\"ok\":false,\"error\":\"permission denied\"}");

    CHECK(plug_grant("missing", "nope"));
    plug_invoke("missing.nope", "secret", capture);
    expect_response("{\"ok\":false,\"error\":\"unknown plugin\"}");
    CHECK(invoke_count == 0);

    CHECK(plug_grant("testplug", "ping"));
    plug_invoke("testplug.ping", NULL, NULL);
    CHECK(invoke_count == 1);
    CHECK(payload_was_null == 0);
    CHECK(strcmp(seen_cmd, "ping") == 0);
    CHECK(strcmp(seen_payload, "") == 0);

    last_response = NULL;
    plug_invoke("testplug.ping", "hello", capture);
    CHECK(invoke_count == 2);
    CHECK(strcmp(seen_payload, "hello") == 0);
    expect_response("{\"ok\":true}");

    plug_invoke("testplug.other", "x", capture);
    CHECK(invoke_count == 2);
    expect_response("{\"ok\":false,\"error\":\"permission denied\"}");

    plug_revoke_all();
    plug_invoke("testplug.ping", "x", capture);
    CHECK(invoke_count == 2);
    expect_response("{\"ok\":false,\"error\":\"permission denied\"}");
}

static void test_command_shape(void) {
    reset_case();
    CHECK(plug_grant("testplug", "ping"));

    plug_invoke(NULL, "x", capture);
    CHECK(invoke_count == 0);
    expect_response("{\"ok\":false,\"error\":\"invalid command format\"}");

    const char *bad[] = {
        "",
        "testplug",
        ".ping",
        "testplug.",
        "testplug.ping.extra",
        "testplug.ping now",
        "test plug.ping",
        "testplug.pi/ng",
        "../testplug.ping",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        plug_invoke(bad[i], "x", capture);
        CHECK(invoke_count == 0);
        expect_response("{\"ok\":false,\"error\":\"invalid command format\"}");
    }

    char long_cmd[96];
    memset(long_cmd, 'a', 70);
    memcpy(long_cmd + 70, ".read", 6);
    plug_invoke(long_cmd, "x", capture);
    CHECK(invoke_count == 0);
    expect_response("{\"ok\":false,\"error\":\"invalid command format\"}");
}

static void test_grant_api(void) {
    reset_case();
    CHECK(!plug_grant(NULL, "read"));
    CHECK(!plug_grant("fs", NULL));
    CHECK(!plug_grant("", "read"));
    CHECK(!plug_grant("fs", ""));
    CHECK(!plug_grant("fs.read", "x"));
    CHECK(!plug_grant("fs", "read now"));
    CHECK(!plug_is_allowed("fs", "read"));

    CHECK(plug_grant("fs", "read"));
    CHECK(plug_grant("fs", "read"));
    CHECK(plug_is_allowed("fs", "read"));
    CHECK(!plug_is_allowed("fs", "write"));

    plug_revoke_all();
    for (int i = 0; i < 128; ++i) {
        char command[32];
        snprintf(command, sizeof(command), "c%d", i);
        CHECK(plug_grant("testplug", command));
    }
    CHECK(!plug_grant("testplug", "overflow"));
    CHECK(plug_is_allowed("testplug", "c0"));
    CHECK(plug_is_allowed("testplug", "c127"));
    CHECK(!plug_is_allowed("testplug", "overflow"));
}

static void test_grant_file(void) {
    reset_case();
    CHECK(plug_grant("testplug", "ping"));
    CHECK(!plug_load_grants("build/does-not-exist-grants.txt"));
    CHECK(plug_is_allowed("testplug", "ping"));

    CHECK(write_text("build/grants-ok.txt",
        "# comment\n"
        "\n"
        "testplug.pong\n"
        "fs.read\n"));
    CHECK(plug_load_grants("build/grants-ok.txt"));
    CHECK(plug_is_allowed("testplug", "ping"));
    CHECK(plug_is_allowed("testplug", "pong"));
    CHECK(plug_is_allowed("fs", "read"));
    CHECK(!plug_is_allowed("fs", "write"));

    CHECK(write_text("build/grants-bad.txt",
        "keystore.encrypt\n"
        "not a command\n"));
    CHECK(!plug_load_grants("build/grants-bad.txt"));
    CHECK(plug_is_allowed("testplug", "ping"));
    CHECK(plug_is_allowed("fs", "read"));
    CHECK(!plug_is_allowed("keystore", "encrypt"));

    plug_revoke_all();
    CHECK(plug_load_grants("crossweb.permissions"));
    CHECK(!plug_is_allowed("fs", "read"));
    CHECK(!plug_is_allowed("fs", "write"));
    CHECK(!plug_is_allowed("keystore", "encrypt"));
    CHECK(!plug_is_allowed("keystore", "decrypt"));
    CHECK(!plug_is_allowed("keystore", "save"));
    CHECK(!plug_is_allowed("keystore", "load"));
    CHECK(!plug_is_allowed("keystore", "get"));
    CHECK(!plug_is_allowed("testplug", "ping"));
}

static void test_identifiers(void) {
    CHECK(ipc_identifier_ok("abc", IPC_MAX_ID_LEN));
    CHECK(ipc_identifier_ok("fs.read", IPC_MAX_CMD_LEN));
    CHECK(ipc_identifier_ok("A1._:-", IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok(NULL, IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok("", IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok("bad\"id", IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok("a b", IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok("a;b", IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok("a/b", IPC_MAX_ID_LEN));
    CHECK(!ipc_identifier_ok("a\\b", IPC_MAX_ID_LEN));

    char boundary[IPC_MAX_ID_LEN + 4];
    memset(boundary, 'a', IPC_MAX_ID_LEN - 1);
    boundary[IPC_MAX_ID_LEN - 1] = '\0';
    CHECK(ipc_identifier_ok(boundary, IPC_MAX_ID_LEN));
    boundary[IPC_MAX_ID_LEN - 1] = 'b';
    boundary[IPC_MAX_ID_LEN] = '\0';
    CHECK(!ipc_identifier_ok(boundary, IPC_MAX_ID_LEN));

    ipc_response("bad\"id", "{\"ok\":true}");
    ipc_emit_event("click\"", "null");
    ipc_response(NULL, "{\"ok\":true}");
    ipc_emit_event("tick", NULL);
}

static void frame(char *dst, size_t cap, const char *id, const char *cmd, const char *b64) {
    snprintf(dst, cap, "%s%c%s%c%s", id, 30, cmd, 30, b64 == NULL ? "" : b64);
}

static void test_ipc_framing(void) {
    reset_case();
    IpcMessage msg;
    char framed[128];

    CHECK(!ipc_handle_js_message(NULL));
    CHECK(!ipc_handle_js_message("noseparator"));
    frame(framed, sizeof(framed), "onlyone", "", "");
    /* Drop the empty command by building a one-separator message. */
    snprintf(framed, sizeof(framed), "onlyone%cfield", 30);
    CHECK(!ipc_handle_js_message(framed));
    snprintf(framed, sizeof(framed), "%cfs.read%c", 30, 30);
    CHECK(!ipc_handle_js_message(framed));
    frame(framed, sizeof(framed), "bad\"id", "fs.read", "");
    CHECK(!ipc_handle_js_message(framed));
    frame(framed, sizeof(framed), "id", "fs read", "");
    CHECK(!ipc_handle_js_message(framed));
    frame(framed, sizeof(framed), "id", "fs.read", "****");
    CHECK(!ipc_handle_js_message(framed));
    CHECK(!ipc_receive(&msg));

    frame(framed, sizeof(framed), "id1", "testplug.ping", "");
    CHECK(ipc_handle_js_message(framed));
    CHECK(ipc_receive(&msg));
    CHECK(strcmp(msg.id, "id1") == 0);
    CHECK(strcmp(msg.cmd, "testplug.ping") == 0);
    CHECK(strcmp(msg.payload, "") == 0);

    frame(framed, sizeof(framed), "id2", "testplug.ping", "aGVsbG8=");
    CHECK(ipc_handle_js_message(framed));
    CHECK(ipc_receive(&msg));
    CHECK(strcmp(msg.payload, "hello") == 0);
    plug_invoke(msg.cmd, msg.payload, capture);
    CHECK(invoke_count == 0);
    expect_response("{\"ok\":false,\"error\":\"permission denied\"}");
    CHECK(plug_grant("testplug", "ping"));
    plug_invoke(msg.cmd, msg.payload, capture);
    CHECK(invoke_count == 1);
    CHECK(strcmp(seen_payload, "hello") == 0);

    reset_case();
    frame(framed, sizeof(framed), "id3", "testplug.ping", "YR5i");
    CHECK(ipc_handle_js_message(framed));
    CHECK(ipc_receive(&msg));
    {
        char expect_payload[] = {'a', 30, 'b', '\0'};
        CHECK(strcmp(msg.payload, expect_payload) == 0);
    }

    reset_case();
    frame(framed, sizeof(framed), "first", "fs.read", "aGk=");
    CHECK(ipc_handle_js_message(framed));
    frame(framed, sizeof(framed), "second", "fs.read", "cGluZw==");
    CHECK(ipc_handle_js_message(framed));
    CHECK(ipc_receive(&msg));
    CHECK(strcmp(msg.id, "first") == 0);
    CHECK(strcmp(msg.payload, "hi") == 0);
    CHECK(ipc_receive(&msg));
    CHECK(strcmp(msg.id, "second") == 0);
    CHECK(strcmp(msg.payload, "ping") == 0);
    CHECK(!ipc_receive(&msg));

    reset_case();
    for (int i = 0; i < 64; ++i) {
        char id[16];
        snprintf(id, sizeof(id), "m%d", i);
        frame(framed, sizeof(framed), id, "fs.read", "");
        CHECK(ipc_handle_js_message(framed));
    }
    frame(framed, sizeof(framed), "overflow", "fs.read", "");
    CHECK(!ipc_handle_js_message(framed));
    CHECK(ipc_receive(&msg));
    CHECK(strcmp(msg.id, "m0") == 0);
    for (int i = 0; i < 63; ++i) {
        CHECK(ipc_receive(&msg));
    }
    CHECK(!ipc_receive(&msg));
}

int main(void) {
    test_registry();
    test_deny_by_default();
    test_command_shape();
    test_grant_api();
    test_grant_file();
    test_identifiers();
    test_ipc_framing();

    if (failures != 0) {
        fprintf(stderr, "test_core: %d checks, %d failures\n", checks, failures);
        return 1;
    }
    printf("test_core: %d checks, 0 failures\n", checks);
    return 0;
}
