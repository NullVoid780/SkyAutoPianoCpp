# Build from source

For people who want to build the app themselves or work on the code. If you just want to use the
app, see the [install guide](docs/INSTALL.md).

## Tech stack

- **Language:** [C++20](https://isocpp.org/)
- **Interface:** [Qt 6](https://www.qt.io/) Widgets (no QML, no web views)
- **Build:** [CMake](https://cmake.org/) with [Ninja](https://ninja-build.org/), libraries from
  [vcpkg](https://vcpkg.io/)
- **Libraries:**
  - [nlohmann/json](https://github.com/nlohmann/json) for reading and writing JSON
  - [miniz](https://github.com/richgel999/miniz) for zip files
  - [dr_libs](https://github.com/mackron/dr_libs) for reading the soundpack's audio files
  - [Catch2](https://github.com/catchorg/Catch2) for tests
  - `skyinput` (in `libs/skyinput`), our own small library that sends key presses to the game
- **Installer:** [Inno Setup 6](https://jrsoftware.org/isinfo.php)

## Prerequisites

- Windows 10 or 11, 64-bit
- [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community is fine). In the installer
  pick **Desktop development with C++**, and make sure **C++ CMake tools for Windows** and **vcpkg
  package manager** are ticked. This also gives you CMake and Ninja.
- [Qt 6.8 or newer](https://www.qt.io/download-qt-installer-oss). In the Qt installer pick the
  **MSVC 2022 64-bit** build. Only Qt Base and Qt SVG are needed.
- [Git](https://git-scm.com/)
- [Inno Setup 6](https://jrsoftware.org/isinfo.php), only if you want to make the installer

## Building

### 1. Get the code

```bat
git clone https://github.com/NullVoid780/SkyAutoPianoCpp.git
cd SkyAutoPianoCpp
```

### 2. Open a build terminal

From the Start menu, open **x64 Native Tools Command Prompt for VS 2022** (or **Developer Command
Prompt for VS 2022**). A normal PowerShell or Command Prompt won't work.

Then tell the build where vcpkg and Qt are:

```bat
set "VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg"
set "QT_ROOT=C:\Qt\6.12.0\msvc2022_64"
```

> [!WARNING]
> **Use your own Qt version in `QT_ROOT`.** Open `C:\Qt` in File Explorer and look at the folder
> name, for example `6.8.3` or `6.12.0`. You need to set these again every time you open a new
> terminal.

### 3. Build the app

```bat
cmake --preset win-x64-release
cmake --build --preset win-x64-release
```

The first build takes a few minutes while vcpkg gets the libraries. When it's done, the app is at
`build\win-x64-release\bin\SkyAutoPiano.exe`. Qt is copied next to it, so you can run it straight
away.

There are three presets:

| Preset | What it's for |
| --- | --- |
| `win-x64-debug` | Debug build, easiest to step through in a debugger |
| `win-x64-release` | Fast build with debug symbols, used for releases |
| `win-x64-ci` | Same as release, but every compiler warning counts as an error |

### 4. Make the installer

With Inno Setup 6 installed:

```bat
cmake --build --preset win-x64-release --target package
```

This creates `build\win-x64-release\deploy\SkyAutoPiano-<version>-setup.exe`. To get only the
ready-to-run folder without the installer, use `--target deploy` instead.

## How the code is organized

You should be comfortable with C++ and Qt Widgets. The app is split into layers, and each layer only
uses the ones below it:

```
ui        windows, widgets, theme          (src/ui)
app       start-up, wires everything       (src/app)
services  library, playback, settings...   (src/services)
codec     reading and writing sheet files  (src/codec)
core      logging, files, text, timing     (src/core)
```

### Folders

```
SkyAutoPianoCpp/
├── assets/           fonts, icons, app icon, soundpack, translations
├── cmake/            helper CMake scripts
├── docs/             install, user and release guides
├── libs/skyinput/    sends key presses to the game
├── packaging/        installer script, Windows manifest and licences
├── src/
│   ├── app/          start-up, and wiring the parts together
│   ├── codec/        reading and writing sheet files (JSON, ABC, MIDI)
│   ├── core/         shared tools: logging, files, text, timing
│   ├── crash/        crash handler and CrashReport.exe
│   ├── model/        plain data: songs, notes, playlists, settings
│   ├── net/          web requests, downloads, zip files
│   ├── platform/     Windows-only parts: file dialogs, tray messages, title bar
│   ├── services/
│   │   ├── audio/          the sheet editor's sound and soundpack
│   │   ├── library/        songs, playlists, favorites, recent plays, import
│   │   ├── notifications/  pop-up messages
│   │   ├── playback/       the Lite and Strict players and their timing
│   │   ├── settings/       settings file, shortcuts, saved window state
│   │   ├── store/          Sky Sheet Store
│   │   └── update/         the updater
│   └── ui/           every window, widget and the theme
├── tests/            automated tests
└── website/          this website
```

### Using a test library

The app keeps its `config`, `data` and `logs` folders next to the exe. To try things without
touching your real songs, point it at another folder:

```bat
set "SKYAUTOPIANO_HOME=D:\test-library"
build\win-x64-release\bin\SkyAutoPiano.exe
```

> [!WARNING]
> **Only one copy of the app can run at a time.** Close it before you build again, or the build
> fails with `LNK1168`.

- **Start-up:** `src/main.cpp` sets up Qt and the crash handler, then `src/app/application.cpp`
  creates every service and window and connects them. If you add a new service, this is where it
  gets created.
- **Playback:** `PlaybackService` (`src/services/playback/playback_service.cpp`) is what the rest of
  the app talks to. It runs either `LitePlayer` or `StrictPlayer`, depending on the setting. Both
  play the song on their own high-priority thread, so the window can't slow the timing down. The
  actual key presses go through `libs/skyinput`.
- **Theme and styles:** **Every** color, size and shadow lives in `src/ui/theme.cpp`. The stylesheet
  `src/ui/styles/app.qss` only uses `@token` names, which are filled in from the theme. A test
  (`tests/ui/test_theme_tokens.cpp`) fails if the stylesheet uses a token that doesn't exist, or a
  color written straight into it.
- **Icons:** SVG files in `assets/icons`. Drop a new file there and use it by its name without
  `.svg`. The build picks them up automatically and recolors them to match the theme.
- **Adding a source file:** New `.cpp` files must be added to the list in `src/CMakeLists.txt` (or
  `tests/CMakeLists.txt` for tests). They are not picked up automatically.

```cpp
IconCache::pixmap(QStringLiteral("download"), size, color, devicePixelRatioF());
```

## Running tests

```bat
ctest --test-dir build\win-x64-release
```

All tests should pass before you open a pull request. Tests use
[Catch2](https://github.com/catchorg/Catch2) and live in `tests/`, in the same folders as the code
they test. Shared helpers are in `tests/support`.

> [!WARNING]
> **Tests must never send real key presses to the game.** Test the timing and the note lists instead
> of the player itself.

## Publishing a new version

The app updates itself from this repository's GitHub releases, so a release must follow these rules:

1. Change the version in `CMakeLists.txt` (`SKY_VERSION`) and `vcpkg.json`, and add a `## vx.y.z`
   section to `CHANGELOG.md`.
2. Build the installer with `--target package`.
3. Create a GitHub release with the tag `vx.y.z` and upload `SkyAutoPiano-x.y.z-setup.exe`. Don't
   rename the file.
4. Paste the notes into the release description. **This is what users see in the update window.**
   Add `<!-- critical -->` for an important fix. Anything after a `---` line only shows on GitHub.
5. Publish it as a normal release, not a draft or pre-release.

## Translations

The app comes in English, Vietnamese, Simplified Chinese, Japanese and Spanish. By default it
follows the Windows language, and **Settings → General → Language** picks one by hand.

- Put every text people see through `tr("…")`. Use `trUtf8("…")` where the code needs a
  `std::string`, and `trNoop("…")` for text in a table that is translated where it's shown.
- Write whole sentences with `%1`, `%2` for the parts that change, never glue pieces together.
  Counts get one text for 1 and one for more, for example
  `trCount(n, trNoop("1 song"), trNoop("%1 songs"))`.
- Add each new text with its translation to every file in `assets/i18n/`. The English text is the
  key.

> [!TIP]
> The tests list any text that is missing from a language file, any unused entry, any translation
> that lost a `%1`, and any visible text that skips `tr()`.

## The website

The website in `website/` is plain HTML, CSS and one `site.js`, so you can open any page straight
from the folder. It's published to GitHub Pages whenever `main` changes the website.

The docs pages are written by hand, so when you change one of the docs, change its page too. The
**Copy as Markdown** button turns the text on the page into Markdown in the browser, so it always
copies exactly what the page says.

## Problems while building

<details>
<summary>`Could not find a package configuration file provided by "Qt6"`</summary>

`QT_ROOT` is missing or wrong. Most of the time the version number doesn't match what you have.
Check the folder name in `C:\Qt` and use that, then run `cmake --preset win-x64-release` again in
the same window.

</details>

<details>
<summary>`LNK1104: cannot open file 'winmm.lib'` or `'cl' is not recognized`</summary>

You're in a normal terminal. Open **x64 Native Tools Command Prompt for VS 2022** and set the two
variables again.

</details>

<details>
<summary>`LNK1168: cannot open … SkyAutoPiano.exe for writing`</summary>

The app is still running. Close it, including from the tray, and build again.

</details>

<details>
<summary>`LNK1104: cannot open file 'bin\skyautopiano_tests.exe'`</summary>

Something, usually antivirus, was holding the file for a moment. Run the build command again.

</details>

<details>
<summary>`Could not find toolchain file: /scripts/buildsystems/vcpkg.cmake`</summary>

`VCPKG_ROOT` isn't set. Point it at your vcpkg folder.

</details>

<details>
<summary>The C++ compiler "is not able to compile a simple test program" and mentions `ld.exe`</summary>

The build folder remembers a linker from an earlier run in a normal terminal. Run
`cmake --preset win-x64-release --fresh` from the Visual Studio terminal to start over without it.

</details>

<details>
<summary>The installer step says Inno Setup was not found</summary>

Install Inno Setup 6 in its default folder, then run the `cmake --preset` step again.

</details>
