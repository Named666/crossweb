#ifndef COMMON_H_
#define COMMON_H_

#include <stdbool.h>
#include <stddef.h>

// File system utilities
bool is_dir(const char *path);
bool mkdir_recursive(const char *path);
bool copy_dir_recursive(const char *src, const char *dst);
bool remove_dir_recursive(const char *path);
bool replace_all_in_file(const char *path, const char *old_str, const char *new_str);

// String utilities
void mangle_package_name(char *mangled, const char *package_name);
void slash_package_name(char *slashed, const char *package_name);

// Build/project helpers (moved from config.c)
const char *get_package_name(void);
bool is_plugin_enabled(const char *plugin_name);
bool validate_android_init(void);

// Command helpers
bool run_npm_command(const char *npm_cmd, bool background);
bool run_android_emulator(const char *avd_name, int wait_seconds);

#endif // COMMON_H_