#include "common.h"
#include "plugins.h"

/* Windows shell links the system WebView2 loader, not a bundled browser.
   WEBVIEW2_INCLUDE and WEBVIEW2_LIB point at the NuGet SDK bindings. */

bool build_dist(void)
{
    const char *include_dir = getenv("WEBVIEW2_INCLUDE");
    const char *lib_dir = getenv("WEBVIEW2_LIB");
    bool result = true;
    Cmd cmd = {0};
    Nob_File_Paths plugin_sources = {0};

    if (include_dir == NULL || include_dir[0] == '\0' || lib_dir == NULL || lib_dir[0] == '\0') {
        nob_log(NOB_ERROR, "Windows host needs the WebView2 SDK headers and loader library.");
        nob_log(NOB_ERROR, "Set WEBVIEW2_INCLUDE and WEBVIEW2_LIB to Microsoft.Web.WebView2 build/native paths.");
        nob_log(NOB_ERROR, "Do not vendor the Fixed Version runtime. The app uses the installed Evergreen WebView2.");
        return false;
    }
    if (!collect_plugin_sources(PLATFORM_DESKTOP, &plugin_sources)) {
        nob_log(NOB_ERROR, "Failed to collect plugin sources");
        return false;
    }
#if CROSSWEB_HOTRELOAD
    nob_log(NOB_WARNING, "The WebView2 host does not hot-reload native code yet; building a static shell.");
#endif
    /* C translation units stay C. g++ would reject some of them as C++. */
    {
        const char *c_sources[] = {
            "./src/plug.c",
            "./src/ipc.c",
            "./src/origin.c",
            "./src/assets_map.c",
            "./src/host_config.c",
            "./src/safe_open.c",
        };
        size_t n;
        for (n = 0; n < sizeof(c_sources) / sizeof(c_sources[0]); ++n) {
            const char *obj = nob_temp_sprintf("./build/win-%zu.o", n);
            cmd_append(&cmd, "gcc", "-std=c99", "-Wall", "-Wextra", "-I.", "-I", include_dir);
            cmd_append(&cmd, "-include", "build/config.h", "-c", c_sources[n], "-o", obj);
            if (!cmd_run(&cmd)) return_defer(false);
        }
        for (n = 0; n < plugin_sources.count; ++n) {
            const char *obj = nob_temp_sprintf("./build/win-plugin-%zu.o", n);
            cmd_append(&cmd, "gcc", "-std=c99", "-Wall", "-Wextra", "-I.", "-I", include_dir);
            cmd_append(&cmd, "-include", "build/config.h", "-c", plugin_sources.items[n], "-o", obj);
            if (!cmd_run(&cmd)) return_defer(false);
        }
        cmd_append(&cmd, "g++", "-std=c++17", "-Wall", "-Wextra", "-I.", "-I", include_dir);
        cmd_append(&cmd, "-include", "build/config.h", "-c", "./src/win/host.cpp", "-o", "./build/win-host.o");
        if (!cmd_run(&cmd)) return_defer(false);
    }
    cmd_append(&cmd, "g++", "-o", "./build/crossweb.exe", "./build/win-host.o");
    {
        size_t n;
        for (n = 0; n < 6; ++n) cmd_append(&cmd, nob_temp_sprintf("./build/win-%zu.o", n));
        for (n = 0; n < plugin_sources.count; ++n) cmd_append(&cmd, nob_temp_sprintf("./build/win-plugin-%zu.o", n));
    }
    cmd_append(&cmd, "-L", lib_dir);
    cmd_append(&cmd, "-lWebView2LoaderStatic");
    cmd_append(&cmd, "-lole32", "-luser32", "-loleaut32", "-luuid", "-lgdi32", "-ladvapi32", "-lshell32");
    if (!cmd_run(&cmd)) return_defer(false);
    nob_log(NOB_INFO, "Linked crossweb.exe against WebView2LoaderStatic. Runtime verification still needs a Windows machine with the Evergreen runtime installed.");

defer:
    nob_cmd_free(cmd);
    nob_da_free(plugin_sources);
    return result;
}
