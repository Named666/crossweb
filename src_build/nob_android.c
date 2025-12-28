#define CROSSWEB_TARGET_NAME "android"

bool build_crossweb(void)
{
    // Build web assets
    Nob_Cmd build_cmd = {0};
    nob_cmd_append(&build_cmd, "cd");
    nob_cmd_append(&build_cmd, "web");
    nob_cmd_append(&build_cmd, "&&");
    nob_cmd_append(&build_cmd, "npm");
    nob_cmd_append(&build_cmd, "run");
    nob_cmd_append(&build_cmd, "build");
    if (!nob_cmd_run_sync(build_cmd)) return false;
    nob_cmd_free(build_cmd);

    // Copy C source files to Android c dir
    if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return false;
    if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return false;
    if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return false;
    if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return false;
    if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return false;

    // Create assets dir and copy web files
    if (!mkdir_if_not_exists("android/app/src/main/assets")) return false;
    Nob_Cmd copy_cmd = {0};
    nob_cmd_append(&copy_cmd, "xcopy");
    nob_cmd_append(&copy_cmd, "/E");
    nob_cmd_append(&copy_cmd, "/I");
    nob_cmd_append(&copy_cmd, "/Y");
    nob_cmd_append(&copy_cmd, "web\\dist");
    nob_cmd_append(&copy_cmd, "android\\app\\src\\main\\assets\\");
    if (!nob_cmd_run_sync(copy_cmd)) return false;
    nob_cmd_free(copy_cmd);

    // For Android, assume gradle is available
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cd");
    nob_cmd_append(&cmd, "android");
    nob_cmd_append(&cmd, "&&");
    nob_cmd_append(&cmd, "gradle");
    nob_cmd_append(&cmd, "assembleDebug");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return result;
}

bool build_dist(void)
{
    // Build web assets
    Nob_Cmd build_cmd = {0};
    nob_cmd_append(&build_cmd, "cd");
    nob_cmd_append(&build_cmd, "web");
    nob_cmd_append(&build_cmd, "&&");
    nob_cmd_append(&build_cmd, "npm");
    nob_cmd_append(&build_cmd, "run");
    nob_cmd_append(&build_cmd, "build");
    if (!nob_cmd_run_sync(build_cmd)) return false;
    nob_cmd_free(build_cmd);

    // Copy C source files to Android c dir
    if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return false;
    if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return false;
    if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return false;
    if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return false;
    if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return false;
    if (!copy_file("src/hotreload.h", "android/app/src/main/c/hotreload.h")) return false;
    if (!copy_file("src/hotreload_posix.c", "android/app/src/main/c/hotreload_posix.c")) return false;

    // Create assets dir and copy web files
    if (!mkdir_if_not_exists("android/app/src/main/assets")) return false;
    Nob_Cmd copy_cmd = {0};
    nob_cmd_append(&copy_cmd, "xcopy");
    nob_cmd_append(&copy_cmd, "/E");
    nob_cmd_append(&copy_cmd, "/I");
    nob_cmd_append(&copy_cmd, "/Y");
    nob_cmd_append(&copy_cmd, "web\\dist");
    nob_cmd_append(&copy_cmd, "android\\app\\src\\main\\assets\\");
    if (!nob_cmd_run_sync(copy_cmd)) return false;
    nob_cmd_free(copy_cmd);

    // Build APK
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cd");
    nob_cmd_append(&cmd, "android");
    nob_cmd_append(&cmd, "&&");
    nob_cmd_append(&cmd, "gradle");
    nob_cmd_append(&cmd, "bundleRelease");
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    if (result) {
        nob_log(NOB_INFO, "AAB built in android/app/build/outputs/bundle/release/");
    }
    return result;
}
