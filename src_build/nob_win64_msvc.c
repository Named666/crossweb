#define CROSSWEB_TARGET_NAME "win64-msvc"

bool build_crossweb(void)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

#ifdef CROSSWEB_HOTRELOAD
    procs.count = 0;
        cmd.count = 0;
            nob_cmd_append(&cmd, "cl.exe");
            nob_cmd_append(&cmd, "/LD");
            nob_cmd_append(&cmd, "/Fobuild\\", "/Fe./build/libplug.dll");
            nob_cmd_append(&cmd, "/I", "./");
            nob_cmd_append(&cmd, "/I", "webview-c/ms.webview2/include");
            nob_cmd_append(&cmd,
                "src/plug.c",
                "src/ipc.c",
                "src/webview.c");
            nob_cmd_append(&cmd,
                "/link",
                "user32.lib", "ole32.lib", "oleaut32.lib");
        nob_da_append(&procs, nob_cmd_run_async(cmd));

        cmd.count = 0;
            nob_cmd_append(&cmd, "cl.exe");
            nob_cmd_append(&cmd, "/I", "./");
            nob_cmd_append(&cmd, "/I", "webview-c/ms.webview2/include");
            nob_cmd_append(&cmd, "/Fobuild\\", "/Febuild\\crossweb.exe");
            nob_cmd_append(&cmd,
                "./src/webview_main.c",
                "./src/hotreload_windows.c",
                "./src/webview.c");
            nob_cmd_append(&cmd,
                "/link",
                "/SUBSYSTEM:CONSOLE",
                "user32.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib");
        nob_da_append(&procs, nob_cmd_run_async(cmd));
    if (!nob_procs_wait(procs)) nob_return_defer(false);
#else
    cmd.count = 0;
        nob_cmd_append(&cmd, "cl.exe");
        nob_cmd_append(&cmd, "/I", "./");
        nob_cmd_append(&cmd, "/I", "webview-c/ms.webview2/include");
        nob_cmd_append(&cmd, "/Fobuild\\", "/Febuild\\crossweb.exe");
        nob_cmd_append(&cmd,
            "./src/webview_main.c",
            "./src/plug.c",
            "./src/ipc.c",
            "./src/webview.c",
            "./src/hotreload_windows.c");
        nob_cmd_append(&cmd,
            "/link",
            "/SUBSYSTEM:CONSOLE",
            "user32.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib");
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
#endif // CROSSWEB_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    return result;
}

bool build_dist(void)
{
#ifdef CROSSWEB_HOTRELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    if (!nob_mkdir_if_not_exists("./crossweb-win64-msvc/")) return false;
    if (!nob_copy_file("./build/crossweb.exe", "./crossweb-win64-msvc/crossweb.exe")) return false;
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
