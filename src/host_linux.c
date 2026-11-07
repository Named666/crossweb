#include "safe_open.h"
#include "hotreload.h"
#include "ipc.h"
#include "origin.h"
#include "assets_map.h"
#include "host_config.h"

#include <cairo.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !CROSSWEB_HOTRELOAD
#include "plugins/fs/path.h"
#include "plugins/app/plugin.h"
#endif

static WebKitWebView *g_view = NULL;
static char g_assets[SAFE_OPEN_PATH_MAX];
static char g_dev_origin[512];
static int g_smoke = 0;
static int g_smoke_status = 2;
static guint g_smoke_timeout = 0;

static const char *bridge_js =
    "(function(){"
    "if(window.external&&window.external.__bridgeInstalled)return;"
    "window.external=window.external||{};"
    "window.external.__bridgeInstalled=true;"
    "var SEP=String.fromCharCode(30);"
    "function encodePayload(value){"
    "if(value===undefined||value===null)return '';"
    "var text=(typeof value==='string')?value:JSON.stringify(value);"
    "var bytes=new TextEncoder().encode(text);"
    "var bin='';"
    "for(var i=0;i<bytes.length;i++)bin+=String.fromCharCode(bytes[i]);"
    "return btoa(bin);"
    "}"
    "window.external.invoke=function(cmd,payload){"
    "var id=Math.random().toString(36).slice(2)+Date.now().toString(36);"
    "var normalized=(payload===undefined||payload===null)?'':(typeof payload==='string'?payload:JSON.stringify(payload));"
    "var encoded=encodePayload(normalized);"
    "window.webkit.messageHandlers.crossweb.postMessage(id+SEP+String(cmd||'')+SEP+encoded);"
    "return id;"
    "};"
    "})();";

static char g_pending_id[IPC_MAX_ID_LEN];

static void respond_to_js(const char *response) {
    ipc_response(g_pending_id, response ? response : "{\"ok\":false}");
}

static void drain_ipc(void) {
    IpcMessage msg;
    while (ipc_receive(&msg)) {
        snprintf(g_pending_id, sizeof(g_pending_id), "%s", msg.id);
        plug_invoke(msg.cmd, msg.payload, respond_to_js);
    }
}

static gboolean eval_idle(gpointer data) {
    char *script = data;
    if (g_view != NULL && script != NULL) {
        webkit_web_view_evaluate_javascript(g_view, script, -1, NULL, NULL, NULL, NULL, NULL);
    }
    g_free(script);
    return G_SOURCE_REMOVE;
}

static bool linux_eval(const char *script, void *userdata) {
    (void)userdata;
    if (script == NULL) return false;
    g_idle_add(eval_idle, g_strdup(script));
    return true;
}

static void on_scheme(WebKitURISchemeRequest *request, gpointer user_data) {
    const char *path = webkit_uri_scheme_request_get_path(request);
    unsigned char *data = NULL;
    size_t len = 0;
    const char *mime = NULL;
    const char *err = NULL;
    GBytes *bytes;
    GInputStream *stream;
    (void)user_data;
    if (!assets_read(g_assets, path, &data, &len, &mime, &err)) {
        GError *error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_NOENT, "%s", err ? err : "not found");
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
        return;
    }
    bytes = g_bytes_new_take(data, len);
    stream = g_memory_input_stream_new_from_bytes(bytes);
    webkit_uri_scheme_request_finish(request, stream, (gint64)len, mime);
    g_object_unref(stream);
    g_bytes_unref(bytes);
}

static gboolean on_decide_policy(WebKitWebView *view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer user_data) {
    const char *uri = NULL;
    (void)view;
    (void)user_data;
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION ||
        type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
    } else if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
        uri = webkit_uri_request_get_uri(webkit_response_policy_decision_get_request(WEBKIT_RESPONSE_POLICY_DECISION(decision)));
    }
    if (uri != NULL && !origin_is_trusted(uri, CROSSWEB_APP_ORIGIN, g_dev_origin)) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }
    return FALSE;
}

static void finish_smoke(int status) {
    g_smoke_status = status;
    if (g_smoke_timeout != 0) {
        g_source_remove(g_smoke_timeout);
        g_smoke_timeout = 0;
    }
    gtk_main_quit();
}

static void on_snapshot(GObject *source, GAsyncResult *result, gpointer user_data) {
    GError *error = NULL;
    cairo_surface_t *surface = webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(source), result, &error);
    int status = GPOINTER_TO_INT(user_data);
    if (surface != NULL) {
        cairo_surface_write_to_png(surface, "build/linux-smoke.png");
        cairo_surface_destroy(surface);
    }
    if (error != NULL) g_error_free(error);
    finish_smoke(status);
}

static void on_title(GObject *object, GParamSpec *pspec, gpointer user_data) {
    const char *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(object));
    (void)pspec;
    (void)user_data;
    if (!g_smoke || title == NULL) return;
    if (strcmp(title, "crossweb-smoke-ok") != 0 && strcmp(title, "crossweb-smoke-fail") != 0) return;
    {
        int status = strcmp(title, "crossweb-smoke-ok") == 0 ? 0 : 1;
#if !CROSSWEB_HOTRELOAD
        if (status == 0 && (fs_commands_entered != 0 || app_ping_entered < 1)) {
            fprintf(stderr, "smoke: page reported success but fs_entered=%d ping_entered=%d\n",
                    fs_commands_entered, app_ping_entered);
            status = 1;
        }
        fprintf(stderr, "smoke: title=%s fs_entered=%d ping_entered=%d\n",
                title, fs_commands_entered, app_ping_entered);
#else
        fprintf(stderr, "smoke: title=%s\n", title);
#endif
        webkit_web_view_get_snapshot(WEBKIT_WEB_VIEW(object), WEBKIT_SNAPSHOT_REGION_VISIBLE,
                                     WEBKIT_SNAPSHOT_OPTIONS_NONE, NULL, on_snapshot, GINT_TO_POINTER(status));
    }
}

static void on_message(WebKitUserContentManager *manager, WebKitJavascriptResult *result, gpointer user_data) {
    JSCValue *value;
    char *text;
    (void)manager;
    (void)user_data;
    value = webkit_javascript_result_get_js_value(result);
    if (!jsc_value_is_string(value)) return;
    text = jsc_value_to_string(value);
    if (!ipc_handle_js_message(text)) {
        fprintf(stderr, "IPC: dropped malformed message\n");
    }
    g_free(text);
    drain_ipc();
}

static gboolean on_smoke_timeout(gpointer user_data) {
    (void)user_data;
    fprintf(stderr, "smoke: timed out before the page reported a result\n");
    finish_smoke(1);
    return G_SOURCE_REMOVE;
}

static int debug_enabled(void) {
    const char *env = getenv("CROSSWEB_WEBVIEW_DEBUG");
    if (env != NULL && strcmp(env, "1") == 0) return 1;
#if CROSSWEB_WEBVIEW_DEBUG
    return 1;
#else
    return 0;
#endif
}

int main(int argc, char **argv) {
    const char *dev_url;
    const char *err = NULL;
    int i;
    int kept = 1;
    WebKitWebContext *context;
    WebKitUserContentManager *manager;
    WebKitUserScript *user_script;
    WebKitSettings *settings;
    GtkWidget *window;
    const char *start_uri;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke") == 0) g_smoke = 1;
        else argv[kept++] = argv[i];
    }
    argc = kept;
    argv[argc] = NULL;

    if (!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "crossweb: GTK could not open a display\n");
        return 1;
    }
    dev_url = getenv("CROSSWEB_DEV_URL");
    if (host_prepare("crossweb.permissions", dev_url, g_dev_origin, sizeof(g_dev_origin), &err) != 0) {
        fprintf(stderr, "crossweb: %s\n", err ? err : "configuration rejected");
        return 1;
    }
    if (!assets_find_root(argc > 0 ? argv[0] : NULL, g_assets, sizeof(g_assets))) {
        fprintf(stderr, "crossweb: bundled assets were not found; refusing to fall back to a dev server\n");
        return 1;
    }
    if (!reload_libplug()) return 1;
    ipc_init(NULL);
    ipc_set_eval(linux_eval, NULL);
    plug_init(NULL);

    context = webkit_web_context_new();
    webkit_web_context_set_sandbox_enabled(context, TRUE);
    webkit_web_context_register_uri_scheme(context, "crossweb", on_scheme, NULL, NULL);
    {
        WebKitSecurityManager *security = webkit_web_context_get_security_manager(context);
        webkit_security_manager_register_uri_scheme_as_secure(security, "crossweb");
        webkit_security_manager_register_uri_scheme_as_cors_enabled(security, "crossweb");
    }

    manager = webkit_user_content_manager_new();
    g_signal_connect(manager, "script-message-received::crossweb", G_CALLBACK(on_message), NULL);
    if (!webkit_user_content_manager_register_script_message_handler(manager, "crossweb")) {
        fprintf(stderr, "crossweb: failed to register the native bridge\n");
        return 1;
    }
    user_script = webkit_user_script_new(bridge_js, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
    webkit_user_content_manager_add_script(manager, user_script);
    webkit_user_script_unref(user_script);

    g_view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                                          "web-context", context,
                                          "user-content-manager", manager,
                                          NULL));
    settings = webkit_web_view_get_settings(g_view);
    webkit_settings_set_enable_developer_extras(settings, debug_enabled());
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(settings, FALSE);
    webkit_settings_set_allow_file_access_from_file_urls(settings, FALSE);
    webkit_settings_set_allow_universal_access_from_file_urls(settings, FALSE);
    g_signal_connect(g_view, "decide-policy", G_CALLBACK(on_decide_policy), NULL);
    g_signal_connect(g_view, "notify::title", G_CALLBACK(on_title), NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 640);
    gtk_window_set_title(GTK_WINDOW(window), "Crossweb");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(g_view));
    gtk_widget_show_all(window);

    start_uri = (g_dev_origin[0] != '\0' && dev_url != NULL) ? dev_url : CROSSWEB_APP_PAGE;
    webkit_web_view_load_uri(g_view, start_uri);
    if (g_smoke) g_smoke_timeout = g_timeout_add(15000, on_smoke_timeout, NULL);
    gtk_main();
    ipc_set_eval(NULL, NULL);
    ipc_deinit();
    return g_smoke ? g_smoke_status : 0;
}
