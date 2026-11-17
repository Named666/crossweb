#ifndef IPC_H_
#define IPC_H_

#include <stdbool.h>
#include "plug.h"

#define IPC_MAX_ID_LEN 64
#define IPC_MAX_CMD_LEN 256
#define IPC_MAX_PAYLOAD_LEN 4096

typedef struct {
    char id[IPC_MAX_ID_LEN];
    char cmd[IPC_MAX_CMD_LEN];
    char payload[IPC_MAX_PAYLOAD_LEN];
    RespondCallback respond;
} IpcMessage;

// ============================================================================
// IPC Module: Centralized Validation & Security Boundary
// ============================================================================
// 
// This module implements the core runtime boundary that separates untrusted
// input (WebView/JNI) from privileged operations (OS access, plugins).
//
// **Public Entry Points:**
//   ipc_init()                 - Initialize IPC system
//   ipc_handle_js_message()    - [PRIMARY] Validate & queue message from JS/JNI
//   ipc_dispatch()             - Queue a command programmatically
//   ipc_process_queue()        - Process all queued messages (in-thread)
//   ipc_response()             - Send response back to caller
//   ipc_emit_event()           - Emit async event to WebView/JNI
//
// **Security Contract:**
//   1. ALL messages from WebView or JNI must enter via ipc_handle_js_message()
//   2. Messages are validated (JSON, lengths, format) before queuing
//   3. Only validated, escaped messages are passed to plugins
//   4. Plugins only receive messages from this centralized queue
//   5. Responses route back through ipc_response() for formatting
//
// This ensures a single, enforced security boundary.
// ============================================================================

void ipc_init(webview_t wv);
bool ipc_dispatch(const char *cmd, const char *payload, RespondCallback respond);
void ipc_process_queue(void);
bool ipc_route_external_message(const char *source, const char *message, RespondCallback respond);
// Register platform callbacks that set/clear the per-invoke context (e.g. pending id)
void ipc_register_platform_response_fns(void (*set_id)(const char *id), void (*clear_id)(void));
bool ipc_handle_js_message(const char *message, RespondCallback respond);
void ipc_inject_bridge(void);
void ipc_log_response(const char *id, const char *response_json);
void ipc_response(const char *id, const char *response_json);
void ipc_emit_event(const char *event, const char *data_json);
void ipc_deinit(void);

#endif // IPC_H_