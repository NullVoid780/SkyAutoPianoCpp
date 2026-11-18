# User guide

A quick tour of everything the app can do. If you haven't installed it yet, start with the
[install guide](INSTALL.md).

## Quick Start

The first time you open the app, a short setup walks you through:

- **Look:** follow Windows, or always light or dark, and pick a theme.
- **Previous version:** the app looks for the original Electron app and offers to import its
  songs, playlists, favorites and settings in one click. If it isn't found, press **Choose folder**
  and pick the old app's folder. Nothing in the old app is changed.
- **Playback:** Lite or Strict, and whether to hide the window when a song starts.
- **Controls:** the global shortcuts for play, previous, next and speed.

Press **Skip setup** to jump straight in. Run it again any time from **Settings → General →
Quick Start**.

## The main window

- **Top**: the menu button (☰) and the search box. Search looks at song names and authors.
- **Middle**: your songs, one card each, with the name, author, transcriber and BPM.
- **Bottom**: the player, with the song name, progress bar, speed, long-press switch, loop and the
  **Prev**, **Play** and **Next** buttons.

The menu (☰) switches between **All Songs**, **Favorite**, **Playlists**, **Recent Play** and
**Store**.

## Adding songs

There are three ways:

- Click **+** in the bottom-right corner and pick one or more sheet files (`.json`, `.txt`,
  `.skysheet`, `.mid`, `.midi`).
- Open **Store** from the menu, find a song and download it.
- Copy everything from the original Electron app with
  **Settings → General → Import Previous Songs**.

## Playing a song

1. Click a song card to pick it.
2. Click inside the Sky window.
3. Press **Ctrl + Shift + V**, or click **Play**. Press it again to pause.

Pausing keeps your place. To start from somewhere else, pause and click anywhere on the progress
bar.

| Control | What it does |
|---|---|
| **Speed** | How fast the song plays, from 0.1x to 5x. **Ctrl + Up** and **Ctrl + Down** change it, even while playing. |
| **Long-press mode** | Holds each key for the length of the note. Turn it on for instruments that hold notes, like the violin. |
| **Loop** | Click to switch between off, loop the whole list, and loop one song. |
| **Delay next** | How long to wait before the next song starts. |

### Lite or Strict?

Choose in **Settings → General → Playback Mode**:

- **Lite** is light on your PC, but the timing can drift a little on a busy PC.
- **Strict** has the most accurate timing, but uses more CPU.

If notes sound out of time, try the other one.

## Organizing songs

Each song card has a **⋮** menu:

- **Favorite** adds the song to your Favorite list (click again to remove it).
- **Edit** opens the sheet editor.
- **Add to playlist** puts the song in a playlist.
- **Delete** removes the song from your library.

Inside a playlist you can also **Move up**, **Move down** and **Remove from playlist**.

**Multi-select** (top right of the list) lets you tick many songs, then add them to a playlist or
delete them all at once.

**Recent Play** shows the songs you played last, newest first.

## The sheet editor

Open it from a song card: **⋮ → Edit**.

- Change the name, author, transcriber and BPM on the left.
- The grid shows the song step by step. Click a step to pick it.
- Click a key on the key pad to add it to that step. Click it again to remove it.
- **Save** keeps your changes, **Cancel** throws them away.
- **Export** saves the sheet as a `.txt`, `.json` or `.skysheet` file you can share.

Sheets with held notes can be viewed but not changed yet.

## Settings

Click the gear in the bottom-left corner. While Settings is open, the global shortcuts are paused
so you can type freely.

**General**
- **Import / Export**: save all your settings to one file, or load them back.
- **Auto Save Settings**: save changes from the player (speed, long-press and so on) by itself.
- **Minimize on Play**: hide the app when a song starts.
- **Playback Mode**: Lite or Strict.
- **Import Previous Songs**: copy everything from the original Electron app.
- **Quick Start**: go through the first-time setup again.

**Keyboard**
If you changed Sky's own keys, turn on **Custom Keyboard** and set the same 15 keys here. Press
**Tab** to jump to the next key.

**Shortcuts**
Click a shortcut box and press the new keys. Two actions can't share the same keys.

**Themes**
Pick a look for the whole app, or build your own. See [Themes](#themes) below.

**Updates**
See your version, **Check Now** for a new one, and turn **Automatic Updates** on or off.

**About**
Links to the original app and this project, and buttons to open your data folder and the licence
files.

Click the **save** button in the bottom-right corner after changing settings, unless
**Auto Save Settings** is on.

## Themes

By default the app follows Windows: if Windows apps are set to dark, the app is dark, and it
switches by itself when you change the Windows setting. Click the sun or moon button in the
top-right of the player to pick light or dark yourself. To follow Windows again, open
**Settings → Themes** and set **Light or Dark** to **Follow Windows** (or **Always light** /
**Always dark**).

For more looks, open **Settings → Themes**:

- **Built-in themes:** Default, Indigo, Green, Orange, Pink, Blue and Black and White. Each one has a
  light and a dark version, and its preview shows both halves. Click one to use it.
- **New Theme** starts a theme from the look you have now. The **pencil** on any theme makes a copy
  you can change.
- **Building a theme:** choose **Editing: Light mode** or **Dark mode**, then pick the five
  **Quick Colors**: Accent, Background, Cards and boxes, Text and Player bar. Each one colors every
  part of the app that belongs with it, and the whole app changes as you go. Opacity is supported.
- **Live Preview:** a small picture of the app in light and dark sits beside the builder and
  stays in view while you scroll, so you always see what you are building. Its tabs switch
  between the song list, menus, alerts, Settings and the sheet editor. Point at a color, or open
  a section, and the preview jumps to the right screen and lights up the part that color paints.
  **Quick Colors** follow the screen too: on the Editor tab they become the editor's five main
  colors, on Settings the Settings window's, and so on.
- **Fine-tuning:** under **All Colors**, open a section (song cards, player bar, menu drawer and so
  on) to change a single part. Each section shows how many of its colors you changed from the theme
  you started with.
- The **undo** arrow next to a color puts back the color it had when you started, and
  **Undo changes** does that for the whole version you are editing. **Save Theme** keeps it.
  **Cancel** throws away your changes.
- **Title bar:** on Windows 11 the bar at the top of each window takes the theme's colors too. It
  follows the window background unless you set it under **All Colors → Title Bar**. Windows 10
  keeps its normal light or dark title bar.
- **Import / Export:** share a theme as a small `.json` file.

The sun and moon button switches the theme you are using between its light and dark version, and
stops following Windows until you turn **Follow Windows** back on.

## If something goes wrong

If the app crashes, a report is saved in `%LOCALAPPDATA%\SkyAutoPiano\crashes` and you'll see a
message about it on the next start. Please share those files and `logs\skyautopiano.log` when you
[report the problem](https://github.com/NullVoid780/SkyAutoPianoCpp/issues).

More fixes are in the [install guide](INSTALL.md#common-problems).
