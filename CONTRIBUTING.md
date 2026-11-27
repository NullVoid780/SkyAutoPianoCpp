# Contributing

Thanks for wanting to help! Small, focused changes are the easiest to review and merge.

## Before you start

- **Open an issue first** for anything bigger than a small bug fix, and wait for a reply. This saves
  you from working on something that might not be accepted.
- Build the app and make sure it runs. [Build from source](DEVELOPMENT_AND_BUILD.md) shows how.
- Read the code around what you want to change, and follow the same style.

## Making a change

- Keep each pull request about **one thing**.
- Add or update a test in `tests/` when you change how something works.
- Run the tests before you open the pull request:

```bat
ctest --test-dir build\win-x64-release
```

- Colors, sizes and spacing go in `src/ui/theme`, not in other files.

> [!WARNING]
> **Tests must never send real key presses to the game.**

## Your pull request

Explain in your own words:

1. What problem it fixes
2. What you changed
3. How you tested it

## Reporting a bug

[Open an issue](https://github.com/NullVoid780/SkyAutoPianoCpp/issues) and include:

- What you did, what you expected, and what happened instead
- Your app version (**Settings → Updates**) and your Windows version
- `logs\skyautopiano.log` from the install folder, and any crash report from
  `%LOCALAPPDATA%\SkyAutoPiano\crashes`

> [!TIP]
> Not sure if it's a bug? Ask on [Discord](https://discord.gg/Mu8RS9FZky) first.
