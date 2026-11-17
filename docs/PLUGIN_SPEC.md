# Crossweb Plugin System Specification

## Summary

Crossweb plugins are optional native extensions that let the web UI call privileged functionality through the core runtime. A plugin can include JavaScript bindings, C implementation, Kotlin or Java code for Android, and Swift support is planned for future iOS work. The design is intentionally minimal at the core: the host process provides windowing, webview hosting, IPC, and event routing, while plugins add the functionality that applications actually need.

## Current Implementation Status

The project already has the foundations for this model, but the runtime is still in an early stage.

### What the code currently does

- A C-based plugin registry and lifecycle API exist in `src/plug.h` and `src/plug.c`.
- Plugins are registered automatically through `PLUG_REGISTER()` and exposed through `plug_init()`, `plug_invoke()`, `plug_emit()`, and `plug_cleanup()`.
- IPC between the webview and the native core is implemented in `src/ipc.c` and routed through the core runtime.
- Desktop builds use a native WebView host, while Android support connects the WebView to C via JNI and can package plugin sources into the mobile build.
- The current build flow already understands plugin directories under `src/plugins/` and can copy plugin assets into Android builds.
- Example plugins such as `fs` and `keystore` demonstrate the basic registration and command-dispatch pattern.

### What is partially implemented

- Plugin lifecycle hooks: `init`, `invoke`, `event`, `cleanup`
- Command dispatch using the `plugin.command` convention
- JSON-based payloads and responses
- Desktop/WebView bridge via the host process
- Android JNI bridge and plugin packaging

### What is planned next

- Capability-based permission enforcement
- Dynamic plugin loading and runtime plugin discovery
- Formal plugin discovery from `node_modules`
- iOS/Swift bridge and packaging
- A fully scripted CLI workflow for plugin creation and mobile add-ons
- Hot-reloading of plugins during development, including live updates while the app is running

## Architectural Model

Crossweb follows a Tauri-style architecture with a privileged core process and UI processes that render web content in system WebViews. The core owns the OS-facing responsibilities such as windows, tray menus, notifications, and IPC policy. Plugins live inside that runtime and are invoked by commands coming from JavaScript through the WebView bridge.

This keeps the trusted computing base small and ensures that sensitive operations are executed in one place rather than directly from the webview.

## Plugin Contract

A plugin is composed of one or more of the following pieces:

- JavaScript API bindings for the guest app
- C code for the core runtime and privileged operations
- Kotlin or Java code for Android-specific implementation
- Swift or Objective-C code for future iOS support
- Optional permission declarations and metadata

### Lifecycle hooks

Each plugin exposes a stable interface:

- `init(...)`: called when the host runtime starts
- `invoke(command, payload, respond)`: handles commands such as `fs.read` or `dialog.open`
- `event(event, data)`: emits or forwards application events back to the UI layer
- `cleanup()`: runs during shutdown or teardown

### Command model

Plugin commands should be routed through the core as structured requests:

- Command format: `plugin.command`
- Payload: JSON object
- Response: JSON object with either a result or an error

The current runtime already uses this pattern in its IPC flow and dispatch layer.

## Plugin Layout

For local app development, plugins should live under the application tree. A concrete layout example is available in [docs/examples/plugin-layout.txt](examples/plugin-layout.txt).

This is the default authoring layout because it keeps the plugin source close to the app and makes debugging straightforward.

For packaged/distributed plugins, Crossweb should also support discovery from npm-style packages using names such as:

- `crossweb-plugin-<name>`
- `@scope-name/crossweb-plugin-<name>`

This should be treated as an installation/distribution mechanism rather than the default development workflow.

## Native Implementation Details

### C runtime

The C implementation runs in the core runtime and can:

- access privileged APIs
- create or manage OS resources
- expose a narrow command interface to the web layer
- emit events back to the UI

### Mobile code

Plugins may also include mobile-specific implementation:

- Android: Kotlin or Java code, invoked through JNI from the C bridge
- iOS: Swift or Objective-C, planned for a future bridge layer

The plugin template should make it easy to write a command in C that triggers a Kotlin or Swift implementation when the target platform needs it.

## Security Model

Plugins should not expose privileged operations directly to the WebView. Instead, they should be routed through the core process and checked against an allowlist of capabilities before execution. The long-term goal is a central permission model that associates each plugin command with the minimum required privilege.

Planned policy features:

- explicit capability declarations per plugin
- command-level allowlists
- per-process permission boundaries
- runtime enforcement in the core before plugin dispatch

## Build and Packaging Integration

The build system should:

- discover local plugins from `src/plugins/`
- include their C sources in the app build
- package mobile-specific source files for Android and later for iOS
- expose plugin JavaScript bindings to the frontend build
- optionally discover installed plugin packages from `node_modules`

## Future Development Goals

1. Implement a capability-based permission model
2. Add plugin discovery for npm-style packages
3. Add CLI scaffolding for plugin creation and mobile add-ons
4. Add a Swift bridge and iOS packaging path
5. Enable plugin hot-reloading during development and runtime so plugins can be updated on the fly
6. Improve developer ergonomics with templates, examples, tests, and live-update workflows

## Example Plugin Flow

A step-by-step flow example is available in [docs/examples/plugin-flow.txt](examples/plugin-flow.txt), and a sample command/response payload is shown in [docs/examples/plugin-command-model.json](examples/plugin-command-model.json).

This model keeps the plugin API simple while preserving a clear boundary between the web UI and the privileged native layer.