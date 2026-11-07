#ifndef IPC_H_
#define IPC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "plug.h"

#define IPC_MAX_ID_LEN 64
#define IPC_MAX_CMD_LEN 256
#define IPC_MAX_PAYLOAD_LEN 4096

typedef struct {
    char id[IPC_MAX_ID_LEN];
    char cmd[IPC_MAX_CMD_LEN];
    char payload[IPC_MAX_PAYLOAD_LEN];
} IpcMessage;

void ipc_init(webview_t wv);
bool ipc_receive(IpcMessage *msg);
/* True when value is a non-empty token of length < max_len using [A-Za-z0-9._:-]. */
bool ipc_identifier_ok(const char *value, size_t max_len);
bool ipc_handle_js_message(const char *message);
/* Host delivers JavaScript. The callback must copy script if it runs it later. */
typedef bool (*IpcEvalFn)(const char *script, void *userdata);
void ipc_set_eval(IpcEvalFn fn, void *userdata);
void ipc_inject_bridge(void);
void ipc_response(const char *id, const char *response_json);
void ipc_emit_event(const char *event, const char *data_json);
void ipc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // IPC_H_