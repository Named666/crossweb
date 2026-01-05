#include "common.h"
#include "plugins.h"

bool build_dist(void)
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
        nob_cmd_append(&cmd, "clang");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-I.");
        nob_cmd_append(&cmd, "-include", "build/config.h");
        nob_cmd_append(&cmd, "-DCROSSWEB_BUILDING_PLUG=1");
        nob_cmd_append(&cmd, "-fPIC", "-shared");
        nob_cmd_append(&cmd, "-o", "./build/libplug.dylib");
        nob_cmd_append(&cmd, "./src/plug.c", "./src/ipc.c");
        
        // Add all discovered plugin sources
        for (size_t i = 0; i < plugin_sources.count; ++i) {
            nob_cmd_append(&cmd, plugin_sources.items[i]);
        }
        
        nob_cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    nob_da_append(&procs, nob_cmd_run_async(cmd));

    cmd.count = 0;
        nob_cmd_append(&cmd, "clang");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-I.");
        nob_cmd_append(&cmd, "-include", "build/config.h");
        nob_cmd_append(&cmd, "-o", "./build/crossweb");
        nob_cmd_append(&cmd, "./src/webview.c", "./src/hotreload_posix.c");
        nob_cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
        nob_cmd_append(&cmd, "-rpath", "./build");
        nob_cmd_append(&cmd, "-rpath", "./");
        nob_cmd_append(&cmd, "-framework", "Cocoa");
        nob_cmd_append(&cmd, "-framework", "WebKit");
    nob_da_append(&procs, nob_cmd_run_async(cmd));
    if (!nob_procs_wait(procs)) nob_return_defer(false);
#else
    cmd.count = 0;
        nob_cmd_append(&cmd, "clang");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-I.");
        nob_cmd_append(&cmd, "-include", "build/config.h");
        nob_cmd_append(&cmd, "-o", "./build/crossweb");
        nob_cmd_append(&cmd, "./src/plug.c", "./src/ipc.c", "./src/webview.c");
        
        // Add all discovered plugin sources
        for (size_t i = 0; i < plugin_sources.count; ++i) {
            nob_cmd_append(&cmd, plugin_sources.items[i]);
        }

        nob_cmd_append(&cmd, "-framework", "Cocoa");
        nob_cmd_append(&cmd, "-framework", "WebKit");
        nob_cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
#endif // CROSSWEB_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    nob_da_free(plugin_sources);
    return result;
}
