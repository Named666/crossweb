#include "common.h"
#include "plugins.h"

bool build_dist(void)
{
    bool result = true;
    Cmd cmd = {0};
    Procs procs = {0};
    
    // Collect plugin source files automatically
    Nob_File_Paths plugin_sources = {0};
    if (!collect_plugin_sources(PLATFORM_DESKTOP, &plugin_sources)) {
        nob_log(NOB_ERROR, "Failed to collect plugin sources");
        return false;
    }
    
    // Collect plugin library dependencies
    Nob_File_Paths plugin_libs = {0};
    collect_plugin_libs(&plugin_libs);

#ifdef CROSSWEB_HOTRELOAD
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-DCROSSWEB_BUILDING_PLUG=1");
    cmd_append(&cmd, "-DWEBVIEW_WINAPI");
    cmd_append(&cmd, "-fPIC", "-shared");
    cmd_append(&cmd, "-static-libgcc");
    cmd_append(&cmd, "-Wno-implicit-function-declaration");
    cmd_append(&cmd, "-o", "./build/libplug.dll");
    cmd_append(&cmd, "./src/plug.c");
    
    // Add all discovered plugin sources
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    
    cmd_append(&cmd, "-lole32", "-loleaut32");
    
    // Add plugin-specific libraries
    for (size_t i = 0; i < plugin_libs.count; ++i) {
        cmd_append(&cmd, plugin_libs.items[i]);
    }
    
    if (!cmd_run(&cmd)) return_defer(false);

    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-DWEBVIEW_WINAPI");
    cmd_append(&cmd, "-I", "./thirdparty/webview-c/ms.webview2/include");
    cmd_append(&cmd, "-Wno-implicit-function-declaration");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd,
        "./src/webview.c",
        "./src/ipc.c",
        "./src/hotreload_windows.c");
    cmd_append(&cmd, "-lole32", "-lcomctl32", "-loleaut32", "-luuid", "-lgdi32", "-ladvapi32");
    
    // Add plugin-specific libraries
    for (size_t i = 0; i < plugin_libs.count; ++i) {
        cmd_append(&cmd, plugin_libs.items[i]);
    }
    
    if (!cmd_run(&cmd)) {
        nob_log(NOB_WARNING, "Could not build crossweb.exe (it might be running).");
    }

    // Ensure WebView2Loader.dll is available at runtime (otherwise WebView2 init
    // fails and the app falls back to the legacy IE engine).
    {
        const char *dst = "./build/WebView2Loader.dll";
        const char *src1 = "./thirdparty/webview-c/ms.webview2/x64/WebView2Loader.dll";
        const char *src2 = "../thirdparty/webview-c/ms.webview2/x64/WebView2Loader.dll";
        if (file_exists(src1) == 1) {
            if (!copy_file(src1, dst)) {
                nob_log(NOB_WARNING, "Could not copy WebView2Loader.dll (it might be running).");
            }
        } else if (file_exists(src2) == 1) {
            if (!copy_file(src2, dst)) {
                nob_log(NOB_WARNING, "Could not copy WebView2Loader.dll (it might be running).");
            }
        } else {
            nob_log(NOB_WARNING, "WebView2Loader.dll not found at '%s' or '%s'. WebView2 may not start.", src1, src2);
        }
    }
#else
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-DWEBVIEW_WINAPI=1");
    cmd_append(&cmd, "-I", "./thirdparty/webview-c/ms.webview2/include");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd, "./src/plug.c", "./src/ipc.c", "./src/webview.c");
    
    // Add all discovered plugin sources
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    
    cmd_append(&cmd, "-lole32", "-lcomctl32", "-loleaut32", "-luuid", "-lgdi32", "-ladvapi32");
    
    // Add plugin-specific libraries
    for (size_t i = 0; i < plugin_libs.count; ++i) {
        cmd_append(&cmd, plugin_libs.items[i]);
    }
    
    if (!cmd_run(&cmd)) return_defer(false);

    // Ensure WebView2Loader.dll is available at runtime (otherwise WebView2 init
    // fails and the app falls back to the legacy IE engine).
    {
        const char *dst = "./build/WebView2Loader.dll";
        const char *src1 = "./thirdparty/webview-c/ms.webview2/x64/WebView2Loader.dll";
        const char *src2 = "../thirdparty/webview-c/ms.webview2/x64/WebView2Loader.dll";
        if (file_exists(src1) == 1) {
            if (!copy_file(src1, dst)) return_defer(false);
        } else if (file_exists(src2) == 1) {
            if (!copy_file(src2, dst)) return_defer(false);
        } else {
            nob_log(NOB_WARNING, "WebView2Loader.dll not found at '%s' or '%s'. WebView2 may not start.", src1, src2);
        }
    }
#endif // CROSSWEB_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    nob_da_free(plugin_sources);
    nob_da_free(plugin_libs);
    return result;
}
