#ifndef IPC_H_
#define IPC_H_

#include <stdbool.h>
#include "plug.h"

typedef struct {
    char cmd[256];
    char payload[1024];
} IpcMessage;

void ipc_init(webview_t wv);
bool ipc_receive(IpcMessage *msg);
void ipc_send(webview_t wv, const char *response);
void ipc_deinit(void);

#endif // IPC_H_