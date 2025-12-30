#include "templates.h"
#include "common.h"
#include <string.h>

bool replace_package_placeholders_in_file(const char *file_path, const char *package_name) {
    char mangled[256], slashed[256];
    mangle_package_name(mangled, package_name);
    slash_package_name(slashed, package_name);
    if (!replace_all_in_file(file_path, "__PACKAGE_MANGLED__", mangled)) return false;
    if (!replace_all_in_file(file_path, "__PACKAGE_PATH__", slashed)) return false;
    if (!replace_all_in_file(file_path, "__PACKAGE__", package_name)) return false;
    if (!replace_all_in_file(file_path, "com_example_crossweb", mangled)) return false;
    if (!replace_all_in_file(file_path, "com/example/crossweb", slashed)) return false;
    if (!replace_all_in_file(file_path, "com.example.crossweb", package_name)) return false;
    return true;
}

bool copy_template_with_replacements(const char *template_dir, const char *target_dir, const char *package_name, const char *app_name) {
    if (!copy_dir_recursive(template_dir, target_dir)) return false;
    char gradle_cache_dir[1024];
    sprintf(gradle_cache_dir, "%s/.gradle", target_dir);
    remove_dir_recursive(gradle_cache_dir);
    char old_java_dir[1024];
    sprintf(old_java_dir, "%s/app/src/main/java", target_dir);
    remove_dir_recursive(old_java_dir);
    char package_path[1024];
    slash_package_name(package_path, package_name);
    char java_dir[1024];
    sprintf(java_dir, "%s/app/src/main/java/%s", target_dir, package_path);
    if (!mkdir_recursive(java_dir)) return false;
    char template_java_dir[1024];
    sprintf(template_java_dir, "%s/app/src/main/java/com/example/crossweb", template_dir);
    Nob_File_Paths files = {0};
    if (read_entire_dir(template_java_dir, &files)) {
        for (size_t i = 0; i < files.count; ++i) {
            const char *file = files.items[i];
            if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) continue;
            size_t len = strlen(file);
            if (len > 3 && (strcmp(file + len - 3, ".kt") == 0 || strcmp(file + len - 5, ".java") == 0)) {
                char template_file[1024], new_file[1024];
                sprintf(template_file, "%s/%s", template_java_dir, file);
                sprintf(new_file, "%s/%s", java_dir, file);
                String_Builder content = {0};
                if (!read_entire_file(template_file, &content)) {
                    da_free(files);
                    return false;
                }
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
                size_t remaining_len = content.count - (search - content.items);
                da_append_many(&replaced, search, remaining_len);
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
                    remaining_len = replaced.count - (search - replaced.items);
                    da_append_many(&final_content, search, remaining_len);
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
    const char *files_to_replace[] = {
        "app/build.gradle.kts",
        "app/src/main/AndroidManifest.xml",
        "app/src/main/res/layout/activity_main.xml",
        "app/src/main/res/values/strings.xml",
        "app/src/main/res/values/styles.xml",
        "app/proguard-rules.pro",
        "settings.gradle",
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