# Crossweb (Based on Musializer)

> [!NOTE]
> This project is a work-in-progress. It serves as both a demonstration application (Musializer) and a reference for the underlying **CrossWeb** framework.

**CrossWeb** is a lightweight, C-based framework for creating cross-platform desktop and mobile applications using modern web technologies for the user interface. It combines a native C backend with a webview container, offering direct access to native capabilities through a flexible plugin system.

The entire build process is managed by a custom, dependency-free build system written in C using `nob.h`.

---

## Features

-   **Cross-Platform:** Targets Windows (GCC, MSVC, MinGW), macOS, Linux, and Android from a single C codebase.
-   **Hybrid Architecture:** Leverage web technologies (like Vite, React, Vue, etc.) for the UI and C for performance-critical backend logic.
-   **Native IPC Bridge:** Secure and efficient communication between the JavaScript frontend and the C backend.
-   **Extensible Plugin System:** Add native capabilities (like filesystem access, secure storage) that can be called from JavaScript.
-   **Native Hot-Reloading:** Reload C plugin code on the fly without restarting the application (for desktop development).
-   **Unified Build System:** A single, simple set of commands to manage, build, and run your project across all supported platforms.
-   **Android Integration:** Robust support for building, running, and debugging on Android, including automatic JNI bridge generation and plugin integration.

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
| **`nob build`**   | Builds the production-ready web assets in `web/dist/`.                          |
| **`nob config`**  | Modifies the build configuration (`build/config.h`).                            |
| **`./nob`**       | Compiles the native desktop application for the host OS.                        |

### Android Commands

| Command                      | Description                                                                                          |
| ---------------------------- | ---------------------------------------------------------------------------------------------------- |
| **`nob android init`**       | Generates an Android project in the `android/` directory from a template.                            |
| **`nob android dev`**        | Runs the app on a connected Android device/emulator, using the live Vite server for the web UI.        |
| **`nob android build`**      | Compiles and bundles the web assets, C plugins, and Kotlin code into a release-signed APK.             |
| **`nob android run`**        | Installs and launches the release APK on a connected device/emulator.                                  |
| **`nob android install`**    | Installs the release APK without launching it.                                                       |

## Getting Started

1.  **Bootstrap the build system:**
    ```sh
    gcc nob.c -o nob.exe
    ```

2.  **Initialize the web frontend:**
    ```sh
    ./nob.exe init
    ```
    Follow the prompts from Vite to set up your preferred web framework (e.g., React, Vue).

3.  **Build and run for Desktop:**
    - To enable hot-reloading for native C code:
      ```sh
      ./nob.exe config hotreload
      ```
    - Compile the native host:
      ```sh
      ./nob.exe
      ```
    - Run the application:
      ```sh
      ./build/crossweb.exe
      ```

4.  **Build and run for Android:**
    - Initialize the Android project structure:
      ```sh
      ./nob.exe android init --package com.yourcompany.yourapp --app-name "Your App"
      ```
    - Build a release APK:
      ```sh
      ./nob.exe android build
      ```
    - Run the app on an emulator or device:
      ```sh
      ./nob.exe android run
      ```

## Architecture Overview

-   **`nob.c`**: The entry point for the stage-1 build system. It handles CLI commands and can compile a stage-2 builder for platform-specific tasks.
-   **`src/`**: Contains the core C source code for the native backend.
    -   `webview.c`: The main entry point for the native application host.
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