#define CROSSWEB_TARGET_NAME "android"

#include <ctype.h>

const char *get_package_name_android(void) {
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
    if (!nob_read_entire_dir(dir, &entries)) return false;
    for (size_t i = 0; i < entries.count; ++i) {
        const char *entry = entries.items[i];
        char path[1024];
        sprintf(path, "%s/%s", dir, entry);
        if (nob_is_dir(path)) {
            if (!collect_c_files(path, files, base)) {
                da_free(entries);
                return false;
            }
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

bool copy_plugins_for_android(void) {
    const char *package_name = get_package_name_android();
    if (!package_name) package_name = "com.example.crossweb"; // fallback
    
    // Convert package to path
    char package_path[1024];
    strcpy(package_path, package_name);
    for (char *p = package_path; *p; ++p) {
        if (*p == '.') *p = '/';
    }
    
    if (!mkdir_if_not_exists("android/app/src/main/c/plugins")) return false;
    Nob_File_Paths plugins = {0};
    if (!nob_read_entire_dir("src/plugins", &plugins)) return false;
    for (size_t i = 0; i < plugins.count; ++i) {
        const char *plugin_name = plugins.items[i];
        if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0) continue;
        if (!is_plugin_enabled(plugin_name)) continue;
        char plugin_c_dir[1024];
        sprintf(plugin_c_dir, "android/app/src/main/c/plugins/%s", plugin_name);
        if (!mkdir_if_not_exists(plugin_c_dir)) return false;
        // Scan src/ folder for all files
        Nob_File_Paths src_files = {0};
        char src_dir[1024];
        sprintf(src_dir, "src/plugins/%s/src", plugin_name);
        if (!nob_read_entire_dir(src_dir, &src_files)) {
            // Fallback to plugin root directory
            sprintf(src_dir, "src/plugins/%s", plugin_name);
            if (!nob_read_entire_dir(src_dir, &src_files)) {
                // No source files found, skip
                continue;
            }
        }
            for (size_t j = 0; j < src_files.count; ++j) {
                const char *file = src_files.items[j];
                char src[1024];
                sprintf(src, "%s/%s", src_dir, file);
                char dst[1024];
                sprintf(dst, "%s/%s", plugin_c_dir, file);
                if (!copy_file(src, dst)) {
                    da_free(src_files);
                    da_free(plugins);
                    return false;
                }
            }
            da_free(src_files);
        }
        // For android/
        char android_src[1024];
        sprintf(android_src, "src/plugins/%s/android", plugin_name);
        if (file_exists(android_src) == 1) {
            Nob_Cmd copy_cmd = {0};
            nob_cmd_append(&copy_cmd, "xcopy");
            nob_cmd_append(&copy_cmd, "/E");
            nob_cmd_append(&copy_cmd, "/I");
            nob_cmd_append(&copy_cmd, "/Y");
            char android_src_star[1024];
            sprintf(android_src_star, "%s\\*", android_src);
            nob_cmd_append(&copy_cmd, android_src_star);
            char dest_dir[1024];
            sprintf(dest_dir, "android\\app\\src\\main\\java\\%s", package_path);
            nob_cmd_append(&copy_cmd, dest_dir);
            if (!nob_cmd_run_sync(copy_cmd)) {
                nob_cmd_free(copy_cmd);
                // Continue, maybe no files
            } else {
                nob_cmd_free(copy_cmd);
            }
        }
        // For guest-js/
        char guest_js_src[1024];
        sprintf(guest_js_src, "src/plugins/%s/guest-js", plugin_name);
        if (file_exists(guest_js_src) == 1) {
            Nob_Cmd copy_cmd = {0};
            nob_cmd_append(&copy_cmd, "xcopy");
            nob_cmd_append(&copy_cmd, "/E");
            nob_cmd_append(&copy_cmd, "/I");
            nob_cmd_append(&copy_cmd, "/Y");
            char guest_js_src_star[1024];
            sprintf(guest_js_src_star, "%s\\*", guest_js_src);
            nob_cmd_append(&copy_cmd, guest_js_src_star);
            nob_cmd_append(&copy_cmd, "web\\src");
            if (!nob_cmd_run_sync(copy_cmd)) {
                nob_cmd_free(copy_cmd);
                // Continue
            } else {
                nob_cmd_free(copy_cmd);
            }
        }
        // For permissions/
        char permissions_src[1024];
        sprintf(permissions_src, "src/plugins/%s/permissions", plugin_name);
        if (file_exists(permissions_src) == 1) {
            if (!mkdir_if_not_exists("android/app/src/main/res")) return false;
            Nob_Cmd copy_cmd = {0};
            nob_cmd_append(&copy_cmd, "xcopy");
            nob_cmd_append(&copy_cmd, "/E");
            nob_cmd_append(&copy_cmd, "/I");
            nob_cmd_append(&copy_cmd, "/Y");
            char permissions_src_star[1024];
            sprintf(permissions_src_star, "%s\\*", permissions_src);
            nob_cmd_append(&copy_cmd, permissions_src_star);
            nob_cmd_append(&copy_cmd, "android\\app\\src\\main\\res");
            if (!nob_cmd_run_sync(copy_cmd)) {
                nob_cmd_free(copy_cmd);
                // Continue
            } else {
                nob_cmd_free(copy_cmd);
            }
        }
    }
    da_free(plugins);
    return true;
}

bool generate_proguard_rules(const char *package_name) {
    FILE *f = fopen("android/app/proguard-rules.pro", "w");
    if (!f) return false;

    // Write base rules
    fprintf(f, "# Add project specific ProGuard rules here.\n");
    fprintf(f, "# You can control the set of applied configuration files using the\n");
    fprintf(f, "# proguardFiles setting in build.gradle.kts.\n");
    fprintf(f, "#\n");
    fprintf(f, "# For more details, see\n");
    fprintf(f, "#   http://developer.android.com/guide/developing/tools/proguard.html\n");
    fprintf(f, "#\n");
    fprintf(f, "# If your project uses WebView with JS, uncomment the following\n");
    fprintf(f, "# and specify the fully qualified class name to the JavaScript interface\n");
    fprintf(f, "# class:\n");
    fprintf(f, "-keepclassmembers class %s.Ipc {\n", package_name);
    fprintf(f, "    public *;\n");
    fprintf(f, "}\n");
    fprintf(f, "#\n");
    fprintf(f, "# Keep native methods\n");
    fprintf(f, "-keepclasseswithmembernames class %s.Ipc {\n", package_name);
    fprintf(f, "    native <methods>;\n");
    fprintf(f, "}\n");
    fprintf(f, "#\n");

    // Generate plugin rules
    Nob_File_Paths plugins = {0};
    if (!nob_read_entire_dir("src/plugins", &plugins)) {
        fclose(f);
        return false;
    }
    for (size_t i = 0; i < plugins.count; ++i) {
        const char *plugin_name = plugins.items[i];
        if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0) continue;
        if (!is_plugin_enabled(plugin_name)) continue;
        char android_path[1024];
        sprintf(android_path, "src/plugins/%s/android", plugin_name);
        if (file_exists(android_path) == 1) {  // if android dir exists
            fprintf(f, "# Keep %s plugin classes since they are accessed via JNI\n", plugin_name);
            fprintf(f, "-keep class %s.plugins.%s.** {\n", package_name, plugin_name);
            fprintf(f, "    *;\n");
            fprintf(f, "}\n");
            fprintf(f, "#\n");
        }
    }
    da_free(plugins);
    fclose(f);
    return true;
}

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

    // Create Android c dir
    if (!mkdir_if_not_exists("android/app/src/main/c")) return false;

    // Copy C source files to Android c dir
    if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return false;
    if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return false;
    if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return false;
    if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return false;
    if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return false;
    if (!copy_file("src/config.h", "android/app/src/main/c/config.h")) return false;

    // Copy plugin files
    if (!copy_plugins_for_android()) return false;

    // Generate ProGuard rules for plugins
    const char *package_name = get_package_name_android();
    if (!package_name) package_name = "com.example.crossweb"; // fallback
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

    // Create Android c dir
    if (!mkdir_if_not_exists("android/app/src/main/c")) return false;

    // Copy C source files to Android c dir
    if (!copy_file("src/android_bridge.c", "android/app/src/main/c/android_bridge.c")) return false;
    if (!copy_file("src/plug.c", "android/app/src/main/c/plug.c")) return false;
    if (!copy_file("src/ipc.c", "android/app/src/main/c/ipc.c")) return false;
    if (!copy_file("src/plug.h", "android/app/src/main/c/plug.h")) return false;
    if (!copy_file("src/ipc.h", "android/app/src/main/c/ipc.h")) return false;
    if (!copy_file("src/hotreload.h", "android/app/src/main/c/hotreload.h")) return false;
    if (!copy_file("src/hotreload_posix.c", "android/app/src/main/c/hotreload_posix.c")) return false;
    if (!copy_file("src/config.h", "android/app/src/main/c/config.h")) return false;

    // Copy plugin files
    if (!copy_plugins_for_android()) return false;

    // Generate ProGuard rules for plugins
    const char *package_name = get_package_name_android();
    if (!package_name) package_name = "com.example.crossweb"; // fallback
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
