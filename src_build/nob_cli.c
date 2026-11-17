// Consolidated CLI logic moved out of top-level nob.c
// This file includes the src_build helper modules and exposes
// a single entry function `nob_cli_main` which the top-level
// `nob.c` will call. Do NOT define NOB_IMPLEMENTATION here.

#include "common.c"
#include "templates.c"
#include "plugins.h"
#include "plugins.c"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <direct.h>

int nob_cli_main(int argc, char **argv)
{
    const char *program = shift_args(&argc, &argv);

    for (int i = 0; i < argc;) {
        if (strcmp(argv[i], "-h=False") == 0 || strcmp(argv[i], "--hotreload=false") == 0) {
            cli_hotreload_enabled = false;
            memmove(&argv[i], &argv[i + 1], (size_t)(argc - i - 1) * sizeof(*argv));
            --argc;
        } else if (strcmp(argv[i], "-h=True") == 0 || strcmp(argv[i], "--hotreload=true") == 0) {
            cli_hotreload_enabled = true;
            memmove(&argv[i], &argv[i + 1], (size_t)(argc - i - 1) * sizeof(*argv));
            --argc;
        } else {
            ++i;
        }
    }
    cli_argc = argc;
    cli_argv = argv;

    nob_log(INFO, "--- STAGE 1 ---");

    if (!mkdir_if_not_exists("build")) return 1;

    if (argc > 0) {
        const char *command_name = shift(argv, argc);
        if (strcmp(command_name, "config") == 0) {
            nob_log(NOB_WARNING, "`nob config` is deprecated; build flags are now owned by the CLI.");
            // TODO: an ability to set the target through the `config` command
            while (argc > 0) {
                const char *flag_name = shift(argv, argc);
                bool found = false;
                for (size_t i = 0; !found && i < ARRAY_LEN(feature_flags); ++i) {
                    if (strcmp(feature_flags[i].name, flag_name) == 0) {
                        feature_flags[i].enabled_by_default = true;
                        found = true;
                    }
                }
                if (!found) {
                    nob_log(NOB_ERROR, "Unknown command `%s`", flag_name);
                    return 1;
                }
            }
            if (!generate_default_config(CONFIG_PATH)) return 1;
            return 0;
        } else if (strcmp(command_name, "init") == 0) {
            if (!mkdir_if_not_exists("web")) return 1;
            if (!run_npm_command("create vite@latest .", false)) return 1;
            return 0;
        } else if (strcmp(command_name, "win") == 0) {
            if (argc == 0) {
                nob_log(NOB_ERROR, "win command requires a subcommand");
                return 1;
            }
            const char *subcommand = shift_args(&argc, &argv);
            if (strcmp(subcommand, "dev") == 0) {
                int web_exists = file_exists("web");
                if (web_exists < 0) return 1;
                if (web_exists == 0) {
                    nob_log(NOB_ERROR, "web folder does not exist; run `nob init` first");
                    return 1;
                }
                // Start Vite dev server in background (Windows)
                if (!run_npm_command("run dev", true)) return 1;
                nob_log(NOB_INFO, "Vite dev server started");

                const char *stage2_binary = "build/nob_stage2";
                Cmd cmd = {0};
                cmd_append(&cmd, NOB_REBUILD_URSELF(stage2_binary, "./src_build/nob_stage2.c"), "-D", "CROSSWEB_TARGET_WIN64_GCC");
                append_cli_hotreload_define(&cmd);
                if (!cmd_run(&cmd)) return 1;
                cmd_free(cmd);

                cmd = (Cmd){0};
                cmd_append(&cmd, stage2_binary);
                if (!cmd_run(&cmd)) return 1;
                cmd_free(cmd);
                return 0;
            } else {
                nob_log(NOB_ERROR, "Unknown win subcommand `%s`", subcommand);
                return 1;
            }
        } else if (strcmp(command_name, "build") == 0) {
            int web_exists = file_exists("web");
            if (web_exists < 0) return 1;
            if (web_exists == 0) {
                nob_log(NOB_ERROR, "web folder does not exist; run `nob init` first");
                return 1;
            }
            // Run Vite build
            if (!run_npm_command("run build", false)) return 1;
            nob_log(NOB_INFO, "Web build finished");
            return 0;
        } else if (strcmp(command_name, "android") == 0) {
            if (argc == 0) {
                nob_log(NOB_ERROR, "android command requires a subcommand");
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
                            nob_log(NOB_ERROR, "Expected package name after --package");
                            return 1;
                        }
                        package_name = shift_args(&argc, &argv);
                    } else if (strcmp(arg, "--app-name") == 0 || strcmp(arg, "-n") == 0) {
                        shift_args(&argc, &argv);
                        if (argc == 0) {
                            nob_log(NOB_ERROR, "Expected app name after --app-name");
                            return 1;
                        }
                        app_name = shift_args(&argc, &argv);
                    } else {
                        nob_log(NOB_ERROR, "Unknown argument: %s", arg);
                        nob_log(NOB_ERROR, "Usage: nob android init [--package <package>] [--app-name <name>]");
                        return 1;
                    }
                }

                // Initialize Android project from template
                if (!mkdir_if_not_exists("android")) return 1;

                // Check if android project already exists
                int build_gradle_exists = file_exists("android/app/build.gradle.kts");
                if (build_gradle_exists < 0) return 1;
                if (build_gradle_exists == 1) {
                    nob_log(NOB_ERROR, "Android project already exists in ./android/");
                    return 1;
                }

                // Copy template with replacements
                if (!copy_template_with_replacements("templates/android", "android", package_name, app_name)) {
                    nob_log(NOB_ERROR, "Failed to copy Android template");
                    return 1;
                }

                nob_log(INFO, "Android project initialized in ./android/ with package '%s' and app name '%s'", package_name, app_name);
                return 0;
            } else if (strcmp(subcommand, "dev") == 0) {
                // This subcommand starts the Vite server, builds the Android debug APK wired to it,
                // then installs and launches that APK on the emulator.
                if (!validate_android_init()) return 1;
                const char *package_name = get_package_name();
                if (!package_name) {
                    nob_log(NOB_ERROR, "Could not determine package name from AndroidManifest.xml");
                    return 1;
                }

                // Start the local Vite server and use adb reverse so the device/emulator can reach it as localhost.
                if (!run_npm_command("run dev", true)) return 1;

                // Build the debug APK configured for the Vite dev server.
                Cmd cmd = {0};
                cmd_append(&cmd, NOB_REBUILD_URSELF("build/nob_stage2", "./src_build/nob_stage2.c"), "-D", "CROSSWEB_TARGET_ANDROID");
                append_cli_hotreload_define(&cmd);
                if (!cmd_run(&cmd)) return 1;
                cmd_free(cmd);

                cmd = (Cmd){0};
                cmd_append(&cmd, "build/nob_stage2");
                cmd_append(&cmd, "dev");
                if (!cmd_run(&cmd)) return 1;
                cmd_free(cmd);

                // Launch emulator
                if (!run_android_emulator("Medium_Phone_API_36", 10)) return 1;
                cmd = (Cmd){0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "reverse");
                cmd_append(&cmd, "tcp:5173");
                cmd_append(&cmd, "tcp:5173");
                if (!cmd_run(&cmd)) return 1;
                cmd_free(cmd);
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
                // Delegate Android build to stage2 compiled with CROSSWEB_TARGET_ANDROID.
                // `nob android build` produces the release APK using packaged assets.
                // `nob android build dev` produces the debug APK pointing at the local Vite server.
                if (argc > 1) {
                    nob_log(NOB_ERROR, "Usage: nob android build [dev]");
                    return 1;
                }
                if (argc == 1 && strcmp(argv[0], "dev") != 0) {
                    nob_log(NOB_ERROR, "Unknown android build mode `%s`", argv[0]);
                    nob_log(NOB_ERROR, "Usage: nob android build [dev]");
                    return 1;
                }
                const char *stage2_binary = "build/nob_stage2";
                Cmd cmd = {0};
                cmd_append(&cmd, NOB_REBUILD_URSELF(stage2_binary, "./src_build/nob_stage2.c"), "-D", "CROSSWEB_TARGET_ANDROID");
                append_cli_hotreload_define(&cmd);
                if (!cmd_run(&cmd)) return 1;
                // Run stage2 with the same argv
                cmd = (Cmd){0};
                cmd_append(&cmd, stage2_binary);
                da_append_many(&cmd, argv, argc);
                if (!cmd_run(&cmd)) return 1;
                return 0;
            } else if (strcmp(subcommand, "run") == 0) {
                // Build, install, and run either the release APK or the Vite-backed debug APK.
                if (argc > 1 || (argc == 1 && strcmp(argv[0], "dev") != 0)) {
                    nob_log(NOB_ERROR, "Usage: nob android run [dev]");
                    return 1;
                }
                bool dev_mode = argc == 1;
                if (!validate_android_init()) return 1;
                const char *package_name = get_package_name();
                if (!package_name) {
                    nob_log(NOB_ERROR, "Could not determine package name from AndroidManifest.xml");
                    return 1;
                }

                const char *stage2_binary = "build/nob_stage2";
                Cmd build_cmd = {0};
                cmd_append(&build_cmd, NOB_REBUILD_URSELF(stage2_binary, "./src_build/nob_stage2.c"), "-D", "CROSSWEB_TARGET_ANDROID");
                append_cli_hotreload_define(&build_cmd);
                if (!cmd_run(&build_cmd)) return 1;
                cmd_free(build_cmd);

                if (dev_mode && !run_npm_command("run dev", true)) return 1;
                Cmd stage2_cmd = {0};
                cmd_append(&stage2_cmd, stage2_binary);
                if (dev_mode) cmd_append(&stage2_cmd, "dev");
                if (!cmd_run(&stage2_cmd)) return 1;
                cmd_free(stage2_cmd);

                // Launch emulator
                if (!run_android_emulator("Medium_Phone_API_36", 30)) return 1;
                if (dev_mode) {
                    Cmd reverse_cmd = {0};
                    cmd_append(&reverse_cmd, "adb", "reverse", "tcp:5173", "tcp:5173");
                    if (!cmd_run(&reverse_cmd)) return 1;
                    cmd_free(reverse_cmd);
                }

                // Install and run the selected APK.
                Cmd cmd = {0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "install");
                cmd_append(&cmd, "-r");
                cmd_append(&cmd, dev_mode
                    ? "android/app/build/outputs/apk/debug/app-debug.apk"
                    : "android/app/build/outputs/apk/release/app-release.apk");
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
                // Installs the release APK on a connected device or emulator. 
                // This is a convenience command that assumes the APK has already been built.
                Cmd cmd = {0};
                cmd_append(&cmd, "adb");
                cmd_append(&cmd, "install");
                cmd_append(&cmd, "-r");
                cmd_append(&cmd, "android/app/build/outputs/apk/release/app-release.apk");
                if (!cmd_run(&cmd)) return 1;
                nob_log(INFO, "APK installed");
                return 0;
            } else {
                nob_log(NOB_ERROR, "Unknown android subcommand `%s`", subcommand);
                return 1;
            }
        } else if (strcmp(command_name, "help") == 0) {
            nob_log(INFO, "Usage: %s [command]", program);
            nob_log(INFO, "Commands:");
            nob_log(INFO, "    config");
            nob_log(INFO, "    init");
            nob_log(INFO, "    win <dev>");
            nob_log(INFO, "    build");
            nob_log(INFO, "    android <init|dev|build [dev]|run|install>");
            nob_log(INFO, "    help");
            return 0;
        } else {
            nob_log(NOB_ERROR, "Unknown command `%s`", command_name);
            return 1;
        }
    }

    // No explicit subcommand was given — indicate to the caller that
    // it should proceed with the stage2/bootstrap flow. We return 2
    // as a sentinel value (0 = handled, 1 = error).
    return 2;
}
