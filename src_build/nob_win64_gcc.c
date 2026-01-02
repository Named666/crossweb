#include "common.h"
#include "plugins.h"


bool build_dist(void)
{
    bool result = true;
    Cmd cmd = {0};
    Procs procs = {0};

#ifdef CROSSWEB_HOTRELOAD
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-DWEBVIEW_WINAPI=1");
    cmd_append(&cmd, "-I", "webview-c/ms.webview2/include");
    cmd_append(&cmd, "-fPIC", "-shared");
    cmd_append(&cmd, "-static-libgcc");
    cmd_append(&cmd, "-o", "./build/libplug.dll");
    cmd_append(&cmd,
        "./src/plug.c",
        "./src/ipc.c",
        "./src/plugins/fs/lib.c",
        "./src/plugins/fs/commands.c",
        "./src/plugins/fs/desktop.c",
        "./src/plugins/fs/error.c");
    cmd_append(&cmd, "-lole32", "-loleaut32");
    if (!cmd_run(&cmd, .async = &procs)) return_defer(false);

    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd,
        "./src/webview.c",
        "./src/hotreload_windows.c");
    cmd_append(&cmd, "-lole32", "-lcomctl32", "-loleaut32", "-luuid", "-lgdi32", "-ladvapi32");
    if (!cmd_run(&cmd, .async = &procs)) return_defer(false);
    if (!nob_procs_wait(procs)) return_defer(false);
#else
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-DWEBVIEW_WINAPI=1");
    cmd_append(&cmd, "-I", "webview-c/ms.webview2/include");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd,
        "./src/plug.c",
        "./src/ipc.c",
        "./src/webview.c",
        "./src/plugins/fs/lib.c",
        "./src/plugins/fs/commands.c",
        "./src/plugins/fs/desktop.c",
        "./src/plugins/fs/error.c");
    cmd_append(&cmd, "-lole32", "-lcomctl32", "-loleaut32", "-luuid", "-lgdi32", "-ladvapi32");
    if (!cmd_run(&cmd)) return_defer(false);
#endif // CROSSWEB_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    return result;
}
