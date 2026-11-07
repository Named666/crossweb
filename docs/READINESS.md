# Crossweb readiness

This repository is **not production-ready**. The notes below are the milestone-1 baseline: a deny-by-default native command gate, a POSIX `nob` bootstrap, and a Linux test/link check. They do not make the framework a viable Tauri, React Native, or Electron replacement.

## Matrix

| Area | State | What the tree actually contains |
| --- | --- | --- |
| IPC framing and `plugin.command` grants | **Supported** for the cases in `tests/test_core.c` | `plug_invoke` denies every command unless `crossweb.permissions` or `plug_grant` lists that exact pair. The committed grant file lists nothing. |
| Linux native binary | **Incomplete** | `./build/crossweb` links and stays running. It registers `fs` and `keystore`, then loops. It does not open a WebView. `ldd` shows `libc` and `libm` only. |
| Windows host | **Incomplete** | `src/webview.c` has a WebView2 loop, hardcoded to `http://localhost:5173`. `thirdparty/webview-c` is not in the repo. Remote debugging is compiled off unless `CROSSWEB_WEBVIEW_DEBUG` is 1. |
| macOS host | **Incomplete** | `src_build/nob_macos.c` passes `-framework Cocoa -framework WebKit`. The non-Windows `main` in `src/webview.c` does not call those frameworks. |
| Android | **Incomplete** | Kotlin/C templates and a JNI bridge exist. `nob android` shells out through `cmd.exe` and `xcopy`. Release signing uses the debug keystore. |
| iOS | **Incomplete** | `src_build/nob_ios.c` is empty. |
| OpenBSD | **Incomplete** | `src_build/nob_openbsd.c` still packages Musializer paths (`./build/musializer`, `./resources/`). |
| Web frontend and dev loop | **Incomplete** | No `web/` app is checked in. `nob init`, `nob dev`, and `nob build` call `npm` through `cmd.exe`. |
| Extra `<webview>` instances | **Incomplete** | No implementation. |
| Packaging and auto-update | **Incomplete** | No installer, notarization, or store packaging. |
| Plugins named only in `PLUGIN_SPEC.md` | **Incomplete** | `dialog`, `clipboard`, `http`, `notifications`, `geolocation`, `biometrics`, and `secure_storage` have no sources. `plug_load` still returns false. |
| macOS, iOS, Windows, and Android **runtimes** | **Unverified** | No build or launch command for those targets was run for this milestone. Status above is from reading the sources. |

"Supported" means the portable tests pass. It does not mean a platform app is shippable.

## Commands

From a POSIX checkout, compiler `cc` (this run used Ubuntu clang 18.1.3):

```sh
bash scripts/check.sh
```

That script:

1. Compiles and runs `tests/test_core.c` with `-std=c99 -Wall -Wextra -Werror`.
2. Bootstraps `nob` with `cc -Wall -Wextra -Werror -o nob nob.c`.
3. Checks `nob help`, rejects `nob config target nosuch`, and checks `nob config target win64-gcc` then `nob config target linux`.
4. Runs `./nob`, which links `./build/crossweb` with `-Wall -Wextra -Werror`.
5. Rejects the binary if `ldd` shows WebKit or GTK.
6. Starts `./build/crossweb` under `timeout 0.5`. The process must still be running (exit 124), print plugin init on stdout, and write no stderr.

`nob config` rewrites `build/config.h` from defaults plus the flags on that command. It does not merge with the previous file. A missing config enables the OS that compiled `nob`. Hot reload and WebView remote debugging are numeric `0` unless named: the sources test them with `#if`, not `#ifdef`.

Do not treat `./build/crossweb` as an app. Without `timeout` it loops.

These commands were **not** run:

- `nob init`, `nob dev`, `nob build`
- `nob android init|dev|build|run|install`
- Windows, macOS, iOS, or OpenBSD host builds
- An emulator, device, or GUI session
- The GitHub Actions workflow in `.github/workflows/core.yml` (the file is in the tree; this session did not wait on a GitHub run)

## Recorded results

Before these changes, on this machine:

- `cc -o nob nob.c` failed with `src_build/common.c: fatal error: 'direct.h' file not found`.
- `cc -std=c99 -o nob nob.c` also failed inside `thirdparty/nob.h` (`struct timespec`, `clock_gettime`, `PATH_MAX`). `nob.h` expects the compiler's default GNU dialect. The bootstrap command is `cc -o nob nob.c`, not `-std=c99`.
- There was no test binary and no CI workflow.
- `src_build/nob_linux.c` passed the text `` `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` `` as one argv element, and `src/webview.c` did not call GTK or WebKit.

After these changes, `bash scripts/check.sh` exited 0.

- `test_core: 391 checks, 0 failures`
- Generated `build/config.h` contained `#define CROSSWEB_HOTRELOAD 0`, `#define CROSSWEB_WEBVIEW_DEBUG 0`, and `#define CROSSWEB_TARGET_LINUX 1`. The Win64/macOS/Android/iOS/OpenBSD target lines stayed comments.
- Link line: `cc -Wall -Wextra -Werror -ggdb -I. -include build/config.h -o ./build/crossweb ./src/plug.c ./src/ipc.c ./src/webview.c` plus `src/plugins/fs/*.c`, `src/plugins/keystore/src/plugin.c`, and `-lm -ldl -lpthread`.
- `ldd ./build/crossweb` resolved `linux-vdso.so.1`, `libm.so.6`, `libc.so.6`, and `ld-linux-x86-64.so.2` only.
- `stdbuf -oL -eL timeout 0.5 ./build/crossweb` exited 124. Stdout was:

```text
Plugin registered: fs (v100)
Plugin registered: keystore (v100)
FS plugin initialized on linux
Keystore plugin initialized
```

Stderr was empty, which matches the committed `crossweb.permissions` loading with zero grants.

### Test limitations

- `tests/test_core.c` does not link `fs` or `keystore`. A grant for `fs.read` is checked as data; the test never calls `fopen`.
- The tests do not load a WebView or execute `src/plugins/ipc/ipc.js`.
- The Android template edits (trusted-page check, base64 `evaluateJavascript`) were not compiled.
- The 0.5s Linux launch shows process startup only. It is not a UI test and it does not prove a window can appear.
- Windows hot reload, WebView2, and the keystore WebAuthn path were not compiled or launched.

## Security defaults in this milestone

- Native commands are denied until a `plugin.command` line is granted. Web content cannot add grants.
- `plug_invoke` rejects malformed names, does not log payloads, and checks the grant before it looks up a plugin.
- IPC ids and commands must match `[A-Za-z0-9._:-]` and the existing length caps before they are queued. Response and event delivery use the same check so a quote cannot be interpolated into the Windows script string.
- Hot reload and WebView remote debugging default off.
- The Android template refuses `JavascriptInterface` calls unless the main-frame URL is the asset origin or a localhost/loopback dev URL, and it base64-encodes values passed into `evaluateJavascript`.

Grants are not a sandbox. Uncommenting `fs.read` or `fs.write` still allows any path the process can open. The filesystem JSON parser is a stub. Keystore commands still handle secrets with no extra confinement. Android's URL check is the main frame only; `addJavascriptInterface` remains visible to script in that page.

## Next milestones

### 1. One real desktop WebView shell

Acceptance:

- A WebView dependency is vendored or fetched by a command written in this document. Today `thirdparty/webview-c` is absent, and the Linux `main` does not call WebKit.
- The default page is bundled app content. `http://localhost:5173` is opt-in.
- Navigation outside the app origin cannot reach `plug_invoke`.
- A CI job for that host shows an unlisted command returns `{"ok":false,"error":"permission denied"}` without entering the plugin, and one granted test command round-trips a payload.
- `fs.read` and `fs.write` refuse paths outside an app data directory. A grant must not mean "any path `fopen` accepts."

### 2. Stop shipping privileged plugins as open `fopen` / secret helpers

Acceptance:

- `fs` path checks have tests for absolute paths, `..`, and links, and they fail closed.
- Keystore responses are covered by tests that do not require Windows Hello. Unsupported platforms keep returning an error and do not crash on a null callback.
- The per-plugin JSON files under `permissions/` are either enforced by the same gate or removed so they cannot be mistaken for policy.

### 3. One packaged target with the grant file inside the package

Acceptance:

- Android or one desktop OS produces an installable artifact without the debug signing config.
- The running app loads grants from the package, not from whatever the current working directory happens to contain.
- `nob android` or the desktop package command runs without `cmd.exe` when the documented host is not Windows.
- That package is launched once and the milestone-1 permission denial is reproduced there.

Until those pass, do not describe Crossweb as production-ready.
