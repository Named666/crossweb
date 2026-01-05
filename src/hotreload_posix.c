#include <stdio.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <time.h>

#include "hotreload.h"

#if defined(MUSIALIZER_TARGET_MACOS) || defined(CROSSWEB_TARGET_MACOS)
    static const char *libplug_file_name = "libplug.dylib";
#else
    static const char *libplug_file_name = "libplug.so";
#endif

static void *libplug = NULL;
static time_t libplug_last_mtime = 0;
static bool libplug_has_timestamp = false;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

bool reload_libplug(void)
{
    if (libplug != NULL) dlclose(libplug);

    libplug = dlopen(libplug_file_name, RTLD_NOW);
    if (libplug == NULL) {
        fprintf(stderr, "HOTRELOAD: could not load %s: %s\n", libplug_file_name, dlerror());
        return false;
    }

    #define PLUG(name, ...) \
        name = dlsym(libplug, #name); \
        if (name == NULL) { \
            fprintf(stderr, "HOTRELOAD: could not find %s symbol in %s: %s\n", \
                     #name, libplug_file_name, dlerror()); \
            return false; \
        }
    LIST_OF_PLUGS
    #undef PLUG

    struct stat st;
    if (stat(libplug_file_name, &st) == 0) {
        libplug_last_mtime = st.st_mtime;
        libplug_has_timestamp = true;
    }

    return true;
}

bool reload_libplug_changed(void)
{
    if (!libplug_has_timestamp) {
        return false;
    }
    struct stat st;
    if (stat(libplug_file_name, &st) != 0) {
        return false;
    }
    return st.st_mtime > libplug_last_mtime;
}
