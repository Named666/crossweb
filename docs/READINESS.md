# Crossweb readiness

This repository is **not production-ready**. Milestone 2 adds a Linux system-WebView shell and the shared trust rules that Windows and Android hosts are written to follow. It does not make those other hosts verified.

The browser engine is the platform's own WebView:

| Target | WebView | Engine packaging |
| --- | --- | --- |
| Linux | WebKitGTK 4.1 (`webkit2gtk-4.1`), falling back to 4.0 | Distro packages. There is no single universal Linux WebView; WebKitGTK is the native GTK backend. |
| Windows | Microsoft Edge WebView2 | Installed Evergreen runtime. The SDK supplies headers and `WebView2LoaderStatic` only. No Fixed Version runtime is vendored. |
| Android | `android.webkit.WebView` | The device's system WebView. |

No Chromium, CEF, or Electron binary is linked or downloaded.

## Matrix

| Area | State | Evidence |
| --- | --- | --- |
| Command grants | **Supported** | `plug_invoke` still denies exact `plugin.command` pairs that are not listed. `crossweb.permissions` grants only `app.ping`. |
| Origin policy | **Supported** as a pure function | `tests/test_bounds.c`. Trusted origins are `crossweb://app`, the Windows virtual host `https://crossweb.localhost`, the Android asset host, and an explicit localhost dev origin. |
| Bundled asset paths | **Supported** | Traversal, `%2e%2e`, unknown suffixes, and symlinks are rejected. |
| `fs` app-data jail | **Supported** on Linux | Reads and writes use `openat` with `O_NOFOLLOW` under the root from `fs_set_root`. Absolute paths and `..` fail. Symlinks fail. |
| Linux WebKitGTK shell | **Verified runtime** under Xvfb | `bash scripts/check.sh` launched `./build/crossweb --smoke`. The page showed the ping result and the denied `fs.read`. `fs_commands_entered` stayed 0 and `app_ping_entered` was 1. |
| Linux dev URL rejection | **Verified** | `CROSSWEB_DEV_URL=http://evil.test:5173 ./build/crossweb --smoke` exited 1 before loading a page. |
| Windows WebView2 host | **Unverified** | `src/win/host.cpp` is the C++ adapter over the C99 core. This environment has no MinGW g++, no WebView2 headers, and no Evergreen runtime. The file was not compiled. |
| Android `android.webkit.WebView` template | **Unverified** | Template and JNI were edited. This environment has no Android SDK, `sdkmanager`, or Gradle. Nothing was compiled, installed, or launched. |
| macOS, iOS, OpenBSD | **Deferred** | `nob config target` rejects them. Their build files return an error. |
| Extra `<webview>` instances, installers, signing | **Incomplete** | Not in this milestone. |

## What the Linux shell does

Production content is `assets/index.html`, `assets/app.css`, and `assets/app.js`, served as `crossweb://app/` from the UI process. The page is not `http://localhost:5173`.

`CROSSWEB_DEV_URL` is optional and must be `http` or `https` to `localhost` or `127.0.0.1` with a port. Remote debugging is off unless `CROSSWEB_WEBVIEW_DEBUG=1` or the compile-time flag is set.

Navigation and subresource responses whose origin is not the app origin or that dev origin are ignored. The bridge script is injected into the top frame only. WebKit's sandbox is enabled. A GLib warning while releasing the sandboxed web-process name was printed on shutdown; the smoke process still exited 0.

`app.ping` echoes a short nonce. `fs.read` is not granted, so `fs_invoke` is not entered. If it is granted later, `fs_read_file` still refuses paths outside the app data directory (`$XDG_DATA_HOME/crossweb` or `~/.local/share/crossweb`).

## Commands

Linux packages used here (Ubuntu 24.04, WebKitGTK 2.52.6):

```sh
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  -o Dpkg::Options::=--force-confdef \
  -o Dpkg::Options::=--force-confold \
  pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev xvfb
bash scripts/check.sh
```

`scripts/check.sh` compiles `test_core`, `test_bounds`, and `test_fs` with `-std=c99 -Wall -Wextra -Werror`, bootstraps `nob`, links `./build/crossweb` to `libwebkit2gtk-4.1`, and runs:

```sh
GDK_BACKEND=x11 WEBKIT_DISABLE_COMPOSITING_MODE=1 LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a -s '-screen 0 1280x800x24' ./build/crossweb --smoke
```

Manual run from the repo root, with a display: `./build/crossweb`.

Windows, when a toolchain exists:

```sh
# Headers and loader from the NuGet package Microsoft.Web.WebView2, build/native.
# Do not download the Fixed Version browser.
export WEBVIEW2_INCLUDE=/path/to/build/native/include
export WEBVIEW2_LIB=/path/to/build/native/x64
./nob config target win64-gcc
./nob
```

The machine also needs the installed Evergreen WebView2 runtime. This session did not run those commands.

Android, when an SDK exists:

```sh
export ANDROID_HOME=/path/to/sdk
./nob config target android
./nob android init --package com.example.crossweb --app-name Crossweb
./nob android build
```

`nob android` still shells out through `cmd.exe` for Gradle. `assembleRelease` is unsigned. A debug keystore is not configured as the release credential. Debug builds may cleartext-load `localhost` and `127.0.0.1` only through `debug-overrides`. Release cleartext is denied. `dev-url.txt` in the APK assets is the only dev-server switch, and a rejected file leaves the app on bundled content. None of this was executed here.

## Recorded results

Compiler: Ubuntu clang 18.1.3. WebKitGTK 2.52.6.

`bash scripts/check.sh` exited 0.

- `test_core: 392 checks, 0 failures`
- `test_bounds: 63 checks, 0 failures`
- `test_fs: 34 checks, 0 failures`
- Link used `pkg-config webkit2gtk-4.1`. `ldd` resolved `libwebkit2gtk-4.1.so.0` and `libgtk-3.so.0`. It did not resolve Chromium, CEF, or Electron.
- Smoke log contained `plug_invoke: denied fs.read` and `smoke: title=crossweb-smoke-ok fs_entered=0 ping_entered=1`.
- Screenshot: `build/linux-smoke.png` (also copied for review). The page text was "Native ping returned ready1. fs.read was denied."
- `CROSSWEB_DEV_URL=http://evil.test:5173` exited 1 with `dev URL is not an explicit localhost origin`.
- Shutdown also printed a GLib-GIO warning about releasing the sandboxed WebProcess bus name. That is not a failed smoke.

Not run, and not claimed as runtime validation:

- Windows compile or launch. `x86_64-w64-mingw32-g++` is absent. `WEBVIEW2_INCLUDE` was not set.
- Android compile, install, or launch. `ANDROID_HOME` is unset. `gradle` and `sdkmanager` are absent.
- `nob init`, `nob dev`, `nob build`, and `nob android`.
- A physical display, device, or emulator. The Linux check used Xvfb.

## Limits that are still true

- The fs jail is not a defense against a concurrent process swapping directories for symlinks after the open check. Symlinks are rejected, and the Linux open uses `O_NOFOLLOW`.
- Android navigation blocking applies to main-frame and subframe navigations. `addJavascriptInterface` is still exposed to script in any document that actually loads. The main-frame URL check in `Ipc.kt` does not identify the calling frame.
- WebView2 resource blocking and navigation cancellation are written in `src/win/host.cpp` and were not compiled.
- Keystore commands remain unimplemented as a safe cross-platform store. They are not granted.
- The Windows hot-reload DLL path is not part of the new WebView2 host.

## Next milestone

Make one non-Linux host match the Linux smoke.

Acceptance:

- On a Windows machine, `./nob` with `WEBVIEW2_INCLUDE` and `WEBVIEW2_LIB` produces `build/crossweb.exe`, `ldd`/module list shows `WebView2Loader` and not a Fixed Version runtime directory, and `--smoke` prints `fs_entered=0` and `ping_entered=1` against `https://crossweb.localhost`.
- Or, on an Android SDK machine, `assembleDebug` installs an APK whose WebView is `android.webkit.WebView`, bundled assets load, `app.ping` round-trips, `fs.read` is denied, and a navigation off `https://appassets.androidplatform.net` does not run. Release stays unsigned until a real keystore is configured.
- The other of those two stays listed as unverified until the same smoke exists.
- CI for the host that was actually executed is required. Xvfb remains the Linux check.
