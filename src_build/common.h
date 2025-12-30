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

#endif // COMMON_H_