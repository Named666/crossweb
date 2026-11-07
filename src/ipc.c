
#include "ipc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
extern void android_response(const char *id, const char *response_json);
#endif

static IpcEvalFn ipc_eval_fn = NULL;
static void *ipc_eval_userdata = NULL;

#define IPC_QUEUE_CAP 64
#define IPC_SEPARATOR '\x1e'

static IpcMessage queue[IPC_QUEUE_CAP];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;
static webview_t active_webview = NULL;

static void ipc_queue_clear(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}

static bool ipc_queue_push(const IpcMessage *msg) {
    if (queue_count >= IPC_QUEUE_CAP) {
        return false;
    }
    queue[queue_tail] = *msg;
    queue_tail = (queue_tail + 1) % IPC_QUEUE_CAP;
    queue_count++;
    return true;
}

bool ipc_identifier_ok(const char *value, size_t max_len) {
    if (value == NULL || value[0] == '\0' || max_len < 2) return false;
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        unsigned char c = *p;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' || c == '-';
        if (!ok) return false;
        n++;
        if (n >= max_len) return false;
    }
    return true;
}

static bool base64_encode(const unsigned char *input, size_t len, char *output, size_t output_cap) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t needed = ((len + 2) / 3) * 4;
    if (output_cap < needed + 1) {
        return false;
    }
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int octet_a = input[i];
        unsigned int octet_b = (i + 1) < len ? input[i + 1] : 0;
        unsigned int octet_c = (i + 2) < len ? input[i + 2] : 0;

        unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = table[(triple >> 18) & 0x3F];
        output[j++] = table[(triple >> 12) & 0x3F];
        output[j++] = (i + 1) < len ? table[(triple >> 6) & 0x3F] : '=';
        output[j++] = (i + 2) < len ? table[triple & 0x3F] : '=';
    }
    output[j] = '\0';
    return true;
}

static int base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2; // padding
    return -1;
}

static bool base64_decode(const char *input, unsigned char *output, size_t output_cap, size_t *written) {
    int val = 0;
    int valb = -8;
    size_t out_len = 0;
    for (const char *p = input; *p; ++p) {
        int v = base64_value(*p);
        if (v == -1) {
            return false;
        }
        if (v == -2) {
            break;
        }
        val = (val << 6) | v;
        valb += 6;
        if (valb >= 0) {
            if (out_len >= output_cap) {
                return false;
            }
            output[out_len++] = (unsigned char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    if (written) {
        *written = out_len;
    }
    if (out_len < output_cap) {
        output[out_len] = '\0';
    }
    return true;
}

void ipc_set_eval(IpcEvalFn fn, void *userdata) {
    ipc_eval_fn = fn;
    ipc_eval_userdata = userdata;
}

void ipc_inject_bridge(void) {
    /* Hosts install their own bridge. The old webview-c injector depended on a
       missing third-party header and is not used by the system WebView shells. */
}

void ipc_init(webview_t wv) {
    active_webview = wv;
    ipc_queue_clear();
}

bool ipc_receive(IpcMessage *msg) {
    if (queue_count == 0 || msg == NULL) {
        return false;
    }
    *msg = queue[queue_head];
    queue_head = (queue_head + 1) % IPC_QUEUE_CAP;
    queue_count--;
    return true;
}

static bool decode_payload_field(const char *encoded, char *out, size_t out_cap) {
    if (encoded == NULL) {
        if (out_cap > 0) out[0] = '\0';
        return true;
    }
    size_t max_bytes = out_cap ? out_cap - 1 : 0;
    unsigned char *buffer = (unsigned char *)out;
    size_t written = 0;
    if (!base64_decode(encoded, buffer, max_bytes, &written)) {
        return false;
    }
    if (written < out_cap) {
        out[written] = '\0';
    }
    return true;
}

bool ipc_handle_js_message(const char *message) {
    if (message == NULL) {
        return false;
    }
    const char *first = strchr(message, IPC_SEPARATOR);
    if (!first) {
        return false;
    }
    const char *second = strchr(first + 1, IPC_SEPARATOR);
    if (!second) {
        return false;
    }

    size_t id_len = (size_t)(first - message);
    size_t cmd_len = (size_t)(second - (first + 1));
    if (id_len == 0 || id_len >= IPC_MAX_ID_LEN) {
        return false;
    }
    if (cmd_len == 0 || cmd_len >= IPC_MAX_CMD_LEN) {
        return false;
    }

    IpcMessage msg = {0};
    memcpy(msg.id, message, id_len);
    msg.id[id_len] = '\0';
    memcpy(msg.cmd, first + 1, cmd_len);
    msg.cmd[cmd_len] = '\0';

    if (!ipc_identifier_ok(msg.id, IPC_MAX_ID_LEN) || !ipc_identifier_ok(msg.cmd, IPC_MAX_CMD_LEN)) {
        return false;
    }

    if (!decode_payload_field(second + 1, msg.payload, sizeof(msg.payload))) {
        fprintf(stderr, "IPC: failed to decode payload for %s\n", msg.cmd);
        ipc_response(msg.id, "{\"ok\":false,\"error\":\"invalid payload\"}");
        return false;
    }

    if (!ipc_queue_push(&msg)) {
        fprintf(stderr, "IPC: queue full, dropping %s\n", msg.id);
        ipc_response(msg.id, "{\"ok\":false,\"error\":\"ipc queue full\"}");
        return false;
    }
    return true;
}

static char *build_dispatch_script(const char *format, const char *id, const char *encoded_payload) {
    if (id == NULL || encoded_payload == NULL) {
        return NULL;
    }
    int needed = snprintf(NULL, 0, format, id, encoded_payload);
    if (needed <= 0) {
        return NULL;
    }
    char *buffer = (char *)malloc((size_t)needed + 1);
    if (buffer == NULL) {
        return NULL;
    }
    snprintf(buffer, (size_t)needed + 1, format, id, encoded_payload);
    return buffer;
}

static void ipc_eval_encoded(const char *token, const char *json, const char *tmpl) {
    if (ipc_eval_fn == NULL || json == NULL) {
        return;
    }
    size_t len = strlen(json);
    size_t encoded_cap = ((len + 2) / 3) * 4 + 4;
    char *encoded = (char *)malloc(encoded_cap);
    if (encoded == NULL) {
        return;
    }
    if (!base64_encode((const unsigned char *)json, len, encoded, encoded_cap)) {
        free(encoded);
        return;
    }
    char *script = build_dispatch_script(tmpl, token, encoded);
    free(encoded);
    if (script != NULL) {
        ipc_eval_fn(script, ipc_eval_userdata);
        free(script);
    }
}

void ipc_response(const char *id, const char *response_json) {
    if (!ipc_identifier_ok(id, IPC_MAX_ID_LEN) || response_json == NULL) {
        return;
    }
#ifdef __ANDROID__
    android_response(id, response_json);
#else
    ipc_eval_encoded(id, response_json,
        "if(window.external&&window.external.onMessage){window.external.onMessage(\"%s\",JSON.parse(atob(\"%s\")));}");
#endif
}

void ipc_emit_event(const char *event, const char *data_json) {
    if (!ipc_identifier_ok(event, IPC_MAX_CMD_LEN) || data_json == NULL) {
        return;
    }
    ipc_eval_encoded(event, data_json,
        "if(window.external&&window.external.onEvent){window.external.onEvent(\"%s\",JSON.parse(atob(\"%s\")));}");
}

void ipc_deinit(void) {
    ipc_queue_clear();
    active_webview = NULL;
}