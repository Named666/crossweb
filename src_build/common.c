#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <direct.h>

// Moved from config.c
const char *get_package_name(void) {
    static char package_name[256];
    String_Builder gradle = {0};
    if (!read_entire_file("android/app/build.gradle.kts", &gradle)) return NULL;
    const char *namespace_attr = "namespace = \"";
    char *pos = strstr(gradle.items, namespace_attr);
    if (!pos) { da_free(gradle); return NULL; }
    pos += strlen(namespace_attr);
    char *end = strchr(pos, '"');
    if (!end) { da_free(gradle); return NULL; }
    size_t len = end - pos;
    if (len >= sizeof(package_name)) { da_free(gradle); return NULL; }
    memcpy(package_name, pos, len);
    package_name[len] = '\0';
    da_free(gradle);
    return package_name;
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

bool is_plugin_enabled(const char *plugin_name) {
    String_Builder content = {0};
    if (!read_entire_file("build/config.h", &content)) return true;
    char macro[256];
    sprintf(macro, "#define CROSSWEB_PLUGIN_%s 1", plugin_name);
    for (size_t j = strlen("#define CROSSWEB_PLUGIN_"); j < strlen(macro); ++j) {
        macro[j] = toupper(macro[j]);
    }
    bool enabled = strstr(content.items, macro) != NULL;
    da_free(content);
    return enabled;
}

bool is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool mkdir_recursive(const char *path) {
    char temp[1024];
    strcpy(temp, path);
    for (char *p = temp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (!mkdir_if_not_exists(temp)) return false;
            *p = '/';
        }
    }
    return mkdir_if_not_exists(temp);
}

bool copy_dir_recursive(const char *src, const char *dst) {
    Nob_File_Paths entries = {0};
    if (!read_entire_dir(src, &entries)) return false;
    for (size_t i = 0; i < entries.count; ++i) {
        const char *entry = entries.items[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        char src_path[1024], dst_path[1024];
        sprintf(src_path, "%s/%s", src, entry);
        sprintf(dst_path, "%s/%s", dst, entry);
        if (is_dir(src_path)) {
            if (!mkdir_if_not_exists(dst_path)) return false;
            if (!copy_dir_recursive(src_path, dst_path)) return false;
        } else {
            if (!copy_file(src_path, dst_path)) return false;
        }
    }
    da_free(entries);
    return true;
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
            if (!remove_dir_recursive(subpath)) return false;
        } else {
            remove(subpath);
        }
    }
    da_free(entries);
    rmdir(path);
    return true;
}

bool replace_all_in_file(const char *path, const char *old_str, const char *new_str) {
    String_Builder content = {0};
    if (!read_entire_file(path, &content)) return false;
    String_Builder new_content = {0};
    const char *search_start = content.items;
    size_t old_len = strlen(old_str), new_len = strlen(new_str);
    while (1) {
        const char *pos = strstr(search_start, old_str);
        if (!pos) break;
        size_t prefix_len = pos - search_start;
        da_append_many(&new_content, search_start, prefix_len);
        da_append_many(&new_content, new_str, new_len);
        search_start = pos + old_len;
    }
    size_t remaining_len = content.count - (search_start - content.items);
    da_append_many(&new_content, search_start, remaining_len);
    bool result = write_entire_file(path, new_content.items, new_content.count);
    da_free(content);
    da_free(new_content);
    return result;
}

void mangle_package_name(char *mangled, const char *package_name) {
    strcpy(mangled, package_name);
    for (char *p = mangled; *p; ++p) {
        if (*p == '.') *p = '_';
    }
}

void slash_package_name(char *slashed, const char *package_name) {
    strcpy(slashed, package_name);
    for (char *p = slashed; *p; ++p) {
        if (*p == '.') *p = '/';
    }
}