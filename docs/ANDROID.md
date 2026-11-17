# Android Platform Detailed Documentation

This document provides comprehensive specifications for the Android platform implementation in CrossWeb, detailing every function, class, and component with their purposes, parameters, return values, and interactions.


## Architecture Overview

CrossWeb Android uses a hybrid architecture with WebView for UI, JNI for native communication, and a plugin system for extensible functionality.

**Architecture summary:**
- Hybrid app using Android WebView for UI, JNI for native <-> Java/Kotlin calls, and a C-based plugin system for native features.

**What the current code does:**
- **Secure-device check**: `MainActivity.kt` prompts users to enable a secure lock screen (PIN/pattern/password) during startup. See [crossweb/templates/android/app/src/main/java/com/example/crossweb/MainActivity.kt](crossweb/templates/android/app/src/main/java/com/example/crossweb/MainActivity.kt).
- **WebView setup**: The activity enables JavaScript, disables most persistent storage and cache, installs a WebViewAssetLoader serving assets from the APK (domain appassets.androidplatform.net), and injects a minimal CSP meta element on page finish.
- **JS bridge**: An `Ipc` instance is registered as `external` via `addJavascriptInterface()`. The Kotlin class implements `postMessage()` and `invoke()` (returns a UUID) and forwards messages to native JNI methods `ipc()` and `nativeInvoke()` respectively. See [crossweb/templates/android/app/src/main/java/com/example/crossweb/Ipc.kt](crossweb/templates/android/app/src/main/java/com/example/crossweb/Ipc.kt).
- **Native JNI implementation**: `crossweb/src/android_bridge.c` provides JNI entry points. The JNI layer:
	- Exposes native functions mapped to the app package (templates use placeholder replacements for package names).
	- Implements asynchronous plugin invocation: it copies strings into heap-allocated data, creates global refs for the Java objects/IDs, spawns a pthread which calls `plug_invoke()` and uses `respond_callback()` to return results to the Java side.
	- Caches method IDs (e.g., `resolveInvokeMethodId`) and initializes plugins in `JNI_OnLoad` by calling `plug_init(NULL)`.
- **IPC support**: A simple stub queue exists in `crossweb/src/ipc.c`, with `ipc_send()` and `ipc_receive()` helpers. It currently builds JavaScript payloads and would call webview-injection helpers (those are left as comments / stubs).
- **Project/template generation and package mangle/replace**: `templates.c` performs replacements for `{{PACKAGE_NAME}}`, `{{APP_NAME}}`, and the mangled package identifiers. The build helpers copy and patch template files into the generated Android project.
- **Plugin copying for Android**: `src_build/plugins.c` implements `copy_plugins_for_android()` which copies C sources into `android/app/src/main/c/plugins`, copies guest-js assets into the web app, copies permission resources into `android/app/src/main/res`, and copies any Java/Kotlin plugin sources into the correct package path.
- **Build orchestration**: `src_build/android_build.c` contains `android_build_crossweb()` and `android_build_dist()` which:
	- Run `npm run build` in `web`.
	- Ensure `android/app/src/main/c` exists and copy various C headers/sources (`android_bridge.c`, `plug.c`, `ipc.c`, `config.h`, etc.).
	- Replace package placeholders in the copied `android_bridge.c` using `mangle_package_name()`/`slash_package_name()` and `replace_all_in_file()`.
	- Generate `CMakeLists.txt` for native sources and `proguard-rules.pro` entries based on plugins.
	- Copy `web/dist` into `android/app/src/main/assets` (on Windows the code uses `xcopy`).
	- For debug builds, runs Gradle (`gradle assembleDebug`) from the `android` folder.

**Files to inspect for behavior and extension points:**
- [crossweb/src/android_bridge.c](crossweb/src/android_bridge.c) — JNI glue and threading/response model.
- [crossweb/templates/android/app/src/main/java/com/example/crossweb/Ipc.kt](crossweb/templates/android/app/src/main/java/com/example/crossweb/Ipc.kt) — Kotlin interface used by the WebView.
- [crossweb/templates/android/app/src/main/java/com/example/crossweb/MainActivity.kt](crossweb/templates/android/app/src/main/java/com/example/crossweb/MainActivity.kt) — Activity lifecycle and WebView configuration.
- [crossweb/src_build/plugins.c](crossweb/src_build/plugins.c) — plugin packaging logic for Android.
- [crossweb/src_build/android_build.c](crossweb/src_build/android_build.c) — top-level build steps invoked by the `nob` build flow.

**Current limitations and recommended next work (short list):**
- **Unimplemented WebView callbacks:** Several JNI functions in `android_bridge.c` are declared but left as TODO (request handling, URL override, onEval, onPage events). Implement them to support advanced navigation, resource interception, and lifecycle events.
- **IPC queue is a stub:** `src/ipc.c` has a simple in-memory queue and commented-out calls to `webview_inject_js`. Replace with a robust, thread-safe bridge that uses the correct webview injection API on Android.
- **Global JNI state safety:** The JNI bridge uses globals like `global_env` and `global_obj`; audit for thread-safety and lifetimes, prefer using local envs with carefully managed global refs and avoid long-lived raw globals.
- **CSP and inline scripts:** The injected CSP in `MainActivity.kt` currently allows `'unsafe-inline'` for scripts/styles. Tighten content security policy and evaluate whether inline script usage can be eliminated.
- **Windows-specific copy commands:** Build code uses `xcopy` directly (Windows tooling). Consider cross-platform copy (or use Java/Gradle copy tasks) for build portability.
- **Release build/hardening:** The current flow focuses on debug and dist; add signing, release build steps, and test ProGuard/R8 obfuscation with the generated `proguard-rules.pro` to keep native/JNI method names intact.
- **Error handling / reporting:** Many helper functions return `false` on error but callers don't always produce user-friendly messages. Improve logging and failure messages in build flow.

**Short-term roadmap (suggested):**
- Implement the missing JNI WebView callbacks in `android_bridge.c` to support URL overrides and resource handling.
- Replace the stub IPC queue with a version that calls into the Android WebView thread safely (use `evaluateJavascript` or Java handlers).
- Harden the JNI lifecycle (avoid raw JNI globals; prefer thread-local or pass refs explicitly).
- Add a Gradle task to copy the built web assets instead of `xcopy` for platform independence.
- Add tests (or a small sample app) to exercise `invoke()`/`resolveInvoke()` round-trips and plugin loading.

If you'd like, I can implement one of the items above next — which would you prefer? (implement JNI callbacks, replace IPC stub, or convert `xcopy` to Gradle copy task)
- `jcmd`: Java string command
- `jpayload`: Java string payload  
**Return Value**: void  
**Behavior**:
- Caches `resolveInvoke` method ID on first call
- Extracts C strings from Java strings
- Sets global variables for callback
- Calls `plug_invoke()` with command and callback
- Clears global variables
- Releases string references

#### `Java_com_example_crossweb_CWebViewClient_assetLoaderDomain(JNIEnv *env, jobject obj)`
**Purpose**: Returns the asset loader domain for WebView configuration.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance  
**Return Value**: `jstring` - Domain string "appassets.androidplatform.net"

#### `Java_com_example_crossweb_CWebViewClient_withAssetLoader(JNIEnv *env, jobject obj)`
**Purpose**: Indicates whether asset loader is enabled.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance  
**Return Value**: `jboolean` - Currently JNI_FALSE (TODO: implement)

#### `Java_com_example_crossweb_CWebViewClient_handleRequest(JNIEnv *env, jobject obj, jstring jwebviewId, jobject jrequest, jboolean jisDocumentStartScriptEnabled)`
**Purpose**: Handles WebView resource requests.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `jwebviewId`: WebView identifier
- `jrequest`: WebResourceRequest object
- `jisDocumentStartScriptEnabled`: Script injection flag  
**Return Value**: `jobject` - Currently NULL (TODO: implement)

#### `Java_com_example_crossweb_CWebViewClient_shouldOverride(JNIEnv *env, jobject obj, jstring jurl)`
**Purpose**: Determines if URL loading should be overridden.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `jurl`: URL string  
**Return Value**: `jboolean` - Currently JNI_FALSE (TODO: implement)

#### `Java_com_example_crossweb_CWebViewClient_onPageLoading(JNIEnv *env, jobject obj, jstring jurl)`
**Purpose**: Called when page starts loading.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `jurl`: Loading URL  
**Return Value**: void (TODO: implement)

#### `Java_com_example_crossweb_CWebViewClient_onPageLoaded(JNIEnv *env, jobject obj, jstring jurl)`
**Purpose**: Called when page finishes loading.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `jurl`: Loaded URL  
**Return Value**: void (TODO: implement)

#### `Java_com_example_crossweb_CWebView_shouldOverride(JNIEnv *env, jobject obj, jstring jurl)`
**Purpose**: WebView URL override check.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `jurl`: URL string  
**Return Value**: `jboolean` - Currently JNI_FALSE (TODO: implement)

#### `Java_com_example_crossweb_CWebView_onEval(JNIEnv *env, jobject obj, jint id, jstring jresult)`
**Purpose**: Handles JavaScript evaluation results.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `id`: Evaluation ID
- `jresult`: Result string  
**Return Value**: void (TODO: implement)

#### `Java_com_example_crossweb_CWebChromeClient_handleReceivedTitle(JNIEnv *env, jobject obj, jobject jwebview, jstring jtitle)`
**Purpose**: Handles received page title.  
**Parameters**:
- `env`: JNI environment
- `obj`: Java object instance
- `jwebview`: WebView object
- `jtitle`: Page title  
**Return Value**: void (TODO: implement)

#### `JNI_OnLoad(JavaVM *vm, void *reserved)`
**Purpose**: JNI library initialization function.  
**Parameters**:
- `vm`: Java VM instance
- `reserved`: Reserved parameter (unused)  
**Return Value**: `jint` - JNI version or error code  
**Behavior**:
- Gets JNI environment
- Initializes method ID cache
- Calls `plug_init(NULL)` for plugin initialization
- Returns JNI_VERSION_1_6

## Build System (nob_android.c)

**Purpose**: Build automation for Android platform using nob build system.

### Functions

#### `is_plugin_enabled(const char *plugin_name)`
**Purpose**: Checks if a plugin is enabled via build configuration.  
**Parameters**:
- `plugin_name`: Plugin name string  
**Return Value**: `bool` - true if enabled, false otherwise  
**Behavior**:
- Reads `build/config.h`
- Searches for macro `CROSSWEB_PLUGIN_{PLUGIN_NAME}`
- Returns true if found or file not found

#### `collect_c_files(const char *dir, Nob_File_Paths *files, const char *base)`
**Purpose**: Recursively collects C source files for CMake generation.  
**Parameters**:
- `dir`: Directory to scan
- `files`: Dynamic array to store file paths
- `base`: Base path for relative path calculation  
**Return Value**: `bool` - true on success  
**Behavior**:
- Lists directory contents
- Recursively processes subdirectories
- Adds .c files to array (excluding mobile.c/desktop.c in plugins/fs)

#### `generate_cmake_lists(const char *path)`
**Purpose**: Generates CMakeLists.txt for native library compilation.  
**Parameters**:
- `path`: Output file path  
**Return Value**: `bool` - true on success  
**Behavior**:
- Creates CMake project file
- Includes all collected C files
- Links against Android log library

#### `copy_plugins_for_android(void)`
**Purpose**: Copies plugin files to Android build directory.  
**Parameters**: None  
**Return Value**: `bool` - true on success  
**Behavior**:
- Creates plugin directories
- Copies C source files
- Copies Android-specific Java/Kotlin files
- Copies guest-js files

#### `generate_proguard_rules(void)`
**Purpose**: Generates ProGuard rules for plugin classes.  
**Parameters**: None  
**Return Value**: `bool` - true on success  
**Behavior**:
- Writes base ProGuard rules
- Scans plugins for android/ directories
- Generates keep rules for plugin manager classes

#### `build_crossweb(void)`
**Purpose**: Builds debug APK for development.  
**Parameters**: None  
**Return Value**: `bool` - true on success  
**Behavior**:
- Builds web assets
- Copies source files
- Calls plugin copying and ProGuard generation
- Generates CMake files
- Copies assets
- Runs Gradle assembleDebug

#### `build_dist(void)`
**Purpose**: Builds release APK for distribution.  
**Parameters**: None  
**Return Value**: `bool` - true on success  
**Behavior**:
- Similar to build_crossweb but includes hotreload
- Runs Gradle bundleRelease

## Plugin System
The Android platform supports a plugin system allowing for modular extensions. Plugins can be developed in C/C++ for native functionality and Java/Kotlin for Android-specific features. Each plugin must implement the required interfaces and register with the CrossWeb plugin manager.
Plugins can be enabled or disabled via build configuration macros in `build/config.h`.