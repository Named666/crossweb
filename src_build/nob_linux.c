#include "common.h"
#include "plugins.h"

#include <stdio.h>
#include <string.h>

static bool append_pkg_config(Cmd *cmd) {
    static const char *packages[] = {"webkit2gtk-4.1", "webkit2gtk-4.0"};
    size_t i;
    for (i = 0; i < sizeof(packages) / sizeof(packages[0]); ++i) {
        char command[256];
        char line[8192];
        FILE *pipe;
        int status;
        char *cursor;
        snprintf(command, sizeof(command), "pkg-config --cflags --libs %s", packages[i]);
        pipe = popen(command, "r");
        if (pipe == NULL) continue;
        if (fgets(line, sizeof(line), pipe) == NULL) {
            pclose(pipe);
            continue;
        }
        status = pclose(pipe);
        if (status != 0) continue;
        cursor = line;
        while (*cursor) {
            char *tok = cursor;
            while (*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') cursor++;
            if (*cursor) {
                *cursor = '\0';
                cursor++;
            }
            if (tok[0] != '\0') cmd_append(cmd, tok);
        }
        nob_log(NOB_INFO, "Using the distro WebKitGTK backend from pkg-config package %s", packages[i]);
        return true;
    }
    nob_log(NOB_ERROR, "WebKitGTK development files were not found.");
    nob_log(NOB_ERROR, "Install the distro packages, for example:");
    nob_log(NOB_ERROR, "  sudo apt install pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev xvfb");
    nob_log(NOB_ERROR, "Crossweb does not download or bundle a browser engine.");
    return false;
}

static void append_host_sources(Cmd *cmd) {
    cmd_append(cmd,
        "./src/host_linux.c",
        "./src/origin.c",
        "./src/assets_map.c",
        "./src/host_config.c",
        "./src/safe_open.c",
        "./src/ipc.c");
}

bool build_dist(void)
{
    bool result = true;
    Cmd cmd = {0};
    Procs procs = {0};

    Nob_File_Paths plugin_sources = {0};
    if (!collect_plugin_sources(PLATFORM_DESKTOP, &plugin_sources)) {
        nob_log(NOB_ERROR, "Failed to collect plugin sources");
        return false;
    }

    nob_log(NOB_INFO, "Building the Linux host against the system WebKitGTK WebView");

#if CROSSWEB_HOTRELOAD
    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-DCROSSWEB_BUILDING_PLUG=1");
    cmd_append(&cmd, "-fPIC", "-shared");
    cmd_append(&cmd, "-o", "./build/libplug.so");
    cmd_append(&cmd, "./src/plug.c");
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    if (!cmd_run(&cmd)) return_defer(false);

    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-o", "./build/crossweb");
    append_host_sources(&cmd);
    cmd_append(&cmd, "./src/hotreload_posix.c");
    cmd_append(&cmd, "-Wl,-rpath=./build/", "-Wl,-rpath=./");
    cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    if (!append_pkg_config(&cmd)) return_defer(false);
    if (!cmd_run(&cmd)) return_defer(false);
#else
    cmd_append(&cmd, "cc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd, "./src/plug.c");
    append_host_sources(&cmd);
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    cmd_append(&cmd, "-lm", "-ldl", "-lpthread");
    if (!append_pkg_config(&cmd)) return_defer(false);
    if (!cmd_run(&cmd)) return_defer(false);
#endif

defer:
    cmd_free(cmd);
    da_free(procs);
    nob_da_free(plugin_sources);
    return result;
}
