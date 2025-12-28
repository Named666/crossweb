#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
// #define NOB_EXPERIMENTAL_DELETE_OLD // Doesn't work on Windows
// #define NOB_WARN_DEPRECATED
#include "./thirdparty/nob.h"
#include "./src_build/configurer.c"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

bool is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool is_plugin_enabled(const char *plugin_name) {
    String_Builder content = {0};
    if (!read_entire_file("build/config.h", &content)) return true; // if not found, assume enabled
    char macro[256];
    sprintf(macro, "#define CROSSWEB_PLUGIN_%s 1", plugin_name);
    for (size_t j = strlen("#define CROSSWEB_PLUGIN_"); j < strlen(macro); ++j) {
        macro[j] = toupper(macro[j]);
    }
    bool enabled = strstr(content.items, macro) != NULL;
    da_free(content);
    return enabled;
}

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
                // Skip mobile.c and desktop.c in plugins/fs since they are included by lib.c
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

bool replace_in_file(const char *path, const char *old_str, const char *new_str) {
    String_Builder content = {0};
    if (!read_entire_file(path, &content)) return false;
    char *pos = strstr(content.items, old_str);
    if (!pos) return true; // not found, assume already correct
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    String_Builder new_content = {0};
    size_t prefix_len = pos - content.items;
    da_append_many(&new_content, content.items, prefix_len);
    da_append_many(&new_content, new_str, new_len);
    da_append_many(&new_content, pos + old_len, content.count - prefix_len - old_len);
    bool result = write_entire_file(path, new_content.items, new_content.count);
    da_free(content);
    da_free(new_content);
    return result;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "./thirdparty/nob.h", "./src_build/configurer.c");

    const char *program = shift_args(&argc, &argv);

    const char *old_build_conf_path = "./build/build.conf";
    int build_conf_exists = file_exists(old_build_conf_path);
    if (build_conf_exists < 0) return 1;
    if (build_conf_exists) {
        // @backcomp
        nob_log(ERROR, "We found %s. That means your build folder has an old schema.", old_build_conf_path);
        nob_log(ERROR, "Instead of %s you are suppose to use %s to configure the build now.", old_build_conf_path, CONFIG_PATH);
        nob_log(ERROR, "Remove your ./build/ folder and run %s again to regenerate the folder with the new schema.", program);
        return 1;
    }

    nob_log(INFO, "--- STAGE 1 ---");

    if (!mkdir_if_not_exists("build")) return 1;

    if (argc > 0) {
        const char *command_name = shift(argv, argc);
        if (strcmp(command_name, "config") == 0) {
            // TODO: an ability to set the target through the `config` command
            while (argc > 0) {
                const char *flag_name = shift(argv, argc);
                bool found = false;
                for (size_t i = 0; !found && i < ARRAY_LEN(feature_flags); ++i) {
                    // TODO: an ability to disable flags that enabled by default
                    //   We don't have such flags yet, but maybe we will at some point?
                    if (strcmp(feature_flags[i].name, flag_name) == 0) {
                        feature_flags[i].enabled_by_default = true;
                        found = true;
                    }
                }
                if (!found) {
                    nob_log(ERROR, "Unknown command `%s`", flag_name);
                    return 1;
                }
            }
            if (!generate_default_config(CONFIG_PATH)) return 1;
            return 0;
        } else if (strcmp(command_name, "init") == 0) {
            if (!mkdir_if_not_exists("web")) return 1;
            Cmd cmd = {0};
            cmd_append(&cmd, "cmd");
            cmd_append(&cmd, "/c");
            cmd_append(&cmd, "cd");
            cmd_append(&cmd, "web");
            cmd_append(&cmd, "&&");
            cmd_append(&cmd, "npm");
            cmd_append(&cmd, "create");
            cmd_append(&cmd, "vite@latest");
            cmd_append(&cmd, ".");
            if (!cmd_run(&cmd)) return 1;
            return 0;
        } else if (strcmp(command_name, "android") == 0) {
            if (argc == 0) {
                nob_log(ERROR, "android command requires a subcommand");
                return 1;
            }
            const char *subcommand = shift_args(&argc, &argv);
            if (strcmp(subcommand, "init") == 0) {
                // Initialize Android project
                if (!mkdir_if_not_exists("android")) return 1;
                int build_gradle_exists = file_exists("android/app/build.gradle");
                if (build_gradle_exists < 0) return 1;
                if (build_gradle_exists == 0) {
                    // Create basic gradle structure
                    Cmd cmd = {0};
                    cmd_append(&cmd, "cmd");
                    cmd_append(&cmd, "/c");
                    cmd_append(&cmd, "cd");
                    cmd_append(&cmd, "android");
                    cmd_append(&cmd, "&&");
                    cmd_append(&cmd, "gradle");
                    cmd_append(&cmd, "init");
                    cmd_append(&cmd, "--type");
                    cmd_append(&cmd, "basic");
                    if (!cmd_run(&cmd)) return 1;
                }
                // TODO: Add more setup for Android WebView project
                nob_log(INFO, "Android project initialized in ./android/");
                return 0;
            } else if (strcmp(subcommand, "dev") == 0) {
                // Start Vite dev server in background
                Cmd cmd = {0};
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "start");
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "cd");
                cmd_append(&cmd, "web");
                cmd_append(&cmd, "&&");
                cmd_append(&cmd, "npm");
                cmd_append(&cmd, "run");
                cmd_append(&cmd, "dev");
                if (!cmd_run(&cmd)) return 1;
                // Modify MainActivity for dev
                if (!replace_in_file("android/app/src/main/java/com/example/crossweb/MainActivity.kt", "webView.loadUrl(\"https://www.google.com\")", "webView.loadUrl(\"http://10.0.2.2:5173\")")) return 1;
                // Build debug APK
                cmd = (Cmd){0};
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "cd");
                cmd_append(&cmd, "android");
                cmd_append(&cmd, "&&");
                cmd_append(&cmd, "gradle");
                cmd_append(&cmd, "assembleDebug");
                if (!cmd_run(&cmd)) return 1;
                // Launch emulator (assume AVD exists)
                cmd = (Cmd){0};
                const char *android_home = getenv("ANDROID_HOME");
                if (!android_home) {
                    nob_log(ERROR, "ANDROID_HOME environment variable is not set");
                    return 1;
                }
                char emulator_path[1024];
                sprintf(emulator_path, "%s\\emulator\\emulator.exe", android_home);
                cmd_append(&cmd, emulator_path);
                cmd_append(&cmd, "-avd");
                cmd_append(&cmd, "Medium_Phone_API_36");
                if (!cmd_run(&cmd)) return 1; // run in background? but sync
                // Wait for emulator
                cmd = (Cmd){0};
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "timeout");
                cmd_append(&cmd, "10");
                cmd_run(&cmd);
                // Install and run
                cmd = (Cmd){0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "install");
                cmd_append(&cmd, "-r");
                cmd_append(&cmd, "android/app/build/outputs/apk/debug/app-debug.apk");
                if (!cmd_run(&cmd)) return 1;
                cmd = (Cmd){0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "shell");
                cmd_append(&cmd, "am");
                cmd_append(&cmd, "start");
                cmd_append(&cmd, "-n");
                cmd_append(&cmd, "com.example.crossweb/.MainActivity");
                if (!cmd_run(&cmd)) return 1;
                nob_log(INFO, "Dev mode launched");
                return 0;
            } else if (strcmp(subcommand, "build") == 0) {
                // Build web assets
                Cmd cmd = {0};
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "cd");
                cmd_append(&cmd, "web");
                cmd_append(&cmd, "&&");
                cmd_append(&cmd, "npm");
                cmd_append(&cmd, "run");
                cmd_append(&cmd, "build");
                if (!cmd_run(&cmd)) return 1;
                // Copy to assets
                if (!mkdir_if_not_exists("android/app/src/main/assets")) return 1;
                cmd = (Cmd){0};
                cmd_append(&cmd, "xcopy");
                cmd_append(&cmd, "web\\dist\\*");
                cmd_append(&cmd, "android\\app\\src\\main\\assets\\");
                cmd_append(&cmd, "/s");
                cmd_append(&cmd, "/y");
                if (!cmd_run(&cmd)) return 1;
                // Modify MainActivity for local
                if (!replace_in_file("android/app/src/main/java/com/example/crossweb/MainActivity.kt", "webView.loadUrl(\"http://10.0.2.2:5173\")", "webView.loadUrl(\"file:///android_asset/index.html\")")) return 1;
                // Copy C source files to Android c dir
                if (!mkdir_if_not_exists("android/app/src/main/c")) return 1;
                if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return 1;
                if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return 1;
                if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return 1;
                if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return 1;
                if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return 1;
                if (!copy_file("src/config.h", "android/app/src/main/c/config.h")) return 1;
                // Copy plugin files
                if (!mkdir_if_not_exists("android/app/src/main/c/plugins")) return 1;
                if (!mkdir_if_not_exists("android/app/src/main/c/plugins/fs")) return 1;
                if (!copy_file("src/plugins/fs/lib.c", "android/app/src/main/c/plugins/fs/lib.c")) return 1;
                if (!copy_file("src/plugins/fs/commands.c", "android/app/src/main/c/plugins/fs/commands.c")) return 1;
                if (!copy_file("src/plugins/fs/commands.h", "android/app/src/main/c/plugins/fs/commands.h")) return 1;
                if (!copy_file("src/plugins/fs/mobile.c", "android/app/src/main/c/plugins/fs/mobile.c")) return 1;
                if (!copy_file("src/plugins/fs/error.c", "android/app/src/main/c/plugins/fs/error.c")) return 1;
                if (!copy_file("src/plugins/fs/error.h", "android/app/src/main/c/plugins/fs/error.h")) return 1;
                if (!copy_file("src/plugins/fs/models.h", "android/app/src/main/c/plugins/fs/models.h")) return 1;
                if (!copy_file("src/plugins/fs/plugin.h", "android/app/src/main/c/plugins/fs/plugin.h")) return 1;
                // Copy keystore plugin
                if (!mkdir_if_not_exists("android/app/src/main/c/plugins/keystore")) return 1;
                if (!mkdir_if_not_exists("android/app/src/main/c/plugins/keystore/src")) return 1;
                if (!copy_file("src/plugins/keystore/src/plugin.c", "android/app/src/main/c/plugins/keystore/src/plugin.c")) return 1;
                if (!copy_file("src/plugins/keystore/src/plugin.h", "android/app/src/main/c/plugins/keystore/src/plugin.h")) return 1;
                // Generate CMakeLists.txt
                if (!generate_cmake_lists("android/app/src/main/c/CMakeLists.txt")) return 1;
                // Build release
                cmd = (Cmd){0};
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "cd");
                cmd_append(&cmd, "android");
                cmd_append(&cmd, "&&");
                cmd_append(&cmd, "gradle");
                cmd_append(&cmd, "assembleRelease");
                cmd_append(&cmd, "--stacktrace");
                if (!cmd_run(&cmd)) return 1;
                nob_log(INFO, "APK built in android/app/build/outputs/apk/release/");
                return 0;
            } else if (strcmp(subcommand, "run") == 0) {
                // Launch emulator if not running
                Cmd cmd = {0};
                const char *android_home = getenv("ANDROID_HOME");
                if (!android_home) {
                    nob_log(ERROR, "ANDROID_HOME environment variable is not set");
                    return 1;
                }
                char emulator_path[1024];
                sprintf(emulator_path, "%s\\emulator\\emulator.exe", android_home);
                cmd_append(&cmd, emulator_path);
                cmd_append(&cmd, "-avd");
                cmd_append(&cmd, "Medium_Phone_API_36");
                cmd_append(&cmd, "-no-snapshot");
                cmd_append(&cmd, "-gpu");
                cmd_append(&cmd, "off");
                if (!cmd_run(&cmd)) return 1;
                // Wait for emulator
                cmd = (Cmd){0};
                cmd_append(&cmd, "cmd");
                cmd_append(&cmd, "/c");
                cmd_append(&cmd, "timeout");
                cmd_append(&cmd, "30");
                cmd_run(&cmd);
                // Install and run release APK
                cmd = (Cmd){0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "install");
                cmd_append(&cmd, "-r");
                cmd_append(&cmd, "android/app/build/outputs/apk/release/app-release.apk");
                if (!cmd_run(&cmd)) return 1;
                cmd = (Cmd){0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "shell");
                cmd_append(&cmd, "am");
                cmd_append(&cmd, "start");
                cmd_append(&cmd, "-n");
                cmd_append(&cmd, "com.example.crossweb/.MainActivity");
                if (!cmd_run(&cmd)) return 1;
                nob_log(INFO, "App launched");
                return 0;
            } else if (strcmp(subcommand, "install") == 0) {
                Cmd cmd = {0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "install");
                cmd_append(&cmd, "-r");
                cmd_append(&cmd, "android/app/build/outputs/apk/release/app-release.apk");
                if (!cmd_run(&cmd)) return 1;
                nob_log(INFO, "APK installed");
                return 0;
            } else {
                nob_log(ERROR, "Unknown android subcommand `%s`", subcommand);
                return 1;
            }
        } else {
            nob_log(ERROR, "Unknown command `%s`", command_name);
            return 1;
        }
    }

    int config_exists = file_exists(CONFIG_PATH);
    if (config_exists < 0) return 1;
    if (config_exists == 0) {
        if (!generate_default_config(CONFIG_PATH)) return 1;
    } else {
        nob_log(INFO, "file `%s` already exists", CONFIG_PATH);
    }

    if (!generate_config_logger("build/config_logger.c")) return 1;

    Cmd cmd = {0};
    const char *stage2_binary = "build/nob_stage2";
    cmd_append(&cmd, NOB_REBUILD_URSELF(stage2_binary, "./src_build/nob_stage2.c"));
    if (!cmd_run(&cmd)) return 1;

    cmd_append(&cmd, stage2_binary);
    da_append_many(&cmd, argv, argc);
    if (!cmd_run(&cmd)) return 1;

    return 0;
}
