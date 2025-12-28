#include <stdbool.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
// #define NOB_WARN_DEPRECATED
#include "../thirdparty/nob.h"
#include "../build/config.h"
#include "./configurer.c"

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

void log_available_subcommands(const char *program, Nob_Log_Level level)
{
    nob_log(level, "Usage: %s [subcommand]", program);
    nob_log(level, "Subcommands:");
    nob_log(level, "    build (default)");
    nob_log(level, "    dist");
    nob_log(level, "    help");
}

int main(int argc, char **argv)
{
    nob_log(NOB_INFO, "--- STAGE 2 ---");
    // log_config(NOB_INFO);
    nob_log(NOB_INFO, "---");

    const char *program = nob_shift_args(&argc, &argv);

    const char *subcommand = NULL;
    if (argc <= 0) {
        subcommand = "build";
    } else {
        subcommand = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcommand, "build") == 0) {
        if (!build_crossweb()) return 1;
    } else if (strcmp(subcommand, "dist") == 0) {
        nob_log(NOB_ERROR, "The `config` command does not exist anymore!");
        nob_log(NOB_ERROR, "Edit %s to configure the build!", CONFIG_PATH);
        return 1;
    } else if (strcmp(subcommand, "help") == 0) {
        log_available_subcommands(program, NOB_INFO);
    } else {
        nob_log(NOB_ERROR, "Unknown subcommand %s", subcommand);
        log_available_subcommands(program, NOB_ERROR);
    }
    // TODO: it would be nice to check for situations like building TARGET_WIN64_MSVC on Linux and report that it's not possible.
    return 0;
}
