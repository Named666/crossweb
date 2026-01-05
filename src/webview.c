#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


#define WEBVIEW_IMPLEMENTATION
#include "./thirdparty/webview-c/webview.h"
#endif

#include "hotreload.h"
#include "plug.h"
#include "ipc.h"

#ifdef _WIN32

static char g_start_url[MAX_PATH * 4];

static char pending_invoke_id[IPC_MAX_ID_LEN] = {0};

static void respond_to_js(const char *response) {
    if (pending_invoke_id[0] == '\0') {
        return;
    }
    const char *payload = response ? response : "{\"ok\":true}";
    ipc_response(pending_invoke_id, payload);
}

static void host_emit_event(const char *event, const char *data_json) {
    if (event == NULL || event[0] == '\0') {
        return;
    }
    ipc_emit_event(event, data_json ? data_json : "null");
}

static void process_ipc_queue(webview_t wv) {
    IpcMessage msg;
    while (ipc_receive(&msg)) {
        char previous_id[IPC_MAX_ID_LEN];
        memcpy(previous_id, pending_invoke_id, sizeof(previous_id));
        strncpy(pending_invoke_id, msg.id, sizeof(pending_invoke_id) - 1);
        pending_invoke_id[sizeof(pending_invoke_id) - 1] = '\0';
        plug_invoke(msg.cmd, msg.payload, respond_to_js);
        memcpy(pending_invoke_id, previous_id, sizeof(pending_invoke_id));
    }
    (void)wv;
}

static bool file_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void ensure_webview2_loader_path(void) {
    // webview-c loads WebView2 via LoadLibraryA("WebView2Loader.dll") and allows
    // overriding the location via WEBVIEW2_WIN32_PATH.
    if (getenv("WEBVIEW2_WIN32_PATH") != NULL) {
        return;
    }

    char exe_path[MAX_PATH * 4] = {0};
    DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof(exe_path));
    if (n == 0 || n >= sizeof(exe_path)) {
        return;
    }

    // Strip filename to get exe directory.
    for (char *p = exe_path + n; p != exe_path; --p) {
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            break;
        }
    }

    char candidate[MAX_PATH * 4];
    const char *dll_name = "WebView2Loader.dll";

    // 1) Exe directory
    snprintf(candidate, sizeof(candidate), "%s\\%s", exe_path, dll_name);
    if (file_exists(candidate)) {
        SetEnvironmentVariableA("WEBVIEW2_WIN32_PATH", exe_path);
        return;
    }

    // 2) Repo layout fallbacks for dev runs:
    //    <exeDir>\..\webview-c\ms.webview2\x64\WebView2Loader.dll
    //    <exeDir>\..\..\webview-c\ms.webview2\x64\WebView2Loader.dll
    static const char *rel_folders[] = {
        ".\\thirdparty\\webview-c\\ms.webview2\\x64",
        "..\\..\\thirdparty\\webview-c\\ms.webview2\\x64",
        "..\\..\\..\\thirdparty\\webview-c\\ms.webview2\\x64",
    };

    for (size_t i = 0; i < sizeof(rel_folders) / sizeof(rel_folders[0]); ++i) {
        char folder[MAX_PATH * 4];
        snprintf(folder, sizeof(folder), "%s\\%s", exe_path, rel_folders[i]);
        snprintf(candidate, sizeof(candidate), "%s\\%s", folder, dll_name);
        if (file_exists(candidate)) {
            SetEnvironmentVariableA("WEBVIEW2_WIN32_PATH", folder);
            return;
        }
    }
}

static bool resolve_start_url(void) {
    // Use Vite development server
    strcpy(g_start_url, "http://localhost:5173");
    return true;
}

static void handle_external_invoke(struct webview *wv, const char *arg) {
    (void)wv;
    if (!ipc_handle_js_message(arg)) {
        fprintf(stderr, "IPC: dropped malformed message\n");
    }
}

static void on_webview_ready(struct webview *wv) {
    (void)wv;
    ipc_inject_bridge();
}

static void configure_webview(struct webview *wv) {
    memset(wv, 0, sizeof(*wv));
    wv->title = "Crossweb";
    wv->url = g_start_url;
    wv->width = 1280;
    wv->height = 800;
    wv->resizable = 1;
    wv->debug = 1;
    wv->external_invoke_cb = handle_external_invoke;
    wv->ready_cb = on_webview_ready;
}

int main(void) {
    if (!resolve_start_url()) {
        return 1;
    }

    ensure_webview2_loader_path();

    if (!reload_libplug()) {
        return 1;
    }

#ifdef CROSSWEB_HOTRELOAD
    if (plug_set_host_emit_event != NULL) {
        plug_set_host_emit_event(host_emit_event);
    }
#endif

    struct webview wv;
    configure_webview(&wv);
    if (webview_init(&wv) != 0) {
        fprintf(stderr, "Failed to initialize WebView2 host\n");
        return 1;
    }

    ipc_init((webview_t)&wv);
    plug_init((webview_t)&wv);

    bool running = true;
    while (running && webview_loop(&wv, 1) == 0) {
#ifdef CROSSWEB_HOTRELOAD
        if (reload_libplug_changed()) {
            void *state = plug_pre_reload();
            if (reload_libplug()) {
                printf("HOTRELOAD: successfully reloaded plugin\n");
                if (plug_set_host_emit_event != NULL) {
                    plug_set_host_emit_event(host_emit_event);
                }
                plug_init((webview_t)&wv);
                plug_post_reload(state);
            } else {
                printf("HOTRELOAD: reload failed, keeping old version\n");
            }
        }
#endif
        process_ipc_queue((webview_t)&wv);
        plug_update((webview_t)&wv);
    }
    plug_cleanup((webview_t)&wv);
    ipc_deinit();
    webview_exit(&wv);
    return 0;
}

#else // !_WIN32

int main(void)
{
    struct sigaction act = {0};
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    if (!reload_libplug()) return 1;
    plug_init((webview_t)NULL);

    for (;;) {
#ifdef CROSSWEB_HOTRELOAD
        if (reload_libplug_changed()) {
            void *state = plug_pre_reload();
            if (!reload_libplug()) return 1;
            plug_post_reload(state);
        }
#endif
        plug_update((webview_t)NULL);
        usleep(1000);
    }
    plug_cleanup((webview_t)NULL);
    return 0;
}
#endif