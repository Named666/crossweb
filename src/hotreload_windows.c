#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include "hotreload.h"

static const char *libplug_file_name = "libplug.dll";
static void *libplug = NULL;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

bool reload_libplug(void)
{
    if (libplug != NULL) FreeLibrary(libplug);

    libplug = LoadLibrary(libplug_file_name);
    if (libplug == NULL) {
        printf("HOTRELOAD: could not load %s: %lu\n", libplug_file_name, GetLastError());
        return 0;
    }

    #define PLUG(name, ...) \
        name = (void*)GetProcAddress(libplug, #name); \
        if (name == NULL) { \
            printf("HOTRELOAD: could not find %s symbol in %s: %lu\n", \
                     #name, libplug_file_name, GetLastError()); \
            return 0; \
        }
    LIST_OF_PLUGS
    #undef PLUG

    return 1;
}

bool reload_libplug_changed(void) {
    // Stub: always return false for now
    return 0;
}
