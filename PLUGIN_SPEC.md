# Plugin System Implementation Specification Plan

Plugin System

Crossweb supports dynamically loadable native plugins written in C.

Plugin Characteristics

- Stable ABI
- Explicit capability declaration
- Platform-independent interface

Plugins register commands and events with the core runtime at load time. Permission checks are enforced centrally.

## Overview
This document outlines the detailed implementation plan for the CrossWeb plugin system, enabling native functionality access from JavaScript in a cross-platform manner.

## 1. Plugin Architecture

### 1.1 ABI-Stable Interface
- **Location**: `src/plug.h`
- **Structure**: `struct Plugin` with fixed layout
  - `const char *name`: Plugin identifier
  - `int version`: Semantic version (e.g., 100 = 1.0.0)
  - Function pointers: `init`, `invoke`, `event`, `cleanup`
- **Stability**: No changes to struct layout across versions

### 1.2 Plugin Discovery and Loading
- **Directory**: `src/plugins/`
- **Structure**: Each plugin is a subdirectory with:
  - `lib.c`: Main plugin definition and re-exports
  - `plugin.h`: Header with extern Plugin declaration
  - `commands.c`: Command implementations
  - `models.h`: Shared data structures
  - `error.h`/`error.c`: Error handling
  - `desktop.c`: Desktop-specific implementations
  - `mobile.c`: Mobile-specific implementations
  - `android/`: Android-specific code (Kotlin/Java)
  - `ios/`: iOS-specific code
  - `guest-js/`: JavaScript API bindings
  - `permissions/`: Generated permission files

### 1.3 Core Plugins
- **fs**: File system operations (read, write, list, etc.)
- **dialog**: Native dialogs (open, save, message)
- **clipboard**: Clipboard access
- **http**: HTTP requests
- **notifications**: System notifications
- **geolocation**: Location services
- **biometrics**: Fingerprint/Face ID
- **secure_storage**: Platform-specific secure storage

## 2. IPC System

### 2.1 Message Format
- **Protocol**: JSON for simplicity, MessagePack/CBOR for performance
- **Commands**: `plugin.command` format (e.g., `fs.read`)
- **Payload**: JSON object with command parameters
- **Response**: JSON object with result or error

### 2.2 JavaScript Bridge
```javascript
// Invoke command
const result = await window.external.invoke('plugin.command', { param1: 'value1' });

// Listen for events
window.external.listen('plugin.event', (data) => {
  console.log('Event received:', data);
});
```

### 2.3 Platform-Specific Transport
- **Android**: WebView.postMessage → JNI → C core
- **iOS**: WKWebView.messageHandlers → Swift → C core
- **Desktop**: WebView IPC mechanisms

## 3. Security Model

### 3.1 Permission System
- **Config File**: `crossweb.config.json` with plugin permissions
- **ACL**: Allowlist of commands per plugin
- **Runtime Checks**: Enforce permissions before command execution

### 3.2 Sandboxing
- **File Access**: Restrict to app directories
- **Network**: Control allowed domains
- **Native APIs**: Platform-specific restrictions

## 4. Build System Integration

### 4.1 Plugin Scanning
- **Build Time**: Scan `src/plugins/` for plugin directories
- **Registration**: Auto-generate plugin registration code
- **Compilation**: Include plugin sources in build

### 4.2 Platform-Specific Builds
- **Android**: Plugins compiled into JNI library
- **iOS**: Static linking with Swift wrappers
- **Desktop**: Dynamic loading or static linking

## 5. Implementation Phases

### Phase 1: Core Infrastructure
- [x] Define Plugin struct
- [x] Implement basic IPC
- [x] Create plugin registration system
- [ ] Implement permission system

### Phase 2: Core Plugins
- [x] Keystore plugin
- [ ] FS plugin implementation
- [ ] Dialog plugin
- [ ] HTTP plugin

### Phase 3: Platform Integration
- [x] Android JNI bridge
- [ ] iOS Swift bridge
- [ ] Desktop webview integration

### Phase 4: Advanced Features
- [ ] Hot reload support
- [ ] Plugin marketplace
- [ ] Advanced security features

## 6. Testing Strategy

### 6.1 Unit Tests
- Plugin command handlers
- IPC message parsing
- Permission enforcement

### 6.2 Integration Tests
- End-to-end command execution
- Platform-specific functionality
- Security boundary testing

### 6.3 Manual Testing
- UI interaction tests
- Cross-platform compatibility
- Performance benchmarks

## 7. File Structure

```
src/
├── plug.c/h          # Plugin system core
├── ipc.c/h           # IPC implementation
└── plugins/
    └── <plugin-name>/
        ├── src/
        │   ├── lib.c
        │   ├── plugin.h
        │   ├── commands.c
        │   ├── models.h
        │   ├── error.h/c
        │   ├── desktop.c
        │   └── mobile.c
        ├── android/
        ├── ios/
        ├── guest-js/
        └── permissions/

```

This specification provides a roadmap for implementing a robust, secure, and extensible plugin system for CrossWeb.