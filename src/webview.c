#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "hotreload.h"
#include "plug.h"

int main(void)
{
#ifndef _WIN32
    struct sigaction act = {0};
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
#endif

    if (!reload_libplug()) return 1;

    // Initialize plugins with a NULL webview for now
    plug_init((webview_t)NULL);

    // Simple run loop. In a real webview-based binary this would run the
    // platform webview event loop and call `plug_update(webview)` from it.
    for (;;) {
#ifdef CROSSWEB_HOTRELOAD
        if (reload_libplug_changed()) {
            void *state = plug_pre_reload();
            if (!reload_libplug()) return 1;
            plug_post_reload(state);
        }
#endif
        plug_update((webview_t)NULL);
#ifdef _WIN32
        Sleep(16);
#else
        usleep(16 * 1000);
#endif
    }

    plug_cleanup((webview_t)NULL);
    return 0;
}
