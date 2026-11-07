#!/usr/bin/env bash
# Reproducible baseline for the portable core and the Linux native runtime.
# Does not launch ./build/crossweb: that binary has no GUI and loops.
set -eu
cd "$(dirname "$0")/.."

mkdir -p build

cc -std=c99 -Wall -Wextra -Werror -I src -o build/test_core \
    tests/test_core.c src/plug.c src/ipc.c
./build/test_core

rm -f nob build/config.h build/config_logger.c build/nob_stage2 build/crossweb build/libplug.so
cc -Wall -Wextra -Werror -o nob nob.c

./nob help >/tmp/crossweb-nob-help.txt

if ./nob config target nosuch >/tmp/crossweb-nob-bad-target.txt 2>&1; then
    echo "expected unknown target to fail" >&2
    exit 1
fi

./nob config target win64-gcc >/tmp/crossweb-nob-win64.txt
grep -E '^#define CROSSWEB_TARGET_WIN64_GCC 1( |$)' build/config.h >/dev/null
grep -E '^#define CROSSWEB_HOTRELOAD 0( |$)' build/config.h >/dev/null
grep -E '^#define CROSSWEB_WEBVIEW_DEBUG 0( |$)' build/config.h >/dev/null

./nob config target linux >/tmp/crossweb-nob-linux.txt
grep -E '^#define CROSSWEB_TARGET_LINUX 1( |$)' build/config.h >/dev/null
if grep -E '^#define CROSSWEB_TARGET_WIN64_GCC 1( |$)' build/config.h >/dev/null; then
    echo "win64 target still enabled after switching to linux" >&2
    exit 1
fi

./nob

test -x build/crossweb
if ldd build/crossweb | grep -Ei 'webkit|gtk'; then
    echo "linux runtime unexpectedly links a GUI toolkit" >&2
    exit 1
fi

# The Linux binary is a native loop, not a window. It must stay running and
# must initialize plugins without writing errors. This is not a GUI smoke test.
if ! command -v timeout >/dev/null || ! command -v stdbuf >/dev/null; then
    echo "timeout and stdbuf are required to observe the linux runtime" >&2
    exit 1
fi
set +e
stdbuf -oL -eL timeout 0.5 ./build/crossweb >build/linux-runtime-start.txt 2>build/linux-runtime-start.err
status=$?
set -e
if [ "$status" -ne 124 ]; then
    echo "linux runtime exited early with status $status" >&2
    cat build/linux-runtime-start.txt >&2
    cat build/linux-runtime-start.err >&2
    exit 1
fi
grep -q "FS plugin initialized on linux" build/linux-runtime-start.txt
grep -q "Keystore plugin initialized" build/linux-runtime-start.txt
if [ -s build/linux-runtime-start.err ]; then
    echo "linux runtime wrote unexpected stderr" >&2
    cat build/linux-runtime-start.err >&2
    exit 1
fi

echo "check: core tests passed; linux runtime linked and stayed up; no GUI toolkit"
