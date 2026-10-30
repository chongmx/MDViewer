# MDViewer

A cross-platform Markdown viewer written in C++17 with [Dear ImGui](https://github.com/ocornut/imgui),
OpenGL and GLFW. One code base targets **Windows, Linux, macOS, Android and the Web (WebAssembly)** on
**x86_64 and aarch64**.

Features: CommonMark-style parsing with GitHub extensions (tables, task lists, strikethrough, autolinks),
headings with a table of contents, syntax-coloured code blocks with copy buttons, in-document and
cross-document links with back/forward history, find-in-page with highlighting, light/dark themes, zoom,
remote documents over HTTP(S), automatic reload when a file changes on disk, drag & drop (desktop).

## Repository layout

```
include/mdv/          public headers, one folder per layer
  os/                 OS abstraction layer API (fs, net, dialog, shell, dispatch, log, platform)
  model/              document model, markdown parser, search, settings data
  interfaces/         service contracts used by view-models
  services/           service implementations on top of the OS layer
  viewmodel/          view-models + Property / Event / Command binding primitives
  view/               Dear ImGui views, markdown renderer, theme, fonts
src/
  os/common|win32|posix|unix|android|web   OS layer backends
  model/ services/ viewmodel/ view/        layer implementations
  app/                composition root (Application) and platform entry points
resources/            fonts and bundled documents (shared by all platforms)
android/              Gradle project (NativeActivity)
web/shell.html        HTML page hosting the WebAssembly build
tests/                unit tests (model, OS helpers, view-models with fake services)
3rdparty/             imgui and IconFontCppHeaders (submodules), glad
```

## Architecture (MVVM)

| Layer          | Depends on            | Responsibility                                       |
|----------------|-----------------------|------------------------------------------------------|
| **os**         | operating system      | files, HTTP(S), dialogs, shell, threads, logging     |
| **model**      | –                     | document tree, parser, search, settings data         |
| **interfaces** | model                 | `IDocumentService`, `ISettingsService`, `IFileSystemService`, `IPlatformService` |
| **services**   | interfaces, model, os | concrete services; `ServiceContainer` wires them     |
| **viewmodel**  | interfaces, model     | presentation state (`Property`), commands, events    |
| **view**       | viewmodel, ImGui      | immediate-mode rendering, input                      |

View-models never include ImGui or OS headers; they receive services through their constructors,
which is what the unit tests use to inject fakes. CMake enforces the layering with one library per layer.

### OS layer backends

| Platform | Files                    | HTTP(S)                    | Open dialog              | Open link            |
|----------|--------------------------|----------------------------|--------------------------|----------------------|
| Windows  | Win32 (UTF-16)           | WinHTTP                    | `GetOpenFileNameW`       | `ShellExecuteW`      |
| Linux    | POSIX                    | libcurl, else HTTP sockets | zenity / kdialog, else in-app browser | `xdg-open` |
| macOS    | POSIX                    | libcurl (system)           | `osascript choose file`  | `open`               |
| Android  | POSIX + `AAssetManager`  | `HttpURLConnection` (JNI)  | in-app browser           | `Intent.ACTION_VIEW` |
| Web      | Emscripten MEMFS         | `emscripten_fetch` (CORS)  | `<input type=file>`      | `window.open`        |

## Getting the code

```sh
git clone --recursive <repo-url>
# or, in an existing clone:
git submodule update --init --recursive
```

GLFW is used from the system if installed, otherwise CMake downloads GLFW 3.4 automatically.
Likewise on Linux/macOS, if the libcurl development package is not installed, CMake downloads
curl and mbedTLS and builds them as static libraries inside the build directory
(`-DMDV_FETCH_CURL=OFF` to disable; HTTPS then falls back to plain HTTP only).

## Building

The easiest way is the build script (Linux, macOS, WSL, or Git Bash/MSYS2 on Windows):

```sh
scripts/build.sh                 # native build, Release
scripts/build.sh --test --run    # build, run unit tests, launch
scripts/build.sh -c Debug        # debug build
scripts/build.sh linux-arm64     # cross-compile for Linux aarch64
scripts/build.sh web             # WebAssembly (activated emsdk required)
scripts/build.sh android         # APK (Gradle + Android SDK required)
scripts/build.sh --help          # all options
```

However you build, binaries are placed in `bin/<os>/<arch>/` with their `resources/` folder next to
them, e.g. `bin/windows/x64/MDViewer.exe`, `bin/linux/arm64/MDViewer`, `bin/macos/universal/MDViewer`,
`bin/web/wasm32/index.html`. Override with `-DMDV_OUTPUT_DIR=<dir>`. Debug and Release builds share
the folder, so the last one built wins.

Presets are optional too; plain CMake works on every desktop platform:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

### Windows (x64 / ARM64)

```sh
cmake --preset windows-x64      # or windows-arm64, or windows-mingw (GCC + Ninja)
cmake --build --preset windows-x64
ctest --preset windows-x64
```

### Linux (x86_64 / aarch64)

```sh
sudo apt install build-essential cmake ninja-build libgl-dev libx11-dev \
     libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev
# libcurl4-openssl-dev is optional: without it curl is built inside the build directory
cmake --preset linux && cmake --build --preset linux    # native build on either architecture
```

Cross-compiling x86_64 → aarch64: see `cmake/toolchains/aarch64-linux-gnu.cmake` and use the
`linux-arm64-cross` preset.

### macOS (universal arm64 + x86_64)

```sh
brew install cmake ninja
cmake --preset macos-universal && cmake --build --preset macos-universal
```

### Web (WebAssembly)

Install and activate the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(3.1.59 or newer, for the `contrib.glfw3` port), then:

```sh
cmake --preset web && cmake --build --preset web
cd bin/web/wasm32 && python3 -m http.server 8080   # open http://localhost:8080
```

Open a document by URL with `http://localhost:8080/?doc=https://example.com/README.md`
(the server must allow cross-origin requests). Use `-DMDV_WEB_GLFW=builtin` for Emscripten's older
built-in GLFW.

### Android (arm64-v8a / x86_64)

Open the `android/` folder in Android Studio, or with Gradle 8.9+ and the Android SDK/NDK installed:

```sh
cd android && gradle assembleDebug
```

The APK lands in `android/app/build/outputs/apk/debug/` (`scripts/build.sh android` also copies it to `bin/android/`).

## Usage

```sh
MDViewer [file.md | https://host/path/file.md]
```

| Shortcut              | Action                 |
|-----------------------|------------------------|
| `Ctrl+O` / `Ctrl+L`   | Open file / URL        |
| `Ctrl+F`, `F3`, `Shift+F3` | Find, next, previous |
| `F5`                  | Reload                 |
| `Alt+Left` / `Alt+Right` | Back / forward      |
| `Ctrl+=`, `Ctrl+-`, `Ctrl+0`, `Ctrl+Wheel` | Zoom |

Settings (theme, zoom, TOC, recent files) are stored in `%APPDATA%\MDViewer`,
`~/Library/Application Support/MDViewer` or `$XDG_CONFIG_HOME/mdviewer`.

## Known limitations

- Images are shown as clickable placeholders (no image decoding yet).
- Text is rendered with Roboto / Cousine; scripts they do not cover (e.g. CJK, emoji) show fallback glyphs.
- Android has no system file picker (NativeActivity cannot receive activity results); use the in-app browser.
- The web build keeps settings in memory only, and remote URLs are subject to CORS.
