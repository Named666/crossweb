#include "android_build.h"
#include "config.h"
#include "plugins.h"
#include "common.h"
#include <string.h>

bool collect_c_files(const char *dir, Nob_File_Paths *files, const char *base) {
    Nob_File_Paths entries = {0};
    if (!read_entire_dir(dir, &entries)) return false;
    for (size_t i = 0; i < entries.count; ++i) {
        const char *entry = entries.items[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        char path[1024];
        sprintf(path, "%s/%s", dir, entry);
        if (is_dir(path)) {
            if (!collect_c_files(path, files, base)) return false;
        } else {
            size_t len = strlen(entry);
            if (len > 2 && strcmp(entry + len - 2, ".c") == 0) {
                if ((strcmp(entry, "mobile.c") == 0 || strcmp(entry, "desktop.c") == 0) && strstr(dir, "plugins/fs") != NULL) continue;
                char rel_path[1024];
                size_t base_len = strlen(base);
                if (strlen(dir) == base_len) {
                    sprintf(rel_path, "%s", entry);
                } else {
                    sprintf(rel_path, "%s/%s", dir + base_len + 1, entry);
                }
                da_append(files, nob_temp_strdup(rel_path));
            }
        }
    }
    da_free(entries);
    return true;
}

bool generate_cmake_lists(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "cmake_minimum_required(VERSION 3.4.1)\n");
    fprintf(f, "project(crossweb C)\n\n");
    fprintf(f, "include_directories(${CMAKE_CURRENT_SOURCE_DIR})\n\n");
    fprintf(f, "# Add your native source files here\n");
    fprintf(f, "add_library(\n");
    fprintf(f, "\tcrossweb\n");
    fprintf(f, "\tSHARED\n");
    Nob_File_Paths c_files = {0};
    if (!collect_c_files("android/app/src/main/c", &c_files, "android/app/src/main/c")) {
        fclose(f);
        return false;
    }
    for (size_t i = 0; i < c_files.count; ++i) {
        fprintf(f, "\t%s\n", c_files.items[i]);
    }
    da_free(c_files);
    fprintf(f, ")\n\n");
    fprintf(f, "# Link against Android log library\n");
    fprintf(f, "find_library(\n");
    fprintf(f, "\tlog-lib\n");
    fprintf(f, "\tlog\n");
    fprintf(f, ")\n");
    fprintf(f, "target_link_libraries(\n");
    fprintf(f, "\tcrossweb\n");
    fprintf(f, "\t${log-lib}\n");
    fprintf(f, ")\n");
    fclose(f);
    return true;
}

bool android_build_crossweb(void) {
    const char *package_name = get_package_name();
    if (!package_name) package_name = "__PACKAGE__";
    Nob_Cmd build_cmd = {0};
    nob_cmd_append(&build_cmd, "cd");
    nob_cmd_append(&build_cmd, "web");
    nob_cmd_append(&build_cmd, "&&");
    nob_cmd_append(&build_cmd, "npm");
    nob_cmd_append(&build_cmd, "run");
    nob_cmd_append(&build_cmd, "build");
    if (!nob_cmd_run_sync(build_cmd)) return false;
    nob_cmd_free(build_cmd);
    if (!mkdir_if_not_exists("android/app/src/main/c")) return false;
    if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return false;
    char mangled[256], slashed[256];
    mangle_package_name(mangled, package_name);
    slash_package_name(slashed, package_name);
    if (!replace_all_in_file("android/app/src/main/c/android_bridge.c", "__PACKAGE_MANGLED__", mangled)) return false;
    if (!replace_all_in_file("android/app/src/main/c/android_bridge.c", "__PACKAGE_PATH__", slashed)) return false;
    if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return false;
    if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return false;
    if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return false;
    if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return false;
    if (!copy_file("src/config.h", "android/app/src/main/c/config.h")) return false;
    if (!copy_plugins_for_android()) return false;
    if (!generate_proguard_rules(package_name)) return false;
    if (!generate_cmake_lists("android/app/src/main/c/CMakeLists.txt")) return false;
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

bool android_build_dist(void) {
    const char *package_name = get_package_name();
    if (!package_name) package_name = "com.example.crossweb";
    Nob_Cmd build_cmd = {0};
    nob_cmd_append(&build_cmd, "cd");
    nob_cmd_append(&build_cmd, "web");
    nob_cmd_append(&build_cmd, "&&");
    nob_cmd_append(&build_cmd, "npm");
    nob_cmd_append(&build_cmd, "run");
    nob_cmd_append(&build_cmd, "build");
    if (!nob_cmd_run_sync(build_cmd)) return false;
    nob_cmd_free(build_cmd);
    if (!mkdir_if_not_exists("android/app/src/main/c")) return false;
    if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return false;
    char mangled[256], slashed[256];
    mangle_package_name(mangled, package_name);
    slash_package_name(slashed, package_name);
    if (!replace_all_in_file("android/app/src/main/c/android_bridge.c", "__PACKAGE_MANGLED__", mangled)) return false;
    if (!replace_all_in_file("android/app/src/main/c/android_bridge.c", "__PACKAGE_PATH__", slashed)) return false;
    if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return false;
    if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return false;
    if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return false;
    if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return false;
    if (!copy_file("src/hotreload.h", "android/app/src/main/c/hotreload.h")) return false;
    if (!copy_file("src/hotreload_posix.c", "android/app/src/main/c/hotreload_posix.c")) return false;
    if (!copy_file("src/config.h", "android/app/src/main/c/config.h")) return false;
    if (!copy_plugins_for_android()) return false;
    if (!generate_proguard_rules(package_name)) return false;
    if (!generate_cmake_lists("android/app/src/main/c/CMakeLists.txt")) return false;
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