#define MUSIALIZER_TARGET_NAME "OpenBSD"

bool build_musializer(void)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

#ifdef MUSIALIZER_HOTRELOAD
        procs.count = 0;
            cmd.count = 0;
                // TODO: add a way to replace `cc` with something else GCC compatible on POSIX
                // Like `clang` for instance
                nob_cmd_append(&cmd, "cc");
                nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
                nob_cmd_append(&cmd, "-I.");
                nob_cmd_append(&cmd, "-fPIC", "-shared");
                nob_cmd_append(&cmd, "-o", "./build/libplug.so");
                nob_cmd_append(&cmd, "./thirdparty/tinyfiledialogs.c");
                nob_cmd_append(&cmd,
                    "./src/plug.c",
                    "./src/ffmpeg_posix.c");
                (void)0;
                nob_cmd_append(&cmd, "-lm", "-lpthread");
            nob_da_append(&procs, nob_cmd_run_async(cmd));

            cmd.count = 0;
                nob_cmd_append(&cmd, "cc");
                nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
                nob_cmd_append(&cmd, "-I.");
                nob_cmd_append(&cmd, "-o", "./build/musializer");
                nob_cmd_append(&cmd,
                    "./src/webview.c",
                    "./src/hotreload_posix.c");
                nob_cmd_append(&cmd, "-lm", "-lpthread");
            nob_da_append(&procs, nob_cmd_run_async(cmd));
        if (!nob_procs_wait(procs)) nob_return_defer(false);
#else
        cmd.count = 0;
            nob_cmd_append(&cmd, "cc");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
            nob_cmd_append(&cmd, "-I.");
            nob_cmd_append(&cmd, "-o", "./build/musializer");
            nob_cmd_append(&cmd, "./thirdparty/tinyfiledialogs.c");
            nob_cmd_append(&cmd,
                "./src/plug.c",
                "./src/ffmpeg_posix.c",
                "./src/webview.c");
            nob_cmd_append(&cmd, "-lm", "-lpthread");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
#endif // MUSIALIZER_HOTRELOAD

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    return result;
}

bool build_dist()
{
#ifdef MUSIALIZER_HOTRELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    if (!nob_mkdir_if_not_exists("./musializer-openbsd-x86_64/")) return false;
    if (!nob_copy_file("./build/musializer", "./musializer-openbsd-x86_64/musializer")) return false;
    if (!nob_copy_directory_recursively("./resources/", "./musializer-openbsd-x86_64/resources/")) return false;
    // TODO: should we pack ffmpeg with Linux build?
    // There are some static executables for Linux
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "tar", "fvcz", "./musializer-openbsd-x86_64.tar.gz", "./musializer-openbsd-x86_64");
    bool ok = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    if (!ok) return false;

    return true;
#endif // MUSIALIZER_HOTRELOAD
}
