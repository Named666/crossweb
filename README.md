# Crossweb (Based on Musializer)

> [!NOTE]
> This project is a work-in-progress. It is not a production-ready alternative to Tauri, React Native, or Electron. Supported, incomplete, and unverified targets are listed in [docs/READINESS.md](docs/READINESS.md).

**CrossWeb** is a lightweight, C-based framework for creating cross-platform desktop and mobile applications using web technologies for the user interface. It combines a native C backend with a webview container and exposes native plugins to JavaScript only when a command is listed in `crossweb.permissions`.

The entire build process is managed by a custom, dependency-free build system written in C using `nob.h`.

---

## Features

-   **Targets:** Linux (system WebKitGTK), Windows (installed WebView2), and Android (`android.webkit.WebView`). macOS, iOS, and OpenBSD are deferred. See [docs/READINESS.md](docs/READINESS.md).
-   **UI:** Vanilla JS/HTML/CSS in `assets/`. No bundled Chromium.
-   **IPC bridge:** JavaScript messages are framed and dispatched to `plugin.command` handlers. Handlers do not run unless that command is granted.
-   **Plugins in tree:** `app` (granted `ping`), `fs` (jailed, not granted), and `keystore` (not granted).
-   **Hot reload:** Opt in with `nob config hotreload`. Automatic relaunch exists for the Windows host only.
-   **Build entrypoint:** `nob`, bootstrapped with `cc -o nob nob.c`.
-   **Android:** `android.webkit.WebView` template. `nob android` is still a Windows `cmd.exe` wrapper and was not built here. Release signing is not configured.

## Prerequisites

-   **C Compiler:** A C compiler like GCC, Clang, or MSVC.
-   **Node.js, npm & Vite:** Required for managing the web frontend.
-   **Android SDK & NDK:** Required only for building and running the Android version. Ensure your `ANDROID_HOME` environment variable is set.
-   **Gradle:** For building the Android project.

## Build System (`nob`)

The project is built using `nob`, a custom tool. To get started, bootstrap it by compiling `nob.c` once:

```sh
# On Windows (using GCC)
gcc nob.c -o nob.exe

# On Linux/macOS
cc -o nob nob.c
```

Once bootstrapped, you can use `nob.exe` (or `./nob`) for all project tasks. It will automatically re-build itself if `nob.c` changes.

## Commands

The `nob` command-line interface provides a complete workflow for development and building.

### General Project Commands

| Command           | Description                                                                     |
| ----------------- | ------------------------------------------------------------------------------- |
| **`nob init`**    | Initializes a new Vite-based web project in the `web/` directory. Run this first. |
| **`nob dev`**     | Starts the Vite development server for the web UI.                              |
| **`nob build`**   | Runs the Vite build into `web/dist/` when `web/` exists. Windows `cmd.exe` only today. |
| **`nob config`**  | Rewrites `build/config.h` from defaults plus the flags named on that command. It does not merge with the previous file. Flags: `hotreload`, `unbundle`, `webview-debug`, and `target` plus one of `win64-gcc`, `linux`, `macos`, `openbsd`, `android`, `ios`. |
| **`./nob`**       | Compiles the native host selected in `build/config.h`. A missing config enables Linux or Windows. The Linux host links system WebKitGTK and opens bundled `assets/`. |
| **`bash scripts/check.sh`** | Runs the trust-boundary tests and the Linux WebView smoke under Xvfb. |

### Android Commands

| Command                      | Description                                                                                          |
| ---------------------------- | ---------------------------------------------------------------------------------------------------- |
| **`nob android init`**       | Generates an Android project in the `android/` directory from a template.                            |
| **`nob android dev`**        | Runs the app on a connected Android device/emulator, using the live Vite server for the web UI.        |
| **`nob android build`**      | Windows-only wrapper that bundles web assets, C plugins, and Kotlin into an APK. Release signing in the template uses the debug keystore. |
| **`nob android run`**        | Installs and launches the release APK on a connected device/emulator.                                  |
| **`nob android install`**    | Installs the release APK without launching it.                                                       |

## Getting Started

1.  **Bootstrap the build system:**
    ```sh
    # Linux / macOS
    cc -o nob nob.c

    # Windows (GCC)
    gcc nob.c -o nob.exe
    ```
    Then run `bash scripts/check.sh` on Linux. That runs the trust-boundary tests and a WebKitGTK smoke test under Xvfb. `./nob` compiles the host selected in `build/config.h`.

    `app.ping` is granted. `fs.read`, `fs.write`, and keystore commands stay denied. If fs is granted later, reads and writes are limited to the app data directory and symlinks are rejected.

2.  **Initialize the web frontend:**
    ```sh
    ./nob init
    ```
    The shell UI checked into `assets/` is vanilla JS, HTML, and CSS. `nob init` is optional Vite scaffolding and still invokes `cmd.exe`. `scripts/check.sh` does not run it.

3.  **Build the native host:**
    ```sh
    ./nob
    ```
    `./build/crossweb` opens bundled `assets/` in system WebKitGTK. `./build/crossweb --smoke` checks granted `app.ping` and denied `fs.read`, then exits. `CROSSWEB_DEV_URL` accepts only localhost or `127.0.0.1` with a port. Remote debugging stays off unless `CROSSWEB_WEBVIEW_DEBUG=1`.

    ```sh
    sudo apt install pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev xvfb
    ```

    Windows needs `WEBVIEW2_INCLUDE` and `WEBVIEW2_LIB` from the WebView2 SDK and the installed Evergreen runtime. It was not compiled in this environment. Android uses `android.webkit.WebView` and was not built here. macOS, iOS, and OpenBSD are deferred.

4.  **Build and run for Android:**
    - Initialize the Android project structure:
      ```sh
      ./nob.exe android init --package com.yourcompany.yourapp --app-name "Your App"
      ```
    - Build an unsigned release APK (not a signed release):
      ```sh
      ./nob android build
      ```
    - Run the app on an emulator or device:
      ```sh
      ./nob.exe android run
      ```

## Architecture Overview

-   **`nob.c`**: The entry point for the stage-1 build system. It handles CLI commands and can compile a stage-2 builder for platform-specific tasks.
-   **`src/`**: Contains the core C source code for the native backend.
    -   `host_linux.c`: Linux shell. It uses the distro WebKitGTK WebView and bundled `assets/`.
    -   `win/host.cpp`: Windows adapter for the installed WebView2 runtime. The C99 core stays in C.
    -   `plug.c` / `plug.h`: The core plugin and IPC message routing system.
    -   `ipc.c` / `ipc.h`: Handles the low-level message passing between JS and C.
    -   `plugins/`: Home for native plugins like `fs` and `keystore`.
-   **`src_build/`**: The source code for the build system itself. It's compiled by `nob.c`.
-   **`web/`**: The source code for the web-based UI, typically a Vite project.
-   **`android/`**: The generated Android project.
-   **`templates/`**: Contains templates for generating new projects (e.g., the Android project structure).

## Plugin Development

Plugins are the primary way to extend the native capabilities of the application.

1.  **Create a Plugin:** Add a new directory in `src/plugins/`. For example, `src/plugins/myplugin/`.
2.  **Implement the Plugin:** A plugin is a `.c` file that defines and registers a `Plugin` struct. It includes functions for initialization, command invocation, and cleanup.
3.  **Expose to JavaScript:** Use the `invokeNative(command, payload)` function from `src/plugins/ipc/ipc.js` in your frontend code to call your plugin's commands. The `command` string is typically formatted as `"pluginName.functionName"`.

The framework handles routing the call to the correct C function, passing the payload, and returning the result asynchronously to JavaScript.