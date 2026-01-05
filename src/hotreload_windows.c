#include <stdio.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include "hotreload.h"
// Hot-reload of libplug.dll implementation for Windows
static char libplug_source_path[MAX_PATH] = "./build/libplug.dll";
static HMODULE libplug = NULL;
static char libplug_loaded_path[MAX_PATH] = {0};
static FILETIME libplug_last_write_time = {0};
static unsigned int libplug_generation = 0;
static bool libplug_has_timestamp = false;

// Source directory watching
static FILETIME src_last_write_time = {0};
static FILETIME plugins_last_write_time = {0};
static bool src_has_timestamp = false;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

// Get the most recent modification time of any file in a directory (recursive)
static bool get_dir_latest_time(const char *dir, FILETIME *latest) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir);
    
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("HOTRELOAD: failed to open directory %s: %lu\n", dir, GetLastError());
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
            // Recurse into subdirectory
            FILETIME sub_latest;
            if (get_dir_latest_time(full_path, &sub_latest)) {
                if (CompareFileTime(&sub_latest, latest) > 0) {
                    *latest = sub_latest;
                    found_any = true;
                }
            }
        } else {
            // Check if this is a .c or .h file
            const char *ext = strrchr(find_data.cFileName, '.');
            if (ext && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)) {
                // printf("HOTRELOAD: found source file %s\n", full_path);
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

// Check if source files have changed and rebuild if needed
static bool check_and_rebuild_sources(void) {
    FILETIME src_current = {0};
    FILETIME plugins_current = {0};
    
    bool src_ok = get_dir_latest_time(".\\src", &src_current);
    bool plugins_ok = get_dir_latest_time(".\\src\\plugins", &plugins_current);
    
    printf("HOTRELOAD: src scan %s, plugins scan %s\n", src_ok ? "OK" : "FAILED", plugins_ok ? "OK" : "FAILED");
    
    // Use whichever is more recent
    FILETIME current_latest = src_current;
    if (CompareFileTime(&plugins_current, &current_latest) > 0) {
        current_latest = plugins_current;
    }
    
    if (!src_has_timestamp) {
        src_last_write_time = current_latest;
        plugins_last_write_time = plugins_current;
        src_has_timestamp = true;
        printf("HOTRELOAD: initialized source watch timestamps\n");
        return false;
    }
    
    // Check if sources changed
    FILETIME combined_last = src_last_write_time;
    if (CompareFileTime(&plugins_last_write_time, &combined_last) > 0) {
        combined_last = plugins_last_write_time;
    }
    
    printf("HOTRELOAD: current latest: %llu, last known: %llu\n", 
           ((ULARGE_INTEGER*)&current_latest)->QuadPart,
           ((ULARGE_INTEGER*)&combined_last)->QuadPart);
    
    if (CompareFileTime(&current_latest, &combined_last) <= 0) {
        return false; // No changes
    }
    
    printf("HOTRELOAD: source files changed, rebuilding plugin DLL...\n");
    
    // Run the build command for the plugin DLL only
    STARTUPINFOA si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    
    // Build to a temporary location first
    char temp_dll_path[MAX_PATH];
    ++libplug_generation;
    int written = snprintf(temp_dll_path, sizeof(temp_dll_path), ".\\build\\libplug_hot_%u.dll", libplug_generation);
    if (written <= 0 || (size_t)written >= sizeof(temp_dll_path)) {
        printf("HOTRELOAD: failed to format temporary DLL path\n");
        return false;
    }
    
    // Build command - hardcoded for now, matches the build system
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), 
             "gcc -Wall -Wextra -ggdb -I. -include build/config.h "
             "-DCROSSWEB_BUILDING_PLUG=1 -DWEBVIEW_WINAPI -fPIC -shared "
             "-static-libgcc -Wno-implicit-function-declaration "
             "-o %s ./src/plug.c "
             "./src/plugins/fs/lib.c ./src/plugins/fs/commands.c "
             "./src/plugins/fs/error.c "
             "./src/plugins/keystore/src/plugin.c "
             "-lole32 -loleaut32 -lcrypt32 -lwebauthn",
             temp_dll_path);
    
    printf("HOTRELOAD: executing: %s\n", cmd);
    
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("HOTRELOAD: failed to start compiler: %lu\n", GetLastError());
        return false;
    }
    
    printf("HOTRELOAD: build process started (PID: %lu)\n", pi.dwProcessId);
    
    // Wait for build to complete
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    printf("HOTRELOAD: build process finished with exit code %lu\n", exit_code);
    
    if (exit_code != 0) {
        printf("HOTRELOAD: build failed with exit code %lu\n", exit_code);
        // Update timestamps anyway to avoid rebuild loop on error
        src_last_write_time = src_current;
        plugins_last_write_time = plugins_current;
        return false;
    }
    
    printf("HOTRELOAD: build successful, new DLL at %s\n", temp_dll_path);
    
    // Update the source path to the newly built DLL
    strncpy(libplug_source_path, temp_dll_path, sizeof(libplug_source_path) - 1);
    libplug_source_path[sizeof(libplug_source_path) - 1] = '\0';
    
    src_last_write_time = src_current;
    plugins_last_write_time = plugins_current;
    return true;
}

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

    // Retry loop for CopyFileA in case the file is being written to by the compiler
    for (int i = 0; i < 10; ++i) {
        if (CopyFileA(libplug_source_path, dst, FALSE)) {
            return true;
        }
        if (GetLastError() != ERROR_SHARING_VIOLATION) {
            break;
        }
        Sleep(10);
    }

    printf("HOTRELOAD: could not copy %s to %s: %lu\n",
           libplug_source_path, dst, GetLastError());
    return false;
}

bool reload_libplug(void)
{
    printf("HOTRELOAD: starting plugin reload\n");
    
    // If libplug.dll doesn't exist, try to find an existing hot reload DLL
    if (!GetFileAttributesExA(libplug_source_path, GetFileExInfoStandard, &(WIN32_FILE_ATTRIBUTE_DATA){0})) {
        printf("HOTRELOAD: %s not found, looking for existing hot reload DLL\n", libplug_source_path);
        
        // Look for the most recent libplug_hot_*.dll
        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(".\\build\\libplug_hot_*.dll", &find_data);
        if (hFind != INVALID_HANDLE_VALUE) {
            char latest_dll[MAX_PATH] = {0};
            FILETIME latest_time = {0};
            
            do {
                if (CompareFileTime(&find_data.ftLastWriteTime, &latest_time) > 0) {
                    latest_time = find_data.ftLastWriteTime;
                    snprintf(latest_dll, sizeof(latest_dll), ".\\build\\%s", find_data.cFileName);
                }
            } while (FindNextFileA(hFind, &find_data));
            
            FindClose(hFind);
            
            if (latest_dll[0] != '\0') {
                printf("HOTRELOAD: found existing DLL: %s\n", latest_dll);
                strncpy(libplug_source_path, latest_dll, sizeof(libplug_source_path) - 1);
            }
        }
    }
    
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
    printf("HOTRELOAD: plugin loaded successfully\n");
    return true;

fail:
    FreeLibrary(handle);
    DeleteFileA(temp_path);
    return false;
}

bool reload_libplug_changed(void)
{
    // printf("HOTRELOAD: checking for source changes...\n");
    
    // First check if source files changed and rebuild if needed
    bool rebuilt = check_and_rebuild_sources();
    
    if (!libplug_has_timestamp) {
        // printf("HOTRELOAD: no DLL timestamp set yet\n");
        return false;
    }
    
    // If we just rebuilt, give the filesystem a moment
    if (rebuilt) {
        Sleep(6);
    }
    
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(libplug_source_path, GetFileExInfoStandard, &attrs)) {
        printf("HOTRELOAD: failed to get DLL attributes: %lu\n", GetLastError());
        return false;
    }
    
    bool changed = CompareFileTime(&attrs.ftLastWriteTime, &libplug_last_write_time) > 0;
    if (changed) {
        printf("HOTRELOAD: plugin DLL changed, reloading...\n");
    }
    return changed;
}
