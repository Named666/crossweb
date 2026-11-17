#include "common.h"

bool cli_hotreload_enabled = true;
int cli_argc = 0;
char **cli_argv = NULL;
/* Ensure the nob header exposes the stripped-prefix typedefs used throughout
    this file when compiled as a separate translation unit. Do NOT define
    NOB_IMPLEMENTATION here. */
#ifndef NOB_STRIP_PREFIX
#define NOB_STRIP_PREFIX
#endif
#ifndef NOB_H_
#include "../thirdparty/nob.h"
#endif
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdio.h> // for sprintf

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
        nob_log(NOB_ERROR, "Android project not initialized. Run 'nob android init' first.");
        return false;
    }
    if (file_exists("android/app/src/main/AndroidManifest.xml") != 1) {
        nob_log(NOB_ERROR, "Android manifest missing. Re-run 'nob android init'.");
        return false;
    }
    return true;
}

bool is_plugin_enabled(const char *plugin_name) {
    (void)plugin_name;
    return true;
}

void append_build_defines(Cmd *cmd) {
#ifdef CROSSWEB_HOTRELOAD
    cmd_append(cmd, "-DCROSSWEB_HOTRELOAD=1");
#endif
#ifdef CROSSWEB_UNBUNDLE
    cmd_append(cmd, "-DCROSSWEB_UNBUNDLE=1");
#endif
#ifdef CROSSWEB_TARGET_WIN64_GCC
    cmd_append(cmd, "-DCROSSWEB_TARGET_WIN64_GCC=1");
#endif
#ifdef CROSSWEB_TARGET_LINUX
    cmd_append(cmd, "-DCROSSWEB_TARGET_LINUX=1");
#endif
#ifdef CROSSWEB_TARGET_MACOS
    cmd_append(cmd, "-DCROSSWEB_TARGET_MACOS=1");
#endif
#ifdef CROSSWEB_TARGET_OPENBSD
    cmd_append(cmd, "-DCROSSWEB_TARGET_OPENBSD=1");
#endif
#ifdef CROSSWEB_TARGET_ANDROID
    cmd_append(cmd, "-DCROSSWEB_TARGET_ANDROID=1");
#endif
    Nob_File_Paths plugins = {0};
    if (!nob_read_entire_dir("src/plugins", &plugins)) return;
    for (size_t i = 0; i < plugins.count; ++i) {
        const char *plugin_name = plugins.items[i];
        if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0) continue;
        char macro[256];
        snprintf(macro, sizeof(macro), "-DCROSSWEB_PLUGIN_%s=1", plugin_name);
        for (size_t j = strlen("-DCROSSWEB_PLUGIN_"); j < strlen(macro); ++j) {
            macro[j] = (char)toupper((unsigned char)macro[j]);
        }
        cmd_append(cmd, nob_temp_strdup(macro));
    }
    da_free(plugins);
}

void append_cli_hotreload_define(Cmd *cmd) {
    if (cli_hotreload_enabled) cmd_append(cmd, "-DCROSSWEB_HOTRELOAD=1");
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

bool run_npm_command(const char *npm_cmd, bool background) {
    char shell_cmd[2048];
    snprintf(shell_cmd, sizeof(shell_cmd), "cd /d web && npm %s", npm_cmd);

    Cmd cmd = {0};
    cmd_append(&cmd, "cmd");
    cmd_append(&cmd, "/c");
    if (background) {
        cmd_append(&cmd, "start");
        cmd_append(&cmd, "\"\"");
        cmd_append(&cmd, "cmd");
        cmd_append(&cmd, "/c");
        cmd_append(&cmd, shell_cmd);
    } else {
        cmd_append(&cmd, shell_cmd);
    }
    bool result = cmd_run(&cmd);
    cmd_free(cmd);
    return result;
}

bool run_android_emulator(const char *avd_name, int wait_seconds) {
    const char *android_home = getenv("ANDROID_HOME");
    if (!android_home) {
        nob_log(NOB_ERROR, "ANDROID_HOME environment variable is not set");
        return false;
    }
    char emulator_path[1024];
    sprintf(emulator_path, "%s\\emulator\\emulator.exe", android_home);
    Cmd cmd = {0};
    cmd_append(&cmd, emulator_path);
    cmd_append(&cmd, "-avd");
    cmd_append(&cmd, avd_name);
    cmd_append(&cmd, "-no-snapshot");
    cmd_append(&cmd, "-gpu");
    cmd_append(&cmd, "off");
    if (!cmd_run_async(cmd)) return false; // Run in background
    // Give emulator process time to start.
    cmd = (Cmd){0};
    cmd_append(&cmd, "cmd");
    cmd_append(&cmd, "/c");
    cmd_append(&cmd, "timeout");
    char timeout_str[16];
    sprintf(timeout_str, "%d", wait_seconds);
    cmd_append(&cmd, timeout_str);
    cmd_append(&cmd, "/nobreak");
    if (!cmd_run(&cmd)) return false;

    // Wait for adb to detect a connected device.
    cmd = (Cmd){0};
    cmd_append(&cmd, "adb");
    cmd_append(&cmd, "wait-for-device");
    if (!cmd_run(&cmd)) return false;

    // Poll until Android reports boot complete.
    for (int i = 0; i < 180; ++i) {
        cmd = (Cmd){0};
        cmd_append(&cmd, "cmd");
        cmd_append(&cmd, "/c");
        cmd_append(&cmd, "adb");
        cmd_append(&cmd, "shell");
        cmd_append(&cmd, "getprop");
        cmd_append(&cmd, "sys.boot_completed");
        cmd_append(&cmd, "|");
        cmd_append(&cmd, "findstr");
        cmd_append(&cmd, "/R");
        cmd_append(&cmd, "^1$");
        if (cmd_run(&cmd)) {
            nob_log(NOB_INFO, "Android emulator is ready");
            return true;
        }

        cmd = (Cmd){0};
        cmd_append(&cmd, "cmd");
        cmd_append(&cmd, "/c");
        cmd_append(&cmd, "timeout");
        cmd_append(&cmd, "1");
        cmd_append(&cmd, "/nobreak");
        cmd_run(&cmd);
    }

    nob_log(NOB_ERROR, "Timed out waiting for Android emulator boot completion");
    return false;
}
