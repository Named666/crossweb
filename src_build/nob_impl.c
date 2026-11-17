// NOB implementation and helper glue for the hotreload-enabled crossweb build.
// This translation unit defines the NOB implementation (nob_log, cmd helpers, etc.)
// but does not include other src_build .c files. Those are compiled separately
// and linked together by the build script.

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "../thirdparty/nob.h"

// Nothing else here; implementations from src_build/common.c and
// src_build/plugins.c will be compiled/linked into the crossweb binary.
