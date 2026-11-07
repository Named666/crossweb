#!/usr/bin/env bash
# Portable tests plus the Linux system-WebKitGTK shell under Xvfb.
set -eu
cd "$(dirname "$0")/.."

mkdir -p build

cc -std=c99 -Wall -Wextra -Werror -D_XOPEN_SOURCE=700 -I src -o build/test_core \
    tests/test_core.c src/plug.c src/ipc.c
./build/test_core

cc -std=c99 -Wall -Wextra -Werror -D_XOPEN_SOURCE=700 -I src -o build/test_bounds \
    tests/test_bounds.c src/origin.c src/assets_map.c src/host_config.c src/safe_open.c src/plug.c
./build/test_bounds

cc -std=c99 -Wall -Wextra -Werror -D_XOPEN_SOURCE=700 -I src -o build/test_fs \
    tests/test_fs.c src/plugins/fs/path.c src/plugins/fs/error.c src/safe_open.c
./build/test_fs

if ! pkg-config --exists webkit2gtk-4.1 && ! pkg-config --exists webkit2gtk-4.0; then
    echo "WebKitGTK development files are required." >&2
    echo "sudo apt install pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev xvfb" >&2
    exit 1
fi
if ! command -v xvfb-run >/dev/null; then
    echo "xvfb-run is required to exercise the Linux WebView without a physical display." >&2
    exit 1
fi

rm -f nob build/config.h build/config_logger.c build/nob_stage2 build/crossweb build/libplug.so
cc -Wall -Wextra -Werror -o nob nob.c

./nob help >/tmp/crossweb-nob-help.txt
if ./nob config target macos >/tmp/crossweb-nob-deferred.txt 2>&1; then
    echo "expected deferred macOS target to fail" >&2
    exit 1
fi
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
if ! ldd build/crossweb | grep -E 'libwebkit2gtk-4\.[01]'; then
    echo "linux host is not linked to system WebKitGTK" >&2
    ldd build/crossweb >&2
    exit 1
fi
if ldd build/crossweb | grep -Ei 'chrome|electron|cef'; then
    echo "linux host links a bundled browser engine" >&2
    exit 1
fi

GDK_BACKEND=x11
WEBKIT_DISABLE_COMPOSITING_MODE=1
LIBGL_ALWAYS_SOFTWARE=1
export GDK_BACKEND WEBKIT_DISABLE_COMPOSITING_MODE LIBGL_ALWAYS_SOFTWARE

set +e
timeout 20 xvfb-run -a -s '-screen 0 1280x800x24' ./build/crossweb --smoke >build/linux-smoke.log 2>&1
status=$?
set -e
if [ "$status" -ne 0 ]; then
    echo "linux WebView smoke failed with status $status" >&2
    cat build/linux-smoke.log >&2
    exit 1
fi
grep -q "smoke: title=crossweb-smoke-ok" build/linux-smoke.log
grep -q "fs_entered=0" build/linux-smoke.log
grep -q "ping_entered=1" build/linux-smoke.log
test -s build/linux-smoke.png

set +e
CROSSWEB_DEV_URL='http://evil.test:5173' timeout 10 xvfb-run -a ./build/crossweb --smoke >build/linux-bad-dev.log 2>&1
status=$?
set -e
if [ "$status" -eq 0 ]; then
    echo "rejected dev URL was accepted" >&2
    exit 1
fi
grep -q "dev URL is not an explicit localhost origin" build/linux-bad-dev.log

echo "check: trust-boundary tests passed; Linux WebKitGTK smoke passed under Xvfb"
