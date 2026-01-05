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

#ifdef CROSSWEB_HOTRELOAD
    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-DCROSSWEB_BUILDING_PLUG=1");
    cmd_append(&cmd, "-fPIC", "-shared");
    cmd_append(&cmd, "-o", "./build/libplug.so");
    cmd_append(&cmd, "./src/plug.c", "./src/ipc.c");
    
    // Add all discovered plugin sources
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    
    cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    if (!cmd_run(&cmd)) return_defer(false);

    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd, "./src/webview.c", "./src/hotreload_posix.c");
    cmd_append(&cmd, "-Wl,-rpath=./build/", "-Wl,-rpath=./");
    cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    cmd_append(&cmd, "`pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0`");
    if (!cmd_run(&cmd)) return_defer(false);
#else
    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd, "./src/plug.c", "./src/ipc.c", "./src/webview.c");
    
    // Add all discovered plugin sources
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    
    cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    cmd_append(&cmd, "`pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0`");
    if (!cmd_run(&cmd)) return_defer(false);
#endif // CROSSWEB_HOTRELOAD

defer:
    cmd_free(cmd);
    da_free(procs);
    nob_da_free(plugin_sources);
    return result;
}
