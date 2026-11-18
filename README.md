<p align="center">
<img src="assets/icon/Icon9.ico" alt="SkyAutoPianoCpp" width="80px"/>
</p>

<h1 align="center"><b>SkyAutoPianoCpp</b></h1>

<p align="center">
Plays music sheets for you in <b>Sky: Children of the Light</b>.
</p>

<p align="center">
  <a href="docs/INSTALL.md">Install guide</a> |
  <a href="docs/USER_GUIDE.md">How to use</a> |
  <a href="DEVELOPMENT_AND_BUILD.md">Build</a> |
  <a href="https://github.com/NullVoid780/SkyAutoPianoCpp/releases">Latest release</a> |
  <a href="https://discord.gg/Mu8RS9FZky">Discord</a>
</p>

<div align="center">
  <a href="https://github.com/NullVoid780/SkyAutoPianoCpp/releases"><img src="https://img.shields.io/github/v/release/NullVoid780/SkyAutoPianoCpp?style=flat-square&color=blue" alt="Latest release" /></a>
  <a href="https://github.com/NullVoid780/SkyAutoPianoCpp/releases"><img src="https://img.shields.io/github/downloads/NullVoid780/SkyAutoPianoCpp/total?style=flat-square&color=blue" alt="Downloads" /></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-blue?style=flat-square" alt="Windows 10 and 11" />
</div>

<br>

## About

Pick a song, press play, and the app presses the right keys in the game at the right time.

SkyAutoPianoCpp is **the new version of [Sky Auto Piano](https://github.com/HerokeyVN/Sky-Auto-Piano)**,
made by HerokeyVN and NullVoid780. It is written in C++ and Qt, and:

- **Uses less memory** and opens faster
- **Keeps steady timing**, because songs play on their own high-priority thread
- **Reads your old files**: songs, playlists and settings from the old version work as they are

> [!TIP]
> **Coming from the old Sky Auto Piano?** Just install this one. On first start it finds your old
> app and copies your songs, playlists, favorites and settings over for you. Your old app isn't
> changed.

> [!NOTE]
> This is a fan project. It is not made by or connected to thatgamecompany. Use it at your own
> risk.

## Features

- **Auto play**: plays any sheet in Sky with one shortcut
- **Long-press mode**: holds notes for instruments like the violin or Aurora's voice
- **Two playback modes**: **Lite** for weaker PCs, **Strict** for the most accurate timing
- **Speed control**: from 0.1x to 5x, even in the middle of a song
- **Loop**: repeat one song or the whole list
- **Global shortcuts**: control the player while the game window is in front
- **Custom keys**: works even if you changed Sky's keyboard keys
- **Many formats**: JSON, TXT, SkySheet, ABC and MIDI
- **Library**: favorites, recent plays, playlists and multi-select
- **Sheet Store**: find and download new songs inside the app
- **Sheet editor**: fix song details and notes
- **Light and dark themes**
- **Auto updates**: get new versions with one click, with notes on what changed
- **Crash reports**: if something goes wrong, a report is saved on your PC to help fix it
- **Easy move**: copy everything from the original Electron app in one click

## Get started

1. Download `SkyAutoPiano-<version>-setup.exe` from the
   [latest release](https://github.com/NullVoid780/SkyAutoPianoCpp/releases).
2. Run it. You don't need administrator rights.
3. Open Sky, sit at an instrument, pick a song in the app, click back into the game and press
   **Ctrl + Shift + V**.

New to the app? The [install guide](docs/INSTALL.md) walks you through every step, and the
[user guide](docs/USER_GUIDE.md) explains every button.

### Default shortcuts

| Shortcut | What it does |
|---|---|
| `Ctrl + Shift + V` | Play / pause |
| `Ctrl + Shift + C` | Previous song |
| `Ctrl + Shift + B` | Next song |
| `Ctrl + Up` | Faster |
| `Ctrl + Down` | Slower |

You can change them in **Settings → Shortcuts**.

## Requirements

- Windows 10 (version 1809 or newer) or Windows 11, 64-bit
- Sky: Children of the Light for PC

Only Windows is supported for now. Linux support is planned for later.

## Privacy

The app only goes online to:

- check GitHub for a new version (you can turn this off in **Settings → Updates**)
- show and download songs when you open the **Store**

It does not collect or send any personal data. Crash reports stay on your own PC.

## Community

Found a bug or have an idea? [Open an issue](https://github.com/NullVoid780/SkyAutoPianoCpp/issues)
or join the [Discord server](https://discord.gg/Mu8RS9FZky).

Want to help with the code? Read [CONTRIBUTING.md](CONTRIBUTING.md) first.

I tried my best to make these docs easy to follow. If something isn't clear or is missing, please
[open an issue](https://github.com/NullVoid780/SkyAutoPianoCpp/issues) or ask on
[Discord](https://discord.gg/Mu8RS9FZky) and I'll fix it.

## Build the app

The full guide, and how the code is organized, is in [DEVELOPMENT_AND_BUILD.md](DEVELOPMENT_AND_BUILD.md).

## Tech stack

- [C++20](https://isocpp.org/) and [Qt 6](https://www.qt.io/) (Widgets)
- [CMake](https://cmake.org/) and [vcpkg](https://vcpkg.io/) for building
- [nlohmann/json](https://github.com/nlohmann/json), [miniz](https://github.com/richgel999/miniz)
  and [Catch2](https://github.com/catchorg/Catch2)
- [Inno Setup](https://jrsoftware.org/isinfo.php) for the installer

## Credits

- [HerokeyVN](https://github.com/HerokeyVN): creator of Sky Auto Piano and its design
- [NullVoid780](https://github.com/NullVoid780): the C++ version
- The old Electron version is still available at
  [HerokeyVN/Sky-Auto-Piano](https://github.com/HerokeyVN/Sky-Auto-Piano)
- [Qt](https://www.qt.io/) is used under the LGPL v3. Its DLLs ship next to the program, so you can
  swap in your own Qt build.
- [Roboto](https://fonts.google.com/specimen/Roboto) font and
  [Bootstrap Icons](https://icons.getbootstrap.com/)

All licence texts are in [packaging/licenses](packaging/licenses).

## License

MIT. See [LICENSE](LICENSE).
