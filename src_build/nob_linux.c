#define MUSIALIZER_TARGET_NAME "linux"

bool build_musializer(void)
{
    bool result = true;
    Cmd cmd = {0};
    Procs procs = {0};

#ifdef MUSIALIZER_HOTRELOAD
    // TODO: add a way to replace `cc` with something else GCC compatible on POSIX
    // Like `clang` for instance
    cmd_append(&cmd, "cc",
        "-Wall", "-Wextra", "-ggdb",
        "-I.",
        "-fPIC", "-shared",
        "-o", "./build/libplug.so",
        "./src/plug.c", "./src/ffmpeg_posix.c", "./thirdparty/tinyfiledialogs.c",
        "-O3", "-march=native", "-ffast-math",
        "-lm", "-ldl", "-flto=auto", "-lpthread");
    if (!cmd_run(&cmd)) return_defer(false);

    cmd_append(&cmd, "cc",
        "-Wall", "-Wextra", "-ggdb",
        "-I.",
        "-o", "./build/musializer",
        "./src/webview.c", "./src/hotreload_posix.c",
        "-Wl,-rpath=./build/",
        "-Wl,-rpath=./",
        "-O3", "-march=native", "-ffast-math",
        "-lm", "-ldl", "-flto=auto", "-lpthread");
    if (!cmd_run(&cmd)) return_defer(false);

    if (!procs_flush(&procs)) return_defer(false);
#else
    cmd_append(&cmd, "cc",
        "-Wall", "-Wextra", "-ggdb",
        "-I.",
        "-o", "./build/musializer",
        "./src/plug.c", "./src/ffmpeg_posix.c", "./src/webview.c", "./thirdparty/tinyfiledialogs.c",
        "-O3", "-march=native", "-ffast-math",
        "-lm", "-ldl", "-flto=auto", "-lpthread");
    if (!cmd_run(&cmd)) return_defer(false);
#endif // MUSIALIZER_HOTRELOAD

defer:
    cmd_free(cmd);
    da_free(procs);
    return result;
}

bool build_dist()
{
#ifdef MUSIALIZER_HOTRELOAD
    nob_log(ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    if (!mkdir_if_not_exists("./musializer-linux-x86_64/")) return false;
    if (!copy_file("./build/musializer", "./musializer-linux-x86_64/musializer")) return false;
    if (!copy_directory_recursively("./resources/", "./musializer-linux-x86_64/resources/")) return false;
    // TODO: should we pack ffmpeg with Linux build?
    // There are some static executables for Linux
    Cmd cmd = {0};
    cmd_append(&cmd, "tar", "fvcz", "./musializer-linux-x86_64.tar.gz", "./musializer-linux-x86_64");
    bool ok = cmd_run(&cmd);
    cmd_free(cmd);
    if (!ok) return false;

    return true;
#endif // MUSIALIZER_HOTRELOAD
}
