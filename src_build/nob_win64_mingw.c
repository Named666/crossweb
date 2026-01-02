#define MUSIALIZER_TARGET_NAME "win64-mingw"

// On windows, mingw doesn't have the `x86_64-w64-mingw32-` prefix for tools such as `windres` or `ar`.
// For gcc, you can use both `x86_64-w64-mingw32-gcc` and just `gcc`
#ifdef _WIN32
#define MAYBE_PREFIXED(x) x
#else
#define MAYBE_PREFIXED(x) "x86_64-w64-mingw32-"x
#endif // _WIN32

bool build_musializer(void)
{
    bool result = true;
    Cmd cmd = {0};
    Procs procs = {0};

    cmd_append(&cmd, MAYBE_PREFIXED("windres"));
    cmd_append(&cmd, "./src/musializer.rc");
    cmd_append(&cmd, "-O", "coff");
    cmd_append(&cmd, "-o", "./build/musializer.res");
    if (!cmd_run(&cmd)) return_defer(false);

#ifdef MUSIALIZER_HOTRELOAD
    cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
    cmd_append(&cmd, "-mwindows", "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-fPIC", "-shared");
    cmd_append(&cmd, "-static-libgcc");
    cmd_append(&cmd, "-o", "./build/libplug.dll");
    cmd_append(&cmd,
        "./src/plug.c",
        "./src/ffmpeg_windows.c",
        "./thirdparty/tinyfiledialogs.c");
    (void)0;
    cmd_append(&cmd, "-lwinmm", "-lgdi32", "-lole32");
    if (!cmd_run(&cmd, .async = &procs)) return_defer(false);

    cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
    cmd_append(&cmd, "-mwindows", "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-o", "./build/musializer");
    cmd_append(&cmd,
        "./src/webview.c",
        "./src/hotreload_windows.c");
    cmd_append(&cmd, "-lwinmm", "-lgdi32");
    if (!cmd_run(&cmd, .async = &procs)) return_defer(false);

    if (!procs_flush(&procs)) return_defer(false);
#else
    cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
    cmd_append(&cmd, "-mwindows", "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-o", "./build/musializer");
    cmd_append(&cmd,
        "./src/plug.c",
        "./src/ffmpeg_windows.c",
        "./src/webview.c",
        "./thirdparty/tinyfiledialogs.c",
        "./build/musializer.res"
        );
    (void)0;
    cmd_append(&cmd, "-lwinmm", "-lgdi32", "-lole32");
    cmd_append(&cmd, "-static");
    if (!cmd_run(&cmd)) return_defer(false);
#endif // MUSIALIZER_HOTRELOAD

defer:
    cmd_free(cmd);
    da_free(procs);
    return result;
}

bool build_dist(void)
{
#ifdef MUSIALIZER_HOTRELOAD
    nob_log(ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    if (!mkdir_if_not_exists("./musializer-win64-mingw/")) return false;
    if (!copy_file("./build/musializer.exe", "./musializer-win64-mingw/musializer.exe")) return false;
    if (!copy_directory_recursively("./resources/", "./musializer-win64-mingw/resources/")) return false;
    if (!copy_file("musializer-logged.bat", "./musializer-win64-mingw/musializer-logged.bat")) return false;
    // TODO: pack ffmpeg.exe with windows build
    //if (!copy_file("ffmpeg.exe", "./musializer-win64-mingw/ffmpeg.exe")) return false;
    Cmd cmd = {0};
    const char *dist_path = "./musializer-win64-mingw.zip";
    cmd_append(&cmd, "zip", "-r", dist_path, "./musializer-win64-mingw/");
    bool ok = cmd_run(&cmd);
    cmd_free(cmd);
    if (!ok) return false;
    nob_log(INFO, "Created %s", dist_path);
    return true;
#endif // MUSIALIZER_HOTRELOAD
}
