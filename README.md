# CrossWeb

<p align=center>
  <img src="./resources/logo/logo-256.png">
</p>

> [!WARNING]
> This software is unfinished. Keep your expectations low.

CrossWeb is a cross-platform webview framework inspired by Tauri 2.0, built entirely in C. It provides a native webview container with IPC communication between JavaScript and C, hot-reloading for development, and a plugin system for extensibility.

*Please, read [CONTRIBUTING.md](CONTRIBUTING.md) before making a PR.*

## Features

- Cross-platform webview (WebView2 on Windows, WKWebView on macOS, WebKitGTK on Linux)
- IPC communication between web app and native C code
- Hot-reloading for development
- Plugin system with ABI-stable C interfaces
- No JavaScript build tools required in the runtime
- Deterministic builds with nob.h

## Supported Platforms

- Windows (MSVC and MinGW GCC)
- Linux (planned)
- macOS (planned)
- Mobile (iOS/Android stubs)


## Build from Source

We are using Custom Build System written entirely in C called `nob`. [nob.c](./nob.c) is the program that builds CrossWeb. For more info on this Build System see the [nob.h repo](https://github.com/tsoding/nob.h).

Before using `nob` you need to bootstrap it. Just compile it with the available C compiler. On Linux it's usually `$ cc -o nob nob.c` on Windows with GCC it's `cc nob.c -g nob.exe`. You only need to boostrap it once. After the bootstrap you can just keep running the same executable over and over again. It even tries to rebuild itself if you modify [nob.c](./nob.c) (which may fail sometimes, so in that case be ready to reboostrap it).

I really recommend to read [nob.c](./nob.c) and [nob.h](https://github.com/tsoding/nob.h) to get an idea of how it all actually works. The Build System is a work in progress, so if something breaks be ready to dive into it.

### Windows GCC

```console
$ gcc nob.c -o nob.exe  # ONLY ONCE!!!
$ ./nob.exe
```
$ ./build/crossweb.exe
```

## Usage

Run the built executable:

```console
$ ./build/crossweb [url]
```

If no URL is provided, it loads the local web app from `./web/index.html`.

## Hot Reloading

Edit `./src_build/configurer.c` and enable `CROSSWEB_HOTRELOAD`.

```console
$ ./nob
$ ./build/crossweb
```

This allows you to modify the plugin code in `src/plug.c` and see changes without restarting the application.

## Architecture

CrossWeb consists of:

- **Webview Layer**: Cross-platform webview using webview-c library
- **IPC System**: JSON/MessagePack communication between web and native
- **Plugin System**: ABI-stable C plugins for extensibility
- **Hot Reload**: Development-time plugin reloading
- **Build System**: nob.h-based deterministic builds

## Developing Web Apps

Place your web application files in the `./web/` directory. The main entry point should be `./web/index.html`.

For IPC communication, use the JavaScript functions defined in `index.html` as examples. Messages are sent to the native side and responses are received asynchronously.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

Keep the app running. Rebuild with `./nob`. Hot reload by focusing on the window of the app and pressing <kbd>h</kbd>.

The way it works is by putting the majority of the logic of the application into a `libplug` dynamic library and just reloading it when requested. The [rpath](https://en.wikipedia.org/wiki/Rpath) (aka hard-coded run-time search path) for that library is set to `.` and `./build/`. See [src_build/nob_win64_msvc.c](src_build/nob_win64_msvc.c) for more information on how everything is configured.
