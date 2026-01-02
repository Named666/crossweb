#include <stdbool.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
// #define NOB_WARN_DEPRECATED
#include "../thirdparty/nob.h"
#include "../build/config.h"

// Include helper implementation files required by platform-specific builders
#include "common.c"
#include "templates.c"
#include "plugins.c"

// @backcomp
#if defined(CROSSWEB_TARGET)
#error "We recently replaced a single CROSSWEB_TARGET macro with a bunch of CROSSWEB_TARGET_<TARGET> macros instead. Since CROSSWEB_TARGET is still defined your ./build/ is probably old. Please remove it so ./build/config.h gets regenerated."
#endif // CROSSWEB_TARGET

#if defined(CROSSWEB_TARGET_LINUX)
#include "nob_linux.c"
#elif defined(CROSSWEB_TARGET_MACOS)
#include "nob_macos.c"
#elif defined(CROSSWEB_TARGET_WIN64_MINGW)
#include "nob_win64_mingw.c"
#elif defined(CROSSWEB_TARGET_WIN64_MSVC)
#include "nob_win64_msvc.c"
#elif defined(CROSSWEB_TARGET_WIN64_GCC)
#include "nob_win64_gcc.c"
#elif defined(CROSSWEB_TARGET_OPENBSD)
#include "nob_openbsd.c"
#elif defined(CROSSWEB_TARGET_ANDROID)
#include "nob_android.c"
#elif defined(CROSSWEB_TARGET_IOS)
#include "nob_ios.c"
#else
#error "No Crossweb Target is defined. Check your ./build/config.h."
#endif // CROSSWEB_TARGET

int main(int argc, char **argv)
{
    nob_log(NOB_INFO, "--- STAGE 2 ---");
    // log_config(NOB_INFO);
    if (!build_dist()) return 1;
    // TODO: it would be nice to check for situations like building TARGET_WIN64_MSVC on Linux and report that it's not possible.
    return 0;
}
