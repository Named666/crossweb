#define CROSSWEB_TARGET_NAME "win64-gcc"

bool build_crossweb(void)
{
    bool result = true;
    Cmd cmd = {0};
    Procs procs = {0};

#ifdef CROSSWEB_HOTRELOAD
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
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
        "./src/hotreload_windows.c");
    cmd_append(&cmd, "-lole32", "-loleaut32", "-ladvapi32");
    if (!cmd_run(&cmd, .async = &procs)) return_defer(false);
    if (!nob_procs_wait(procs)) return_defer(false);
#else
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-o", "./build/crossweb");
    cmd_append(&cmd,
        "./src/plug.c",
        "./src/ipc.c",
        "./src/hotreload_windows.c",
        "./src/plugins/fs/lib.c",
        "./src/plugins/fs/commands.c",
        "./src/plugins/fs/desktop.c",
        "./src/plugins/fs/error.c");
    cmd_append(&cmd, "-lole32", "-loleaut32", "-ladvapi32");
    if (!cmd_run(&cmd)) return_defer(false);
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
    if (!nob_mkdir_if_not_exists("./crossweb-win64-gcc/")) return false;
    if (!nob_copy_file("./build/crossweb.exe", "./crossweb-win64-gcc/crossweb.exe")) return false;
    if (!nob_copy_directory_recursively("./web/", "./crossweb-win64-gcc/web/")) return false;
    Nob_Cmd cmd = {0};
    const char *dist_path = "./crossweb-win64-gcc.zip";
    nob_cmd_append(&cmd, "zip", "-r", dist_path, "./crossweb-win64-gcc/");
    bool ok = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    if (!ok) return false;
    nob_log(NOB_INFO, "Created %s", dist_path);
    return true;
#endif // CROSSWEB_HOTRELOAD
}
