#define CROSSWEB_TARGET_NAME "android"

#include <ctype.h>
#include "common.c"
#include "config.c"
#include "plugins.c"
#include "android_build.c"

bool build_crossweb(void)
{
    return android_build_crossweb();
}

bool build_dist(void)
{
    return android_build_dist();
}
