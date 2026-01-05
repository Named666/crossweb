#ifndef PLUGINS_H_
#define PLUGINS_H_

#include <stdbool.h>

// Platform types for plugin source collection
typedef enum {
    PLATFORM_DESKTOP,   // Windows, macOS, Linux
    PLATFORM_MOBILE     // Android, iOS
} PluginPlatform;

// Collect all plugin source files for compilation.
// Scans src/plugins/ and returns paths relative to project root.
// Caller must free the returned paths with da_free().
// - platform: PLATFORM_DESKTOP or PLATFORM_MOBILE (affects mobile.c vs desktop.c)
// - files: output array of source file paths
// Returns true on success.
bool collect_plugin_sources(PluginPlatform platform, Nob_File_Paths *files);

// Collect libraries needed for plugins (e.g., "-lcrypt32" for keystore on Windows)
// - libs: output array of library flags
// Returns true on success.
bool collect_plugin_libs(Nob_File_Paths *libs);

bool copy_plugins_for_android(void);
bool generate_proguard_rules(const char *package_name);

#endif // PLUGINS_H_