# Install guide

This guide shows you how to install SkyAutoPianoCpp, play your first song, update it and remove it.
At the end there are fixes for the most common problems.

## What you need

- Windows 10 (version 1809 or newer) or Windows 11, 64-bit
- About 60 MB of free space
- Sky: Children of the Light for PC

You **don't** need administrator rights or any other programs. The installer has everything.

## 1. Download

1. Go to the [Releases page](https://github.com/NullVoid780/SkyAutoPianoCpp/releases).
2. Under the newest release, download `SkyAutoPiano-<version>-setup.exe`.

## 2. Install

1. Double-click the file you downloaded.
2. If Windows shows a blue **"Windows protected your PC"** window, click **More info**, then
   **Run anyway**. This shows up because the installer isn't code-signed yet.
3. Click **Next** and keep the suggested folder, or pick your own.
4. Tick **Create a desktop shortcut** if you want one.
5. Click **Install**, then **Finish**. The app opens by itself.

Next time, open it from the Start menu: search for **Sky Auto Piano**.

## 3. First start

- **Used the original Electron app before?** The app finds it and asks if you want to copy your
  songs, playlists, favorites and settings. Click **Import**. Your old app isn't changed.
- **Didn't get asked, or skipped it?** Go to **Settings → General → Import Previous Songs** and
  pick the old app's folder.
- **New here?** Click **+** in the bottom-right corner to add sheet files, or open the menu (☰)
  and go to **Store** to download songs.

## 4. Play your first song

1. Open Sky and sit at an instrument.
2. In the app, click the song you want.
3. Click inside the Sky window so the game is in front.
4. Press **Ctrl + Shift + V**. Press it again to pause.

Tip: turn on **Settings → General → Minimize on Play** to hide the app when a song starts.

## Where your files are

Everything is kept inside the install folder
(`C:\Users\<your name>\AppData\Local\Programs\Sky Auto Piano` by default):

| Folder | What's inside |
|---|---|
| `config` | Your settings |
| `data` | Your songs, playlists, favorites and recent plays |
| `logs` | A small log file that helps with bug reports |

You can open this folder from **Settings → About → Data Folder**.

**To back up**, copy the `config` and `data` folders somewhere safe. You can also save your
settings to one file with **Settings → General → Export**.

## Updating

The app checks for a new version when it starts, and every few hours while it's open. When there is
one, a window shows what's new:

- **Update now** downloads it. When it's ready, click **Restart now**, or just keep playing: it
  installs by itself when you close the app.
- **Not now** hides it until the next time you open the app.

You can also check by hand in **Settings → Updates → Check Now**, or download the newest installer
and run it over your current install.

Updating never touches your songs or settings. Don't want update checks? Turn off
**Automatic Updates** in **Settings → Updates**.

## Uninstalling

1. Open **Windows Settings → Apps → Installed apps**.
2. Find **Sky Auto Piano**, click **⋯**, then **Uninstall**.

Your `config`, `data` and `logs` folders are **kept**, so nothing is lost if you install again.
To remove everything, delete the install folder after uninstalling.

## Common problems

**The game doesn't react when a song plays**
- Click inside the Sky window before you press the shortcut.
- If you run Sky as administrator, run this app as administrator too. Windows blocks key presses
  from a normal program to an administrator one.
- If you changed Sky's keyboard keys, set the same keys in **Settings → Keyboard**.

**Notes are out of time, or some are missed**
- Try the other mode in **Settings → General → Playback Mode**. **Strict** has the best timing,
  **Lite** is easier on a slow PC.
- Close heavy programs like screen recorders while you play.

**Shortcuts don't work**
- Shortcuts are paused while the **Settings** window is open. Close it and try again.
- Another program may use the same keys. Pick different ones in **Settings → Shortcuts**.

**"The code execution cannot proceed because ... .dll was not found"**
- A file is missing from the install folder, often because antivirus removed it. Run the installer
  again to repair it.

**My antivirus warns about the app**
- The app sends key presses to another window, which some antivirus programs find suspicious. All
  the code is in this repository, so you can read it or [build it yourself](../DEVELOPMENT_AND_BUILD.md).

**The app closed by itself**
- A crash report is saved on your PC, and the app tells you about it on the next start. Please
  [open an issue](https://github.com/NullVoid780/SkyAutoPianoCpp/issues) and attach the newest
  files from `%LOCALAPPDATA%\SkyAutoPiano\crashes` and `logs\skyautopiano.log`.

**I can't open a second window**
- That's on purpose. Opening the app again brings the open window to the front.
