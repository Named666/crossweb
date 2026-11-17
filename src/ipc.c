// ============================================================================
// ipc.c - Centralized IPC Validation & Message Broker
// ============================================================================
// This module establishes the core runtime boundary by:
//   1. Centralizing all incoming message validation (JSON parsing, length checks)
//   2. Queueing validated messages in a thread-safe queue
//   3. Processing queued messages in a central dispatch loop
//   4. Routing to plugins through plug_invoke() with consistent error handling
//
// **Security Boundary:** All privileged operations must enter through this module.
// Messages from WebView (Windows) or JNI (Android) MUST be validated here before
// reaching plugin code. No direct plugin calls should bypass this queue.
//
// Flow:
//   WebView/JNI → ipc_handle_js_message() → validation → ipc_queue_push()
//   → [event loop / background thread] → ipc_process_queue() → plug_invoke()
//   → [plugin handles] → RespondCallback → ipc_response() → WebView/JNI
//
// ============================================================================

#include "ipc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef _WIN32
#ifndef CROSSWEB_BUILDING_PLUG
#include "./thirdparty/webview-c/webview.h"
#endif
#endif

#ifdef __ANDROID__
extern void android_response(const char *id, const char *response_json);
#endif

#define IPC_QUEUE_CAP 64

static IpcMessage queue[IPC_QUEUE_CAP];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;
static webview_t active_webview = NULL;
// platform hooks to set/clear a per-invoke id context (used by Windows webview)
static void (*platform_set_pending_id)(const char *id) = NULL;
static void (*platform_clear_pending_id)(void) = NULL;
// Lightweight, platform-neutral spin lock for the IPC queue.
// This keeps the core IPC implementation free from platform-specific headers.
static int ipc_lock_flag = 0;

static void ipc_lock_init(void) {
    ipc_lock_flag = 0;
}

static void ipc_lock_acquire(void) {
    while (__sync_lock_test_and_set(&ipc_lock_flag, 1)) {
        /* spin */
    }
}

static void ipc_lock_release(void) {
    __sync_lock_release(&ipc_lock_flag);
}
static void ipc_queue_clear(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}

static bool ipc_queue_push(const IpcMessage *msg) {
    bool ok = false;
    ipc_lock_acquire();
    if (queue_count < IPC_QUEUE_CAP) {
        queue[queue_tail] = *msg;
        queue_tail = (queue_tail + 1) % IPC_QUEUE_CAP;
        queue_count++;
        ok = true;
    }
    ipc_lock_release();
    return ok;
}

void ipc_process_queue(void) {
    IpcMessage msg;
    while (1) {
        ipc_lock_acquire();
        if (queue_count == 0) {
            ipc_lock_release();
            break;
        }
        msg = queue[queue_head];
        queue_head = (queue_head + 1) % IPC_QUEUE_CAP;
        queue_count--;
        ipc_lock_release();

        if (platform_set_pending_id) platform_set_pending_id(msg.id);
        plug_invoke(msg.cmd, msg.payload, msg.respond);
        if (platform_clear_pending_id) platform_clear_pending_id();
    }
}

// Simple JSON string extraction - finds value for a key in a flat JSON object
// Returns pointer to start of value (after quotes for strings), sets *len to value length
// For strings, returns content without quotes. For other types, returns raw value.
static const char *json_get_string(const char *json, const char *key, size_t *len) {
    if (!json || !key || !len) return NULL;
    
    size_t key_len = strlen(key);
    const char *p = json;
    
    while ((p = strstr(p, "\"")) != NULL) {
        p++; // skip opening quote
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '"') {
            // Found key, skip to colon
            p += key_len + 1;
            while (*p && (*p == ' ' || *p == ':')) p++;
            
            if (*p == '"') {
                // String value
                p++; // skip opening quote
                const char *start = p;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p+1)) p++; // skip escaped char
                    p++;
                }
                *len = (size_t)(p - start);
                return start;
            } else if (*p == 'n' && strncmp(p, "null", 4) == 0) {
                // null value
                *len = 0;
                return p;
            } else {
                // Other value (number, object, array) - find end
                const char *start = p;
                int depth = 0;
                while (*p) {
                    if (*p == '{' || *p == '[') depth++;
                    else if (*p == '}' || *p == ']') {
                        if (depth == 0) break;
                        depth--;
                    }
                    else if (depth == 0 && (*p == ',' || *p == '}')) break;
                    p++;
                }
                *len = (size_t)(p - start);
                return start;
            }
        }
    }
    return NULL;
}

// Unescape JSON string in-place, returns new length
static size_t json_unescape(char *str, size_t len) {
    size_t i = 0, j = 0;
    while (i < len) {
        if (str[i] == '\\' && i + 1 < len) {
            i++;
            switch (str[i]) {
                case 'n': str[j++] = '\n'; break;
                case 'r': str[j++] = '\r'; break;
                case 't': str[j++] = '\t'; break;
                case '"': str[j++] = '"'; break;
                case '\\': str[j++] = '\\'; break;
                case '/': str[j++] = '/'; break;
                default: str[j++] = str[i]; break;
            }
        } else {
            str[j++] = str[i];
        }
        i++;
    }
    return j;
}

#ifdef _WIN32
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

void ipc_inject_bridge(void) {
    // Minimal bridge - just ensure onMessage handler exists
    static const char *bridge_js =
        "(function(){"
        "if(!window.external)window.external={};"
        "if(!window.external.onMessage)window.external.onMessage=function(){};"
        "if(!window.external.onEvent)window.external.onEvent=function(){};"
        "})();";
    ipc_eval_js(bridge_js);
}
#endif

void ipc_init(webview_t wv) {
    active_webview = wv;
    ipc_queue_clear();
    ipc_lock_init();
#ifdef _WIN32
    if (wv != NULL) {
        ipc_inject_bridge();
    }
#else
    (void)wv;
#endif
}

bool ipc_handle_js_message(const char *message, RespondCallback respond) {
    if (message == NULL) {
        return false;
    }

    // Parse JSON: {id, cmd, payload}
    size_t id_len = 0, cmd_len = 0, payload_len = 0;
    const char *id_ptr = json_get_string(message, "id", &id_len);
    const char *cmd_ptr = json_get_string(message, "cmd", &cmd_len);
    const char *payload_ptr = json_get_string(message, "payload", &payload_len);

    if (!id_ptr || id_len == 0 || id_len >= IPC_MAX_ID_LEN) {
        fprintf(stderr, "IPC: invalid or missing id\n");
        return false;
    }
    if (!cmd_ptr || cmd_len == 0 || cmd_len >= IPC_MAX_CMD_LEN) {
        fprintf(stderr, "IPC: invalid or missing cmd\n");
        return false;
    }

    IpcMessage msg = {0};
    
    // Copy and unescape id
    memcpy(msg.id, id_ptr, id_len);
    msg.id[id_len] = '\0';
    size_t new_len = json_unescape(msg.id, id_len);
    msg.id[new_len] = '\0';

    // Copy and unescape cmd
    memcpy(msg.cmd, cmd_ptr, cmd_len);
    msg.cmd[cmd_len] = '\0';
    new_len = json_unescape(msg.cmd, cmd_len);
    msg.cmd[new_len] = '\0';

    // Handle payload - can be string, object, or null
    if (payload_ptr && payload_len > 0) {
        // Check if it's a string (payload was quoted)
        if (payload_len < IPC_MAX_PAYLOAD_LEN) {
            // For string values, json_get_string returns content without quotes
            // For objects/arrays, it returns the raw JSON
            memcpy(msg.payload, payload_ptr, payload_len);
            msg.payload[payload_len] = '\0';
            // Only unescape if it was a string (not starting with { or [)
            if (payload_ptr[0] != '{' && payload_ptr[0] != '[' && 
                strncmp(payload_ptr, "null", 4) != 0) {
                new_len = json_unescape(msg.payload, payload_len);
                msg.payload[new_len] = '\0';
            }
        }
    }

    msg.respond = respond;
    if (!ipc_queue_push(&msg)) {
        fprintf(stderr, "IPC: queue full, dropping %s\n", msg.id);
        if (respond) {
            respond("{\"ok\":false,\"error\":\"ipc queue full\"}");
        }
        return false;
    }
    return true;
}

bool ipc_route_external_message(const char *source, const char *message, RespondCallback respond) {
    if (!ipc_handle_js_message(message, respond)) {
        if (source && source[0] != '\0') {
            fprintf(stderr, "IPC: dropped malformed message from %s\n", source);
        } else {
            fprintf(stderr, "IPC: dropped malformed message\n");
        }
        return false;
    }
    return true;
}

bool ipc_dispatch(const char *cmd, const char *payload, RespondCallback respond) {
    if (cmd == NULL || cmd[0] == '\0') {
        if (respond) respond("{\"error\":\"missing command\"}");
        return false;
    }
    fprintf(stderr, "IPC: dispatch cmd=%s\n", cmd);
    IpcMessage msg = {0};
    strncpy(msg.cmd, cmd, IPC_MAX_CMD_LEN - 1);
    if (payload) {
        strncpy(msg.payload, payload, IPC_MAX_PAYLOAD_LEN - 1);
    }
    msg.respond = respond;
    return ipc_queue_push(&msg);
}

// Escape a string for JSON output
static char *json_escape_string(const char *str, size_t *out_len) {
    if (!str) {
        *out_len = 0;
        return NULL;
    }
    
    size_t len = strlen(str);
    // Worst case: every char needs escaping
    char *escaped = (char *)malloc(len * 2 + 1);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"':  escaped[j++] = '\\'; escaped[j++] = '"'; break;
            case '\\': escaped[j++] = '\\'; escaped[j++] = '\\'; break;
            case '\n': escaped[j++] = '\\'; escaped[j++] = 'n'; break;
            case '\r': escaped[j++] = '\\'; escaped[j++] = 'r'; break;
            case '\t': escaped[j++] = '\\'; escaped[j++] = 't'; break;
            default:   escaped[j++] = str[i]; break;
        }
    }
    escaped[j] = '\0';
    *out_len = j;
    return escaped;
}

static bool ipc_emit_bridge_message(const char *handler, const char *name, const char *payload) {
    if (handler == NULL || name == NULL || name[0] == '\0' || payload == NULL) {
        return false;
    }
#ifdef __ANDROID__
    if (strcmp(handler, "onMessage") == 0) {
        android_response(name, payload);
        return true;
    }
    return false;
#elif defined(_WIN32)
    size_t name_esc_len = 0;
    char *name_escaped = json_escape_string(name, &name_esc_len);
    if (!name_escaped) return false;

    size_t payload_esc_len = 0;
    char *payload_escaped = json_escape_string(payload, &payload_esc_len);
    if (!payload_escaped) {
        free(name_escaped);
        return false;
    }

    const char *tmpl = "if(window.external&&window.external.%s){window.external.%s(\"%s\",JSON.parse(\"%s\"));}";
    int needed = snprintf(NULL, 0, tmpl, handler, handler, name_escaped, payload_escaped);
    if (needed > 0) {
        char *script = (char *)malloc((size_t)needed + 1);
        if (script) {
            snprintf(script, (size_t)needed + 1, tmpl, handler, handler, name_escaped, payload_escaped);
            ipc_eval_js(script);
            free(script);
        }
    }

    free(name_escaped);
    free(payload_escaped);
    return true;
#else
    (void)handler;
    (void)name;
    (void)payload;
    return false;
#endif
}

void ipc_response(const char *id, const char *response_json) {
    if (id == NULL || id[0] == '\0' || response_json == NULL) {
        return;
    }
    ipc_log_response(id, response_json);
    ipc_emit_bridge_message("onMessage", id, response_json);
}

void ipc_log_response(const char *id, const char *response_json) {
    if (response_json == NULL || strstr(response_json, "\"error\"") == NULL) {
        return;
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "Crossweb", "IPC/plugin error (id=%s): %s", id ? id : "", response_json);
#else
    fprintf(stderr, "IPC/plugin error (id=%s): %s\n", id ? id : "", response_json);
    fflush(stderr);
#endif
}

void ipc_emit_event(const char *event, const char *data_json) {
    if (event == NULL || event[0] == '\0' || data_json == NULL) {
        return;
    }
    ipc_emit_bridge_message("onEvent", event, data_json);
}

void ipc_deinit(void) {
    ipc_queue_clear();
    active_webview = NULL;
}

void ipc_register_platform_response_fns(void (*set_id)(const char *id), void (*clear_id)(void)) {
    platform_set_pending_id = set_id;
    platform_clear_pending_id = clear_id;
}