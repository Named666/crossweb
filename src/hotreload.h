#ifndef HOTRELOAD_H_
#define HOTRELOAD_H_

#include <stdbool.h>

#include "plug.h"
#include "config.h"

#ifdef CROSSWEB_HOTRELOAD
    #define PLUG(name, ...) extern name##_t *name;
    LIST_OF_PLUGS
    #undef PLUG
    bool reload_libplug(void);
    bool reload_libplug_changed(void);
#else
    #define PLUG(name, ret, ...) ret name(__VA_ARGS__);
    LIST_OF_PLUGS
    #undef PLUG
#endif // CROSSWEB_HOTRELOAD

#endif // HOTRELOAD_H_
