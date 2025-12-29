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
#include <direct.h>

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

bool copy_dir_recursive(const char *src, const char *dst) {
    Nob_File_Paths entries = {0};
    if (!read_entire_dir(src, &entries)) return false;
    
    for (size_t i = 0; i < entries.count; ++i) {
        const char *entry = entries.items[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        
        char src_path[1024];
        char dst_path[1024];
        sprintf(src_path, "%s/%s", src, entry);
        sprintf(dst_path, "%s/%s", dst, entry);
        
        if (is_dir(src_path)) {
            if (!mkdir_if_not_exists(dst_path)) {
                da_free(entries);
                return false;
            }
            if (!copy_dir_recursive(src_path, dst_path)) {
                da_free(entries);
                return false;
            }
        } else {
            if (!copy_file(src_path, dst_path)) {
                da_free(entries);
                return false;
            }
        }
    }
    da_free(entries);
    return true;
}

bool replace_all_in_file(const char *path, const char *old_str, const char *new_str) {
    String_Builder content = {0};
    if (!read_entire_file(path, &content)) return false;
    
    String_Builder new_content = {0};
    const char *search_start = content.items;
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    
    while (1) {
        const char *pos = strstr(search_start, old_str);
        if (!pos) break;
        // Copy everything before the match
        size_t prefix_len = pos - search_start;
        da_append_many(&new_content, search_start, prefix_len);
        // Replace the match
        da_append_many(&new_content, new_str, new_len);
        // Move past the old string
        search_start = pos + old_len;
    }
    
    // Copy remaining content
    da_append_many(&new_content, search_start, strlen(search_start));
    
    bool result = write_entire_file(path, new_content.items, new_content.count);
    da_free(content);
    da_free(new_content);
    return result;
}

bool mkdir_recursive(const char *path) {
    char temp[1024];
    strcpy(temp, path);
    for (char *p = temp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (!mkdir_if_not_exists(temp)) return false;
            *p = '/'; // restore for Windows, but since we're using /, it's fine
        }
    }
    return mkdir_if_not_exists(temp);
}

const char *get_package_name(void) {
    static char package_name[256];
    String_Builder manifest = {0};
    if (!read_entire_file("android/app/src/main/AndroidManifest.xml", &manifest)) {
        return NULL;
    }
    const char *package_attr = "package=\"";
    char *pos = strstr(manifest.items, package_attr);
    if (!pos) {
        da_free(manifest);
        return NULL;
    }
    pos += strlen(package_attr);
    char *end = strchr(pos, '"');
    if (!end) {
        da_free(manifest);
        return NULL;
    }
    size_t len = end - pos;
    if (len >= sizeof(package_name)) {
        da_free(manifest);
        return NULL;
    }
    memcpy(package_name, pos, len);
    package_name[len] = '\0';
    da_free(manifest);
    return package_name;
}

bool remove_dir_recursive(const char *path) {
    Nob_File_Paths entries = {0};
    if (!read_entire_dir(path, &entries)) return false;
    for (size_t i = 0; i < entries.count; ++i) {
        const char *entry = entries.items[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        char subpath[1024];
        sprintf(subpath, "%s/%s", path, entry);
        if (is_dir(subpath)) {
            if (!remove_dir_recursive(subpath)) {
                da_free(entries);
                return false;
            }
        } else {
            remove(subpath);
        }
    }
    da_free(entries);
    rmdir(path);
    return true;
}

bool validate_android_init(void) {
    if (file_exists("android/app/build.gradle.kts") != 1) {
        nob_log(ERROR, "Android project not initialized. Run 'nob android init' first.");
        return false;
    }
    if (file_exists("android/app/src/main/AndroidManifest.xml") != 1) {
        nob_log(ERROR, "Android manifest missing. Re-run 'nob android init'.");
        return false;
    }
    return true;
}

bool copy_template_with_replacements(const char *template_dir, const char *target_dir, 
                                   const char *package_name, const char *app_name) {
    // Copy the entire template directory
    if (!copy_dir_recursive(template_dir, target_dir)) return false;
    
    // Remove unnecessary Gradle cache directory
    char gradle_cache_dir[1024];
    sprintf(gradle_cache_dir, "%s/.gradle", target_dir);
    remove_dir_recursive(gradle_cache_dir);
    
    // Remove the copied java directory
    char old_java_dir[1024];
    sprintf(old_java_dir, "%s/app/src/main/java", target_dir);
    remove_dir_recursive(old_java_dir);
    
    // Define the package directory path
    char package_path[1024];
    strcpy(package_path, package_name);
    for (char *p = package_path; *p; ++p) {
        if (*p == '.') *p = '/';
    }
    
    //  the package directory structure
    char java_dir[1024];
    sprintf(java_dir, "%s/app/src/main/java/%s", target_dir, package_path);
    if (!mkdir_recursive(java_dir)) return false;
    
    // Copy Java/Kotlin files from template to new package, replacing placeholders
    char template_java_dir[1024];
    sprintf(template_java_dir, "%s/app/src/main/java/com/example/crossweb", template_dir);
    Nob_File_Paths files = {0};
    if (read_entire_dir(template_java_dir, &files)) {
        for (size_t i = 0; i < files.count; ++i) {
            const char *file = files.items[i];
            if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) continue;
            size_t len = strlen(file);
            if (len > 3 && (strcmp(file + len - 3, ".kt") == 0 || strcmp(file + len - 5, ".java") == 0)) {
                char template_file[1024];
                char new_file[1024];
                sprintf(template_file, "%s/%s", template_java_dir, file);
                sprintf(new_file, "%s/%s", java_dir, file);
                // Read template file, replace placeholders, write to new file
                String_Builder content = {0};
                if (!read_entire_file(template_file, &content)) {
                    da_free(files);
                    return false;
                }
                // Replace {{PACKAGE_NAME}}
                String_Builder replaced = {0};
                const char *search = content.items;
                const char *placeholder = "{{PACKAGE_NAME}}";
                size_t ph_len = strlen(placeholder);
                while (1) {
                    const char *pos = strstr(search, placeholder);
                    if (!pos) break;
                    size_t prefix_len = pos - search;
                    da_append_many(&replaced, search, prefix_len);
                    da_append_many(&replaced, package_name, strlen(package_name));
                    search = pos + ph_len;
                }
                da_append_many(&replaced, search, strlen(search));
                // Replace {{APP_NAME}} if app_name
                if (app_name) {
                    String_Builder final_content = {0};
                    search = replaced.items;
                    placeholder = "{{APP_NAME}}";
                    ph_len = strlen(placeholder);
                    while (1) {
                        const char *pos = strstr(search, placeholder);
                        if (!pos) break;
                        size_t prefix_len = pos - search;
                        da_append_many(&final_content, search, prefix_len);
                        da_append_many(&final_content, app_name, strlen(app_name));
                        search = pos + ph_len;
                    }
                    da_append_many(&final_content, search, strlen(search));
                    da_free(replaced);
                    replaced = final_content;
                }
                if (!write_entire_file(new_file, replaced.items, replaced.count)) {
                    da_free(replaced);
                    da_free(content);
                    da_free(files);
                    return false;
                }
                da_free(replaced);
                da_free(content);
            }
        }
        da_free(files);
    }
    
    // Replace placeholders in all relevant files
    const char *files_to_replace[] = {
        "app/build.gradle.kts",
        "app/src/main/AndroidManifest.xml",
        "app/src/main/res/layout/activity_main.xml",
        "app/src/main/res/values/strings.xml",
        "app/src/main/res/values/styles.xml",
        "app/proguard-rules.pro",
        NULL
    };
    
    for (size_t i = 0; files_to_replace[i] != NULL; ++i) {
        char file_path[1024];
        sprintf(file_path, "%s/%s", target_dir, files_to_replace[i]);
        if (file_exists(file_path) == 1) {
            if (!replace_all_in_file(file_path, "{{PACKAGE_NAME}}", package_name)) return false;
            if (app_name && !replace_all_in_file(file_path, "{{APP_NAME}}", app_name)) return false;
        }
    }
    
    return true;
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
