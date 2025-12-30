#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
// #define NOB_EXPERIMENTAL_DELETE_OLD // Doesn't work on Windows
// #define NOB_WARN_DEPRECATED
#include "./thirdparty/nob.h"
#include "./src_build/configurer.c"
#include "./src_build/common.c"
#include "./src_build/config.c"
#include "./src_build/templates.c"
#include "./src_build/plugins.h"
#include "./src_build/android_build.h"
#include "./src_build/plugins.c"
#include "./src_build/android_build.c"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <direct.h>

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
                // Parse command line arguments
                const char *package_name = "com.example.crossweb";
                const char *app_name = "Crossweb";
                
                while (argc > 0) {
                    const char *arg = argv[0];
                    if (strcmp(arg, "--package") == 0 || strcmp(arg, "-p") == 0) {
                        shift_args(&argc, &argv);
                        if (argc == 0) {
                            nob_log(ERROR, "Expected package name after --package");
                            return 1;
                        }
                        package_name = shift_args(&argc, &argv);
                    } else if (strcmp(arg, "--app-name") == 0 || strcmp(arg, "-n") == 0) {
                        shift_args(&argc, &argv);
                        if (argc == 0) {
                            nob_log(ERROR, "Expected app name after --app-name");
                            return 1;
                        }
                        app_name = shift_args(&argc, &argv);
                    } else {
                        nob_log(ERROR, "Unknown argument: %s", arg);
                        nob_log(ERROR, "Usage: nob android init [--package <package>] [--app-name <name>]");
                        return 1;
                    }
                }
                
                // Initialize Android project from template
                if (!mkdir_if_not_exists("android")) return 1;
                
                // Check if android project already exists
                int build_gradle_exists = file_exists("android/app/build.gradle.kts");
                if (build_gradle_exists < 0) return 1;
                if (build_gradle_exists == 1) {
                    nob_log(ERROR, "Android project already exists in ./android/");
                    return 1;
                }
                
                // Copy template with replacements
                if (!copy_template_with_replacements("templates/android", "android", package_name, app_name)) {
                    nob_log(ERROR, "Failed to copy Android template");
                    return 1;
                }
                
                nob_log(INFO, "Android project initialized in ./android/ with package '%s' and app name '%s'", package_name, app_name);
                return 0;
            } else if (strcmp(subcommand, "dev") == 0) {
                if (!validate_android_init()) return 1;
                const char *package_name = get_package_name();
                if (!package_name) {
                    nob_log(ERROR, "Could not determine package name from AndroidManifest.xml");
                    return 1;
                }
                // Convert package to path
                char package_path[1024];
                strcpy(package_path, package_name);
                for (char *p = package_path; *p; ++p) {
                    if (*p == '.') *p = '/';
                }
                char main_activity_path[1024];
                sprintf(main_activity_path, "android/app/src/main/java/%s/MainActivity.kt", package_path);
                
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
                if (!replace_all_in_file(main_activity_path, "webView.loadUrl(\"https://www.google.com\")", "webView.loadUrl(\"http://10.0.2.2:5173\")")) return 1;
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
                char activity_name[1024];
                sprintf(activity_name, "%s/.MainActivity", package_name);
                cmd_append(&cmd, activity_name);
                if (!cmd_run(&cmd)) return 1;
                nob_log(INFO, "Dev mode launched");
                return 0;
            } else if (strcmp(subcommand, "build") == 0) {
                if (!validate_android_init()) return 1;
                const char *package_name = get_package_name();
                if (!package_name) {
                    nob_log(ERROR, "Could not determine package name from AndroidManifest.xml");
                    return 1;
                }
                // Convert package to path
                char package_path[1024];
                strcpy(package_path, package_name);
                for (char *p = package_path; *p; ++p) {
                    if (*p == '.') *p = '/';
                }
                char main_activity_path[1024];
                sprintf(main_activity_path, "android/app/src/main/java/%s/MainActivity.kt", package_path);
                
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
                if (!replace_all_in_file(main_activity_path, "webView.loadUrl(\"http://10.0.2.2:5173\")", "webView.loadUrl(\"file:///android_asset/index.html\")")) return 1;
                // Copy C source files to Android c dir
                if (!mkdir_if_not_exists("android/app/src/main/c")) return 1;
                if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return 1;
                // Replace JNI package in android_bridge.c
                if (!replace_package_placeholders_in_file("android/app/src/main/c/android_bridge.c", package_name)) return 1;
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
                // Replace package in keystore plugin
                if (!replace_package_placeholders_in_file("android/app/src/main/c/plugins/keystore/src/plugin.c", package_name)) return 1;
                if (!copy_file("src/plugins/keystore/src/plugin.h", "android/app/src/main/c/plugins/keystore/src/plugin.h")) return 1;
                // Generate CMakeLists.txt
                if (!generate_cmake_lists("android/app/src/main/c/CMakeLists.txt")) return 1;
                // Generate ProGuard rules
                if (!generate_proguard_rules(package_name)) return 1;
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
                if (!validate_android_init()) return 1;
                const char *package_name = get_package_name();
                if (!package_name) {
                    nob_log(ERROR, "Could not determine package name from AndroidManifest.xml");
                    return 1;
                }
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
                char activity_name[1024];
                sprintf(activity_name, "%s/.MainActivity", package_name);
                cmd_append(&cmd, activity_name);
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
