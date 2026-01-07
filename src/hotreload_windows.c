#include <stdio.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include "hotreload.h"
#include "common.h"
#include "plugins.h"

// Hot-reload of libplug.dll implementation for Windows
// 
// Strategy: We load libplug.dll directly. When source files change, we:
// 1. Unload the current DLL (FreeLibrary)
// 2. Rebuild the DLL (compiler can now overwrite the file)
// 3. Load the new DLL (LoadLibrary)
//
// This is simpler than copying to temp files and avoids accumulating
// leftover DLLs.

static const char *libplug_file_name = "./build/libplug.dll";
static HMODULE libplug = NULL;

// Source directory watching
static FILETIME src_last_write_time = {0};
static bool src_has_timestamp = false;

static bool rebuild_requested = false;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

// Forward declarations
static bool rebuild_plugin_dll(void);

// Get the most recent modification time of any .c/.h file in a directory (recursive)
static bool get_dir_latest_time(const char *dir, FILETIME *latest) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir);
    
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    memset(latest, 0, sizeof(*latest));
    bool found_any = false;
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir, find_data.cFileName);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            FILETIME sub_latest;
            if (get_dir_latest_time(full_path, &sub_latest)) {
                if (CompareFileTime(&sub_latest, latest) > 0) {
                    *latest = sub_latest;
                    found_any = true;
                }
            }
        } else {
            const char *ext = strrchr(find_data.cFileName, '.');
            if (ext && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)) {
                if (CompareFileTime(&find_data.ftLastWriteTime, latest) > 0) {
                    *latest = find_data.ftLastWriteTime;
                    found_any = true;
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
    return found_any;
}

// Get the latest modification time across all source directories
static bool get_sources_latest_time(FILETIME *latest) {
    // ./src already contains ./src/plugins recursively.
    return get_dir_latest_time(".\\src", latest);
}

static bool rebuild_plugin_dll(void) {
    printf("HOTRELOAD: rebuilding plugin DLL...\n");

    // Collect plugin source files automatically
    Nob_File_Paths plugin_sources = {0};
    if (!collect_plugin_sources(PLATFORM_DESKTOP, &plugin_sources)) {
        nob_log(NOB_ERROR, "Failed to collect plugin sources");
        return false;
    }
    
    // Collect plugin library dependencies
    Nob_File_Paths plugin_libs = {0};
    collect_plugin_libs(&plugin_libs);

    Cmd cmd = {0};
    cmd_append(&cmd, "gcc");
    cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-include", "build/config.h");
    cmd_append(&cmd, "-DCROSSWEB_BUILDING_PLUG=1");
    cmd_append(&cmd, "-DWEBVIEW_WINAPI");
    cmd_append(&cmd, "-fPIC", "-shared");
    cmd_append(&cmd, "-static-libgcc");
    cmd_append(&cmd, "-Wno-implicit-function-declaration");
    cmd_append(&cmd, "-o", "./build/libplug.dll");
    cmd_append(&cmd, "./src/plug.c");
    
    // Add all discovered plugin sources
    for (size_t i = 0; i < plugin_sources.count; ++i) {
        cmd_append(&cmd, plugin_sources.items[i]);
    }
    
    cmd_append(&cmd, "-lole32", "-loleaut32", "-lcrypt32", "-lwebauthn");
    
    // Add plugin-specific libraries
    for (size_t i = 0; i < plugin_libs.count; ++i) {
        cmd_append(&cmd, plugin_libs.items[i]);
    }
    
    if (!cmd_run(&cmd)) {
        nob_cmd_free(cmd);
        nob_da_free(plugin_sources);
        nob_da_free(plugin_libs);
        return false;
    }

    nob_cmd_free(cmd);
    nob_da_free(plugin_sources);
    nob_da_free(plugin_libs);

    printf("HOTRELOAD: build successful\n");
    return true;
}

bool reload_libplug_changed(void)
{
    FILETIME current_latest;
    if (!get_sources_latest_time(&current_latest)) {
        return false;
    }

    if (!src_has_timestamp) {
        src_last_write_time = current_latest;
        src_has_timestamp = true;
        return false;
    }

    if (CompareFileTime(&current_latest, &src_last_write_time) <= 0) {
        return false; // No changes
    }

    printf("HOTRELOAD: source files changed, triggering rebuild...\n");

    // Only flag here; actual rebuild happens in reload_libplug() where we can
    // safely unload the DLL first.
    rebuild_requested = true;
    src_last_write_time = current_latest;
    return true;
}

bool reload_libplug(void)
{
    printf("HOTRELOAD: starting plugin reload\n");

    // Unload first so the build can overwrite the DLL file.
    if (libplug != NULL) {
        FreeLibrary(libplug);
        libplug = NULL;
        // Give the OS a moment to drop file locks in edge cases.
        Sleep(1);
    }

    if (rebuild_requested) {
        if (!rebuild_plugin_dll()) {
            // Keep rebuild_requested=true so we can try again later.
            return false;
        }
        rebuild_requested = false;
    }

    // Load the active DLL
    libplug = LoadLibraryA(libplug_file_name);
    if (libplug == NULL) {
        printf("HOTRELOAD: could not load %s: %lu\n", libplug_file_name, GetLastError());
        return false;
    }

    #define PLUG(name, ...) name##_t *new_##name = NULL;
    LIST_OF_PLUGS
    #undef PLUG

    #define PLUG(name, ...) \
        do { \
            FARPROC proc = GetProcAddress(libplug, #name); \
            if (proc == NULL) { \
                printf("HOTRELOAD: could not find %s symbol in %s: %lu\n", \
                       #name, libplug_file_name, GetLastError()); \
                FreeLibrary(libplug); \
                libplug = NULL; \
                return false; \
            } \
            memcpy(&new_##name, &proc, sizeof(new_##name)); \
        } while (0);
    LIST_OF_PLUGS
    #undef PLUG

    #define PLUG(name, ...) name = new_##name;
    LIST_OF_PLUGS
    #undef PLUG

    printf("HOTRELOAD: plugin loaded successfully from %s\n", libplug_file_name);

    return true;
}
