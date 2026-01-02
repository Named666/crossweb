#include "plugins.h"
#include "common.h"
#include "common.h"
#include <string.h>

bool copy_plugins_for_android(void) {
    const char *package_name = get_package_name();
    if (!package_name) package_name = "com.example.crossweb";
    
    char mangled[256], package_path[1024];
    mangle_package_name(mangled, package_name);
    slash_package_name(package_path, package_name);
    
    if (!mkdir_if_not_exists("android/app/src/main/c/plugins")) return false;
    Nob_File_Paths plugins = {0};
    if (!nob_read_entire_dir("src/plugins", &plugins)) return false;
    for (size_t i = 0; i < plugins.count; ++i) {
        const char *plugin_name = plugins.items[i];
        if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0 || !is_plugin_enabled(plugin_name)) continue;
        char plugin_c_dir[1024];
        sprintf(plugin_c_dir, "android/app/src/main/c/plugins/%s", plugin_name);
        if (!mkdir_if_not_exists(plugin_c_dir)) return false;
        Nob_File_Paths src_files = {0};
        char src_dir[1024];
        sprintf(src_dir, "src/plugins/%s/src", plugin_name);
        if (!nob_read_entire_dir(src_dir, &src_files)) {
            sprintf(src_dir, "src/plugins/%s", plugin_name);
            if (!nob_read_entire_dir(src_dir, &src_files)) continue;
        }
        for (size_t j = 0; j < src_files.count; ++j) {
            const char *file = src_files.items[j];
            char src[1024], dst[1024];
            sprintf(src, "%s/%s", src_dir, file);
            sprintf(dst, "%s/%s", plugin_c_dir, file);
            if (!copy_file(src, dst)) {
                da_free(src_files);
                da_free(plugins);
                return false;
            }
            size_t len = strlen(file);
            if (len > 2 && strcmp(file + len - 2, ".c") == 0) {
                if (!replace_all_in_file(dst, "com_example_crossweb", mangled)) {
                    da_free(src_files);
                    da_free(plugins);
                    return false;
                }
                char slashed[256];
                slash_package_name(slashed, package_name);
                if (!replace_all_in_file(dst, "com/example/crossweb", slashed)) {
                    da_free(src_files);
                    da_free(plugins);
                    return false;
                }
                if (!replace_all_in_file(dst, "__PACKAGE_PATH__", slashed)) {
                    da_free(src_files);
                    da_free(plugins);
                    return false;
                }
            }
        }
        da_free(src_files);
        // For guest-js/
        char guest_js_src[1024];
        sprintf(guest_js_src, "src/plugins/%s/js", plugin_name);
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
            } else {
                nob_cmd_free(copy_cmd);
            }
            // Copy Android Kotlin/Java plugin sources (if present) into app java package
            char android_src_dir[1024];
            sprintf(android_src_dir, "src/plugins/%s/android", plugin_name);
            if (file_exists(android_src_dir) == 1) {
                // target: android/app/src/main/java/<package_path>/plugins/<plugin_name>
                char package_path[1024];
                slash_package_name(package_path, package_name);
                char target_dir[1024];
                sprintf(target_dir, "android/app/src/main/java/%s/plugins/%s", package_path, plugin_name);
                if (!mkdir_recursive(target_dir)) {
                    da_free(plugins);
                    return false;
                }
                Nob_File_Paths android_files = {0};
                if (nob_read_entire_dir(android_src_dir, &android_files)) {
                    for (size_t k = 0; k < android_files.count; ++k) {
                        const char *file = android_files.items[k];
                        if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) continue;
                        size_t len = strlen(file);
                        if ((len > 3 && strcmp(file + len - 3, ".kt") == 0) || (len > 5 && strcmp(file + len - 5, ".java") == 0)) {
                            char src_file[1024], dst_file[1024];
                            sprintf(src_file, "%s/%s", android_src_dir, file);
                            sprintf(dst_file, "%s/%s", target_dir, file);
                            if (!copy_file(src_file, dst_file)) {
                                da_free(android_files);
                                da_free(plugins);
                                return false;
                            }
                            // Replace package placeholders in copied sources
                            if (!replace_all_in_file(dst_file, "__PACKAGE__", package_name)) {
                                da_free(android_files);
                                da_free(plugins);
                                return false;
                            }
                            char slashed[256];
                            slash_package_name(slashed, package_name);
                            if (!replace_all_in_file(dst_file, "__PACKAGE_PATH__", slashed)) {
                                da_free(android_files);
                                da_free(plugins);
                                return false;
                            }
                        }
                    }
                    da_free(android_files);
                }
            }
        }
    }
    da_free(plugins);
    return true;
}

bool generate_proguard_rules(const char *package_name) {
    FILE *f = fopen("android/app/proguard-rules.pro", "w");
    if (!f) return false;
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
    Nob_File_Paths plugins = {0};
    if (!nob_read_entire_dir("src/plugins", &plugins)) {
        fclose(f);
        return false;
    }
    for (size_t i = 0; i < plugins.count; ++i) {
        const char *plugin_name = plugins.items[i];
        if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0 || !is_plugin_enabled(plugin_name)) continue;
        char android_path[1024];
        sprintf(android_path, "src/plugins/%s/android", plugin_name);
        if (file_exists(android_path) == 1) {
            Nob_File_Paths android_files = {0};
            if (!nob_read_entire_dir(android_path, &android_files)) continue;
            for (size_t j = 0; j < android_files.count; ++j) {
                const char *file = android_files.items[j];
                if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) continue;
                size_t len = strlen(file);
                const char *ext = NULL;
                if (len > 3 && strcmp(file + len - 3, ".kt") == 0) {
                    ext = ".kt";
                } else if (len > 5 && strcmp(file + len - 5, ".java") == 0) {
                    ext = ".java";
                }
                if (ext) {
                    char class_name[256];
                    size_t class_len = len - strlen(ext);
                    if (class_len >= sizeof(class_name)) continue;
                    memcpy(class_name, file, class_len);
                    class_name[class_len] = '\0';
                    fprintf(f, "# Keep %s class and its members since it's accessed via JNI\n", class_name);
                    fprintf(f, "-keep class %s.plugins.%s.%s {\n", package_name, plugin_name, class_name);
                    fprintf(f, "    *;\n");
                    fprintf(f, "}\n");
                    fprintf(f, "# Keep native methods\n");
                    fprintf(f, "-keepclasseswithmembernames class %s.plugins.%s.%s {\n", package_name, plugin_name, class_name);
                    fprintf(f, "    native <methods>;\n");
                    fprintf(f, "}\n");
                    fprintf(f, "#\n");
                }
            }
            da_free(android_files);
        }
    }
    da_free(plugins);
    fclose(f);
    return true;
}