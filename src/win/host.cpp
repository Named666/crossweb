/* Windows host adapter.
 *
 * WebView2's public control API is a COM/C++ interface (WIL/WRL). This file is
 * the unavoidable adapter. The C99 core (plug, ipc, origin, assets, fs) stays
 * in C. CreateCoreWebView2EnvironmentWithOptions is called with a null browser
 * folder, so the process uses the installed Evergreen WebView2 runtime shared
 * with Microsoft Edge. This does not bundle Chromium, CEF, Electron, or a
 * Fixed Version runtime.
 *
 * Build (not run) requires the WebView2 SDK headers and loader library:
 *   WEBVIEW2_INCLUDE  NuGet Microsoft.Web.WebView2 build/native/include
 *   WEBVIEW2_LIB      NuGet Microsoft.Web.WebView2 build/native/<arch>
 * The SDK loader is not the browser. The browser must already be installed.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "safe_open.h"
#include "ipc.h"
#include "plug.h"
#include "origin.h"
#include "assets_map.h"
#include "host_config.h"
#include "plugins/fs/path.h"
#include "plugins/app/plugin.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

static ComPtr<ICoreWebView2> g_view;
static ComPtr<ICoreWebView2Controller> g_controller;
static HWND g_window = NULL;
static char g_assets[SAFE_OPEN_PATH_MAX];
static char g_dev_origin[512];
static wchar_t g_assets_wide[SAFE_OPEN_PATH_MAX];
static int g_smoke = 0;
static int g_smoke_status = 2;

static const wchar_t *bridge_js =
    L"(function(){"
    L"if(window.external&&window.external.__bridgeInstalled)return;"
    L"window.external=window.external||{};"
    L"window.external.__bridgeInstalled=true;"
    L"var SEP=String.fromCharCode(30);"
    L"function encodePayload(value){"
    L"if(value===undefined||value===null)return '';"
    L"var text=(typeof value==='string')?value:JSON.stringify(value);"
    L"var bytes=new TextEncoder().encode(text);"
    L"var bin='';"
    L"for(var i=0;i<bytes.length;i++)bin+=String.fromCharCode(bytes[i]);"
    L"return btoa(bin);"
    L"}"
    L"window.external.invoke=function(cmd,payload){"
    L"var id=Math.random().toString(36).slice(2)+Date.now().toString(36);"
    L"var normalized=(payload===undefined||payload===null)?'':(typeof payload==='string'?payload:JSON.stringify(payload));"
    L"var encoded=encodePayload(normalized);"
    L"window.chrome.webview.postMessage(id+SEP+String(cmd||'')+SEP+encoded);"
    L"return id;"
    L"};"
    L"})();";

static bool debug_enabled(void) {
    const char *env = getenv("CROSSWEB_WEBVIEW_DEBUG");
    if (env != NULL && strcmp(env, "1") == 0) return true;
#if CROSSWEB_WEBVIEW_DEBUG
    return true;
#else
    return false;
#endif
}

static void narrow_copy(const wchar_t *wide, char *out, size_t cap) {
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)cap, NULL, NULL);
}

static void widen_copy(const char *text, wchar_t *out, size_t cap) {
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out, (int)cap);
}

static bool uri_trusted(const wchar_t *wide) {
    char uri[2048];
    narrow_copy(wide, uri, sizeof(uri));
    return origin_is_trusted(uri, CROSSWEB_WINDOWS_APP_ORIGIN, g_dev_origin);
}

static char g_pending_id[IPC_MAX_ID_LEN];

static void respond_to_js(const char *response) {
    ipc_response(g_pending_id, response ? response : "{\"ok\":false}");
}

static void drain_ipc_real(void) {
    IpcMessage msg;
    while (ipc_receive(&msg)) {
        snprintf(g_pending_id, sizeof(g_pending_id), "%s", msg.id);
        plug_invoke(msg.cmd, msg.payload, respond_to_js);
    }
}

static bool win_eval(const char *script, void *userdata) {
    wchar_t wide[65536];
    (void)userdata;
    if (g_view == NULL || script == NULL) return false;
    if (MultiByteToWideChar(CP_UTF8, 0, script, -1, wide, (int)(sizeof(wide) / sizeof(wide[0]))) == 0) {
        return false;
    }
    g_view->ExecuteScript(wide, nullptr);
    return true;
}

static void resize_webview(void) {
    if (g_controller == NULL || g_window == NULL) return;
    RECT bounds;
    GetClientRect(g_window, &bounds);
    g_controller->put_Bounds(bounds);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_SIZE:
        resize_webview();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(g_smoke ? g_smoke_status : 0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static void note_smoke_title(const wchar_t *title) {
    char narrow[128];
    if (!g_smoke || title == NULL) return;
    narrow_copy(title, narrow, sizeof(narrow));
    if (strcmp(narrow, "crossweb-smoke-ok") != 0 && strcmp(narrow, "crossweb-smoke-fail") != 0) return;
    g_smoke_status = strcmp(narrow, "crossweb-smoke-ok") == 0 ? 0 : 1;
    if (g_smoke_status == 0 && (fs_commands_entered != 0 || app_ping_entered < 1)) {
        fprintf(stderr, "smoke: page reported success but fs_entered=%d ping_entered=%d\n",
                fs_commands_entered, app_ping_entered);
        g_smoke_status = 1;
    }
    PostQuitMessage(g_smoke_status);
}

static HRESULT on_controller_created(HRESULT hr, ICoreWebView2Controller *controller) {
    ComPtr<ICoreWebView2Settings> settings;
    ComPtr<ICoreWebView2_3> view3;
    if (FAILED(hr) || controller == NULL) return hr;
    g_controller = controller;
    g_controller->get_CoreWebView2(&g_view);
    g_controller->put_IsVisible(TRUE);
    resize_webview();
    g_view->get_Settings(&settings);
    settings->put_AreDevToolsEnabled(debug_enabled() ? TRUE : FALSE);
    settings->put_AreDefaultScriptDialogsEnabled(FALSE);
    settings->put_IsWebMessageEnabled(TRUE);
    if (SUCCEEDED(g_view->QueryInterface(IID_PPV_ARGS(&view3)))) {
        view3->SetVirtualHostNameToFolderMapping(
            L"crossweb.localhost", g_assets_wide, COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY);
    }
    g_view->AddScriptToExecuteOnDocumentCreated(bridge_js, nullptr);
    g_view->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
            LPWSTR message = nullptr;
            char narrow[IPC_MAX_PAYLOAD_LEN + IPC_MAX_CMD_LEN + IPC_MAX_ID_LEN + 8];
            args->TryGetWebMessageAsString(&message);
            if (message != NULL) {
                narrow_copy(message, narrow, sizeof(narrow));
                CoTaskMemFree(message);
                if (!ipc_handle_js_message(narrow)) {
                    fprintf(stderr, "IPC: dropped malformed message\n");
                }
                drain_ipc_real();
            }
            return S_OK;
        }).Get(), nullptr);
    g_view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            if (uri != NULL && !uri_trusted(uri)) args->put_Cancel(TRUE);
            CoTaskMemFree(uri);
            return S_OK;
        }).Get(), nullptr);
    g_view->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    g_view->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
        [](ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
            ComPtr<ICoreWebView2WebResourceRequest> request;
            ComPtr<ICoreWebView2Environment> environment;
            ComPtr<ICoreWebView2_2> sender2;
            LPWSTR uri = nullptr;
            args->get_Request(&request);
            request->get_Uri(&uri);
            if (uri != NULL && !uri_trusted(uri) && SUCCEEDED(sender->QueryInterface(IID_PPV_ARGS(&sender2)))) {
                sender2->get_Environment(&environment);
                ComPtr<ICoreWebView2WebResourceResponse> response;
                environment->CreateWebResourceResponse(nullptr, 403, L"Forbidden", L"", &response);
                args->put_Response(response.Get());
            }
            CoTaskMemFree(uri);
            return S_OK;
        }).Get(), nullptr);
    g_view->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
        [](ICoreWebView2 *sender, IUnknown *) -> HRESULT {
            LPWSTR title = nullptr;
            sender->get_DocumentTitle(&title);
            note_smoke_title(title);
            CoTaskMemFree(title);
            return S_OK;
        }).Get(), nullptr);
    if (g_dev_origin[0] != '\0') {
        wchar_t dev[2048];
        const char *url = getenv("CROSSWEB_DEV_URL");
        widen_copy(url ? url : "", dev, sizeof(dev) / sizeof(dev[0]));
        g_view->Navigate(dev);
    } else {
        g_view->Navigate(L"https://crossweb.localhost/index.html");
    }
    return S_OK;
}

static HRESULT on_environment_created(HRESULT hr, ICoreWebView2Environment *environment) {
    if (FAILED(hr) || environment == NULL) {
        fprintf(stderr, "crossweb: WebView2 runtime was not created (hr=0x%08lx). Install the Evergreen WebView2 runtime.\n",
                (unsigned long)hr);
        PostQuitMessage(1);
        return hr;
    }
    return environment->CreateCoreWebView2Controller(
        g_window,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
                return on_controller_created(result, controller);
            }).Get());
}

int main(int argc, char **argv) {
    const char *err = NULL;
    const char *dev_url;
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW window_class;
    MSG msg;
    wchar_t user_data[MAX_PATH];
    int i;
    int kept = 1;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke") == 0) g_smoke = 1;
        else argv[kept++] = argv[i];
    }
    argc = kept;

    dev_url = getenv("CROSSWEB_DEV_URL");
    if (host_prepare("crossweb.permissions", dev_url, g_dev_origin, sizeof(g_dev_origin), &err) != 0) {
        fprintf(stderr, "crossweb: %s\n", err ? err : "configuration rejected");
        return 1;
    }
    if (!assets_find_root(argc > 0 ? argv[0] : NULL, g_assets, sizeof(g_assets))) {
        fprintf(stderr, "crossweb: bundled assets were not found; refusing to fall back to a dev server\n");
        return 1;
    }
    widen_copy(g_assets, g_assets_wide, sizeof(g_assets_wide) / sizeof(g_assets_wide[0]));
    ipc_init(NULL);
    ipc_set_eval(win_eval, NULL);
    plug_init(NULL);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"CrosswebWindow";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&window_class);
    g_window = CreateWindowExW(0, L"CrosswebWindow", L"Crossweb", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 960, 720,
                               NULL, NULL, instance, NULL);
    if (g_window == NULL) return 1;
    ShowWindow(g_window, SW_SHOW);

    if (ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Crossweb\\WebView2", user_data, MAX_PATH) == 0) {
        wcscpy_s(user_data, MAX_PATH, L".\\WebView2");
    }
    /* nullptr browserExecutableFolder selects the installed shared runtime. */
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, user_data, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {
                return on_environment_created(result, environment);
            }).Get());

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ipc_set_eval(NULL, NULL);
    ipc_deinit();
    CoUninitialize();
    return g_smoke ? g_smoke_status : (int)msg.wParam;
}
