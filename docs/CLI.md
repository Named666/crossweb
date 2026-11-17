# Crossweb CLI Specification

## Summary

The Crossweb CLI is implemented as a single C-based entry point in `nob.c` and uses a two-stage build pattern. The first stage regenerates or re-executes the build helper, and the second stage performs the actual app or platform build steps. The current CLI already supports app initialization and platform-specific development flows for Windows and Android. Plugin scaffolding is the next major CLI feature and is still planned rather than fully implemented.

## Current Implementation Status

### What the CLI currently does

- `nob init` to bootstrap the web frontend project
- `nob config` as a deprecated compatibility command; build flags are now supplied by the CLI
- `nob win dev` to start Vite and launch a Windows desktop build in development mode
- `nob android init` / `nob android dev` / `nob android build [dev]` / `nob android run` / `nob android install`
- Template copying, asset packaging, and Android build file generation

### What to do next

- `nob win build` / `nob win build dev` default for Windows release and `dev` for debug builds
- `nob win run` quickly builds and launches the Windows app
- `nob win run dev` quickly builds and launches the Windows app in development mode (rename `nob win dev` to `nob win run dev`)
-  Complete `nob android build` / `nob android build dev` default for Android release and `dev` for debug builds
- `nob android run` to quickly build and launch the Android app
- `nob android run dev` to quickly build and launch the Android app in development mode
- `plugin new` scaffolding for plugin projects
- `plugin android add` and `plugin ios add` for mobile support
- plugin discovery from `node_modules`
- stronger permission and capability integration into the build/runtime flow
- plugin hot-reload support during development, including live updates for plugin code and assets

## Application Commands

### `nob init`

Bootstraps the web frontend project in the local app workspace.

Example:

```text
nob init
```

### `nob config`

Generates a default configuration file and enables feature flags for the current project.

### `nob win run dev`

Starts the Vite dev server and launches the Windows desktop host.

Build-producing commands enable hot reload by default. Disable it for a command with:

```text
nob win dev -h=False
nob android build -h=False
```

The supported build targets are Windows GCC, Linux, macOS, OpenBSD, and Android. iOS is not exposed until its builder is implemented.

### `nob android init`

Creates an Android application project from the bundled template.

Flags:

- `--package <package>`
- `--app-name <name>`

Example:

```text
nob android init --package com.acme.app --app-name "Acme"
```

### `nob android run dev`

Starts the Vite dev server, builds a debug APK wired to that server, then installs and launches it on the emulator.

### `nob android build`

Builds a release APK and packages frontend assets into the application bundle.

### `nob android build dev`

Builds a debug APK that loads the web UI from the local Vite dev server at `10.0.2.2:5173`.

### `nob android run` / `nob android install`

Runs the app on an emulator or installs the produced APK on a device.

## Plugin CLI Workflow (Planned)

The intended plugin workflow should be split between local app development and published plugin packages.

### 1. Local plugin scaffolding

For app-local development, new plugins should be generated under the application tree:

```text
src/plugins/<plugin-name>/
```

This keeps the plugin source close to the app and makes it easy to edit, test, and debug.

### 2. Published plugin package scaffolding

For distribution, the CLI should scaffold a package directory named:

```text
crossweb-plugin-<plugin-name>
```

The JavaScript package name should follow the same convention:

```text
crossweb-plugin-<plugin-name>
```

and scoped packages should follow:

```text
@scope-name/crossweb-plugin-<plugin-name>
```

### 3. Planned plugin commands

The CLI should expose commands such as:

```text
plugin new <name>
plugin new <name> --no-api
plugin new <name> --android
plugin new <name> --ios
plugin android add
plugin ios add
```

Behavior:

- `plugin new` creates the plugin project skeleton
- `--no-api` skips JavaScript API scaffolding
- `--android` adds an Android library project using Kotlin or Java
- `--ios` adds an iOS package or bridge scaffold for future Swift support
- `plugin android add` and `plugin ios add` can upgrade an existing plugin project with mobile support

The generated plugin template should include:

- a native C entry point for the plugin
- a JS wrapper in `js/`
- Android source under `android/` when requested
- iOS source under `ios/` when requested
- a minimal example command showing how C can trigger the mobile implementation

## Plugin Discovery Model

The CLI and build system should support two discovery paths:

1. Local development path
   - scan `src/plugins/` for app-local plugins

2. Package/distribution path
   - scan installed packages in `node_modules/` for names matching `crossweb-plugin-*` or `@scope/crossweb-plugin-*`

The local path should remain the default authoring workflow. npm-style discovery should be optional and should not replace local plugin development.

## Current Gaps and Design Notes

- Plugin scaffolding is not implemented yet.
- The Android flow assumes the Android SDK and toolchain are available on the host machine.
- The CLI currently edits some generated source files in place for dev/build switching; this should eventually be replaced by templated or reversible patching.
- Permission enforcement is not yet part of the CLI flow and should be integrated with config generation and runtime permission checks.

## Future Roadmap Alignment

The CLI should evolve toward these goals:

- first-class plugin authoring commands
- better platform subcommands such as `build windows`, `build macos`, `build linux`, and `build ios`
- tighter integration with plugin capability declarations
- improved packaging and release flows for desktop and mobile targets

## Files of Interest

- `nob.c` — main CLI entry and subcommand dispatch
- `src_build/` — helper modules for templating, config, and platform build logic
- `templates/android/` — Android project template used by the current CLI
- `docs/PLUGIN_SPEC.md` — plugin runtime and packaging design
