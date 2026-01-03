#include <stdio.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include "hotreload.h"

static const char *libplug_source_path = "./build/libplug.dll";
static HMODULE libplug = NULL;
static char libplug_loaded_path[MAX_PATH] = {0};
static FILETIME libplug_last_write_time = {0};
static unsigned int libplug_generation = 0;
static bool libplug_has_timestamp = false;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

static void unload_current_library(void)
{
    if (libplug != NULL) {
        FreeLibrary(libplug);
        libplug = NULL;
    }
    if (libplug_loaded_path[0] != '\0') {
        DeleteFileA(libplug_loaded_path);
        libplug_loaded_path[0] = '\0';
    }
}

static bool copy_plugin_to_temp(char *dst, size_t dst_size)
{
    ++libplug_generation;
    int written = snprintf(dst, dst_size, ".\\build\\libplug_hot_%u.dll", libplug_generation);
    if (written <= 0 || (size_t)written >= dst_size) {
        printf("HOTRELOAD: failed to format temporary plugin path\n");
        return false;
    }
    if (!CopyFileA(libplug_source_path, dst, FALSE)) {
        printf("HOTRELOAD: could not copy %s to %s: %lu\n",
               libplug_source_path, dst, GetLastError());
        return false;
    }
    return true;
}

bool reload_libplug(void)
{
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(libplug_source_path, GetFileExInfoStandard, &attrs)) {
        printf("HOTRELOAD: could not stat %s: %lu\n", libplug_source_path, GetLastError());
        return false;
    }

    char temp_path[MAX_PATH];
    if (!copy_plugin_to_temp(temp_path, sizeof(temp_path))) {
        return false;
    }

    HMODULE handle = LoadLibraryA(temp_path);
    if (handle == NULL) {
        printf("HOTRELOAD: could not load %s: %lu\n", temp_path, GetLastError());
        DeleteFileA(temp_path);
        return false;
    }

    #define PLUG(name, ...) name##_t *new_##name = NULL;
    LIST_OF_PLUGS
    #undef PLUG

    #define PLUG(name, ...) \
        do { \
            FARPROC proc = GetProcAddress(handle, #name); \
            if (proc == NULL) { \
                printf("HOTRELOAD: could not find %s symbol in %s: %lu\n", \
                       #name, temp_path, GetLastError()); \
                goto fail; \
            } \
            memcpy(&new_##name, &proc, sizeof(new_##name)); \
        } while (0);
    LIST_OF_PLUGS
    #undef PLUG

    unload_current_library();

    libplug = handle;
    strncpy(libplug_loaded_path, temp_path, sizeof(libplug_loaded_path));
    libplug_loaded_path[sizeof(libplug_loaded_path) - 1] = '\0';

    #define PLUG(name, ...) name = new_##name;
    LIST_OF_PLUGS
    #undef PLUG

    libplug_last_write_time = attrs.ftLastWriteTime;
    libplug_has_timestamp = true;
    return true;

fail:
    FreeLibrary(handle);
    DeleteFileA(temp_path);
    return false;
}

bool reload_libplug_changed(void)
{
    if (!libplug_has_timestamp) {
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(libplug_source_path, GetFileExInfoStandard, &attrs)) {
        return false;
    }
    return CompareFileTime(&attrs.ftLastWriteTime, &libplug_last_write_time) > 0;
}
