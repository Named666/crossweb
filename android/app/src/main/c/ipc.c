#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Simple message queue (replace with better impl)
static IpcMessage queue[100];
static int queue_size = 0;

void ipc_init(webview_t wv) {
    if (wv != NULL) {
        // Inject JS bridge
        const char *js = "window.__native__ = {"
                         "invoke: function(cmd, payload) {"
                         "  var id = Math.random().toString(36).substr(2, 9);"
                         "  window.external.postMessage(JSON.stringify({id: id, cmd: cmd, payload: payload}));"
                         "  return id;"
                         "},"
                         "listen: function(callback) {"
                         "  window.__native__.onMessage = callback;"
                         "}"
                         "};";
        // Assuming webview.h has webview_inject_js
        // webview_inject_js(wv, js);
    }
    // Set up listener for webview messages
    // For now, stub
}

bool ipc_receive(IpcMessage *msg) {
    if (queue_size > 0) {
        *msg = queue[--queue_size];
        return true;
    }
    return false;
}

void ipc_send(webview_t wv, const char *response) {
    char js[2048];
    snprintf(js, sizeof(js), "window.__native__.onMessage(%s)", response);
    // webview_inject_js(wv, js); // Assuming webview.h has this
}

void ipc_deinit(void) {
    queue_size = 0;
}