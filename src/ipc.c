
#include "ipc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef CROSSWEB_BUILDING_PLUG
#include "./thirdparty/webview-c/webview.h"
#endif
#endif

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

#ifdef _WIN32
#ifdef CROSSWEB_BUILDING_PLUG
static bool ipc_eval_js(const char *script) { (void)script; return false; }
#else
static bool ipc_eval_js(const char *script) {
    if (active_webview == NULL || script == NULL) {
        return false;
    }
    struct webview *wv = (struct webview *)active_webview;
    if (webview_eval(wv, script) != 0) {
        fprintf(stderr, "IPC: failed to evaluate bridge JS\n");
        return false;
    }
    return true;
}
#endif

static void ipc_inject_bridge(void) {
    static const char *bridge_js =
        "(function(){"
        "var SEP=String.fromCharCode(30);"
        "function encodePayload(value){"
        "  if(value===undefined||value===null){return '';}"
        "  var text=(typeof value==='string')?value:JSON.stringify(value);"
        "  if(window.TextEncoder){"
        "    var bytes=new TextEncoder().encode(text);"
        "    var bin='';"
        "    for(var i=0;i<bytes.length;i++){bin+=String.fromCharCode(bytes[i]);}"
        "    return btoa(bin);"
        "  }"
        "  return btoa(unescape(encodeURIComponent(text)));"
        "}"
        "function install(){"
        "  if(window.__native__&&window.__native__.__bridgeInstalled){return;}"
        "  window.__native__=window.__native__||{};"
        "  window.__native__.__bridgeInstalled=true;"
        "  window.__native__.invoke=function(cmd,payload){"
        "    var id=Math.random().toString(36).slice(2)+Date.now().toString(36);"
        "    var normalized=(payload===undefined||payload===null)?'':(typeof payload==='string'?payload:JSON.stringify(payload));"
        "    var encoded=encodePayload(normalized);"
        "    if(window.external&&typeof window.external.invoke==='function'){"
        "      window.external.invoke(id+SEP+String(cmd||'')+SEP+encoded);"
        "    }"
        "    return id;"
        "  };"
        "  window.__native__.listen=function(cb){window.__native__.onEvent=cb;};"
        "}"
        "if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',install);}"
        "else{install();}"
        "})();";
    ipc_eval_js(bridge_js);
}
#endif

void ipc_init(webview_t wv) {
    active_webview = wv;
    ipc_queue_clear();
#ifdef _WIN32
    if (wv != NULL) {
        ipc_inject_bridge();
    }
#else
    (void)wv;
#endif
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

    if (!decode_payload_field(second + 1, msg.payload, sizeof(msg.payload))) {
        fprintf(stderr, "IPC: failed to decode payload for %s\n", msg.cmd);
        ipc_send_response(msg.id, "{\"ok\":false,\"error\":\"invalid payload\"}");
        return false;
    }

    if (!ipc_queue_push(&msg)) {
        fprintf(stderr, "IPC: queue full, dropping %s\n", msg.id);
        ipc_send_response(msg.id, "{\"ok\":false,\"error\":\"ipc queue full\"}");
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

void ipc_send_response(const char *id, const char *response_json) {
    if (id == NULL || id[0] == '\0' || response_json == NULL) {
        return;
    }
#ifdef _WIN32
    size_t resp_len = strlen(response_json);
    size_t encoded_cap = ((resp_len + 2) / 3) * 4 + 4;
    char *encoded = (char *)malloc(encoded_cap);
    if (encoded == NULL) {
        return;
    }
    if (!base64_encode((const unsigned char *)response_json, resp_len, encoded, encoded_cap)) {
        free(encoded);
        return;
    }
    static const char *tmpl =
        "if(window.__native__&&window.__native__.onMessage){window.__native__.onMessage(\"%s\",JSON.parse(atob(\"%s\")));}";
    char *script = build_dispatch_script(tmpl, id, encoded);
    free(encoded);
    if (script != NULL) {
        ipc_eval_js(script);
        free(script);
    }
#else
    (void)id;
    (void)response_json;
#endif
}

void ipc_emit_event(const char *event, const char *data_json) {
    if (event == NULL || event[0] == '\0' || data_json == NULL) {
        return;
    }
#ifdef _WIN32
    size_t data_len = strlen(data_json);
    size_t encoded_cap = ((data_len + 2) / 3) * 4 + 4;
    char *encoded = (char *)malloc(encoded_cap);
    if (encoded == NULL) {
        return;
    }
    if (!base64_encode((const unsigned char *)data_json, data_len, encoded, encoded_cap)) {
        free(encoded);
        return;
    }
    int needed = snprintf(NULL, 0,
        "if(window.__native__&&window.__native__.onEvent){window.__native__.onEvent(\"%s\",JSON.parse(atob(\"%s\")));}",
        event, encoded);
    if (needed <= 0) {
        free(encoded);
        return;
    }
    char *script = (char *)malloc((size_t)needed + 1);
    if (script == NULL) {
        free(encoded);
        return;
    }
    snprintf(script, (size_t)needed + 1,
        "if(window.__native__&&window.__native__.onEvent){window.__native__.onEvent(\"%s\",JSON.parse(atob(\"%s\")));}",
        event, encoded);
    free(encoded);
    ipc_eval_js(script);
    free(script);
#else
    (void)event;
    (void)data_json;
#endif
}

void ipc_deinit(void) {
    ipc_queue_clear();
    active_webview = NULL;
}