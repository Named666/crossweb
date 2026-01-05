#include "common.h"
#include "plugins.h"

bool build_crossweb(void)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    
    // Collect plugin source files automatically
    Nob_File_Paths plugin_sources = {0};
    if (!collect_plugin_sources(PLATFORM_DESKTOP, &plugin_sources)) {
        nob_log(NOB_ERROR, "Failed to collect plugin sources");
        return false;
    }

#ifdef CROSSWEB_HOTRELOAD
    procs.count = 0;
        cmd.count = 0;
            nob_cmd_append(&cmd, "cl.exe");
            nob_cmd_append(&cmd, "/LD");
            nob_cmd_append(&cmd, "/Fobuild\\", "/Fe./build/libplug.dll");
            nob_cmd_append(&cmd, "/I", "./");
            nob_cmd_append(&cmd, "/I", "thirdparty/webview-c/ms.webview2/include");
            nob_cmd_append(&cmd, "/FI", "build/config.h");
            nob_cmd_append(&cmd, "/DCROSSWEB_BUILDING_PLUG=1");
            nob_cmd_append(&cmd, "/DWEBVIEW_WINAPI=1");
            nob_cmd_append(&cmd, "./src/plug.c");
            
            // Add all discovered plugin sources
            for (size_t i = 0; i < plugin_sources.count; ++i) {
                nob_cmd_append(&cmd, plugin_sources.items[i]);
            }
            
            nob_cmd_append(&cmd, "/link", "user32.lib", "ole32.lib", "oleaut32.lib");
#ifdef CROSSWEB_PLUGIN_KEYSTORE
            nob_cmd_append(&cmd, "crypt32.lib", "webauthn.lib");
#endif
        nob_da_append(&procs, nob_cmd_run_async(cmd));

        cmd.count = 0;
            nob_cmd_append(&cmd, "cl.exe");
            nob_cmd_append(&cmd, "/I", "./");
            nob_cmd_append(&cmd, "/I", "thirdparty/webview-c/ms.webview2/include");
            nob_cmd_append(&cmd, "/FI", "build/config.h");
            nob_cmd_append(&cmd, "/DWEBVIEW_WINAPI=1");
            nob_cmd_append(&cmd,
                "./src/hotreload_windows.c",
                "./src/webview.c",
                "./src/ipc.c");
            nob_cmd_append(&cmd,
                "/link",
                "/SUBSYSTEM:CONSOLE",
                "user32.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib", "gdi32.lib", "comctl32.lib");
        nob_da_append(&procs, nob_cmd_run_async(cmd));
    if (!nob_procs_wait(procs)) nob_return_defer(false);
#else
    cmd.count = 0;
        nob_cmd_append(&cmd, "cl.exe");
        nob_cmd_append(&cmd, "/I", "./");
        nob_cmd_append(&cmd, "/I", "thirdparty/webview-c/ms.webview2/include");
        nob_cmd_append(&cmd, "/FI", "build/config.h");
        nob_cmd_append(&cmd, "/DWEBVIEW_WINAPI=1");
        nob_cmd_append(&cmd, "./src/plug.c", "./src/ipc.c", "./src/webview.c");
        
        // Add all discovered plugin sources
        for (size_t i = 0; i < plugin_sources.count; ++i) {
            nob_cmd_append(&cmd, plugin_sources.items[i]);
        }
        
        nob_cmd_append(&cmd,
            "/link",
            "/SUBSYSTEM:CONSOLE",
            "user32.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib", "gdi32.lib", "comctl32.lib");
#ifdef CROSSWEB_PLUGIN_KEYSTORE
        nob_cmd_append(&cmd, "crypt32.lib", "webauthn.lib");
#endif
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
#endif // CROSSWEB_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    nob_da_free(plugin_sources);
    return result;
}

bool build_dist(void)
{
#ifdef CROSSWEB_HOTRELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    if (!build_crossweb()) return false;
    
    if (!nob_mkdir_if_not_exists("./crossweb-win64-msvc/")) return false;
    if (!nob_copy_file("./build/crossweb.exe", "./crossweb-win64-msvc/crossweb.exe")) return false;
    // Required for the WebView2 (Edge/Chromium) backend.
    if (!nob_copy_file("./thirdparty/webview-c/ms.webview2/x64/WebView2Loader.dll", "./crossweb-win64-msvc/WebView2Loader.dll")) return false;
    if (!nob_copy_directory_recursively("./web/", "./crossweb-win64-msvc/web/")) return false;
    Nob_Cmd cmd = {0};
    const char *dist_path = "./crossweb-win64-msvc.zip";
    nob_cmd_append(&cmd, "powershell", "Compress-Archive", "-Path", "./crossweb-win64-msvc/*", "-DestinationPath", dist_path, "-Force");
    bool ok = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    if (!ok) return false;
    nob_log(NOB_INFO, "Created %s", dist_path);
    return true;
#endif // CROSSWEB_HOTRELOAD
}
