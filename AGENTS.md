# Agent Rules - SkyAutoPianoCpp

This file is the operating contract for AI agents working in this repository.
SkyAutoPianoCpp (Sky Auto Piano 2) is the native C++20 / Qt 6 successor of the
Electron app [Sky Auto Piano](https://github.com/HerokeyVN/Sky-Auto-Piano).
It plays music sheets in Sky: Children of the Light on Windows 10/11 x64.

Language: this document and `docs/` are written in English. Code identifiers
and commit messages are English. User-facing UI text always goes through the
i18n system (section 6).

---

## 1. Commit messages (mandatory)

Every commit message an agent proposes, writes or executes **MUST** follow
the full convention in [docs/commit-convention.md](docs/commit-convention.md).

### Required format

```text
TYPE(scope): summary
```

### Mandatory rules

1. **`TYPE` MUST be uppercase:** allowed types are `FEAT`, `FIX`, `REFACT`,
   `CHORE`, `DOCS`, `STYLE`, `TEST`, `PERF`, `SEC`, `OPS`, `REMOVE`.
   Lowercase or mixed-case types (`feat`, `Fix`, `docs`) are forbidden.
   If the user writes a lowercase type, the agent **MUST** normalize it to
   uppercase before proposing the commit.
2. **`scope` MUST be lowercase** and name the changed area. Suggested scopes:
   - `library` - sheet library, playlists, favorites, legacy import/removal
   - `playback` - playback engine, timing, speed, loop
   - `update` - self-update check, download, install
   - `store` - in-app Sheet Store
   - `settings` - settings service and persisted configuration
   - `audio` - audio output and soundpacks
   - `editor` - sheet editor window
   - `ui` - windows, widgets, welcome screen / Quick Start, themes
   - `i18n` - translation files in `assets/i18n`
   - `codec` - sheet formats (JSON, TXT, SkySheet, ABC, MIDI)
   - `core` - core models and shared utilities
   - `platform` - Windows integration, global hotkeys
   - `crash` - crash reporting
   - `net` - networking helpers
   - `installer` - Inno Setup script and `packaging/`
   - `build` - CMake, presets, vcpkg manifest
   - `release` - version bumps and release preparation
   - `docs`, `tests`, `deps`, `ci` - as named
3. **`summary` is English, imperative mood** (`add`, `fix`, `update` - not
   `added`, `fixes`, `updating`). Short, specific, action-oriented.
   **NO** vague summaries (`update code`, `fix bug`, `changes`, `refactor`).
   **NO** trailing period.
4. **Breaking changes:** add `!` after the scope
   (e.g. `FEAT(library)!: change the sheet payload format to version 3`)
   or a `BREAKING CHANGE:` footer. Breaking means: sheet/playlist file
   formats older versions cannot read, persisted settings or state schema
   changes without migration, storage layout changes.
5. **Pre-commit checklist:**
   - [ ] TYPE uppercase and from the allowed list?
   - [ ] Scope lowercase and matching the main changed area?
   - [ ] Summary English, imperative, no trailing period?
   - [ ] Summary specific, not vague?
   - [ ] Breaking change marked (if formats, schemas or storage changed)?

---

## 2. Git workflow and protecting main

### Safe sync

- Before creating a branch, fetch the latest `origin/main`.
- Before editing, check the current branch and working tree with `git status`.
- Never delete, overwrite or reset uncommitted changes.
- Do not switch branches automatically if it could disturb existing work.
- Never run `git reset --hard`, `git checkout --`, `git clean -fd` or other
  destructive commands without the user's explicit approval.
- Do not rebase shared branches without agreement from the maintainers.

### Working rules

- Do regular work on a feature branch and merge through a Pull Request.
- Branch names:
  - `feat/<short-name>` (e.g. `feat/legacy-migration`)
  - `fix/<short-name>` (e.g. `fix/welcome-language-crash`)
  - `docs/<short-name>` (e.g. `docs/agent-rules`)
  - `refact/<short-name>` (e.g. `refact/library-dedup`)
  - `chore/<short-name>` (e.g. `chore/bump-2-0-2`)
- **No direct commits/pushes to `main`** for regular work. `main` must stay
  stable and releasable.
- **Never force push** to `main`.
- Keep branches short, small and atomic to reduce conflicts.

### Commit and push authorization

- **NEVER** run `git commit` or `git push` without the user's explicit
  confirmation.
- Always present the commit message (per section 1) and the target branch
  before committing.

### Quality gates

- Build and run the related tests after changing code (section 5).
- Run the full test suite before proposing a PR when feasible.
- Distinguish pre-existing failures from failures your change caused.
- Never claim tests passed if they were not actually run; if they cannot
  run, say why and what was skipped.
- Do not "fix" unrelated code just to make tests pass.
- Do not install toolchains or dependencies without approval.

---

## 3. Scope control and minimal changes

- Only edit files directly related to the assigned task.
- Do not refactor unrelated modules without approval.
- Keep existing public APIs and file formats stable unless a breaking
  change is explicitly requested.
- Do not add new dependencies unless necessary (and never without approval -
  dependencies come through `vcpkg.json`).
- Do not change project configuration, CI workflows or git hooks without
  approval.
- Always summarize which files changed and explain the important changes.
- Ask when a request is ambiguous and a wrong assumption would cause
  large rework - especially before anything irreversible.

---

## 4. Project shape and mandatory architecture

All code changes **MUST** follow the layered architecture, CMake target graph,
include-direction rules, and persistence invariants documented in
**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**.

Summary of layout:

- `libs/skyinput` - Windows input/hotkey/window library (Win32 only, no Qt).
- `src/app` - application entry (`main.cpp`), `Application` composition root,
  and app-shell coordinators (`LegacyMigrationCoordinator`, `UpdateController`,
  `IssueRecorder`, `SkyWindowWatcher`, `SingleInstance`).
- `src/core`, `src/model` - shared core utilities (`Paths`, generated
  `core/version.hpp`, `translator`, `file_system`, `logger`, `result`) and
  neutral data models (`Toast`, `LoopMode`, `Config`, `SheetEntry`, etc.).
- `src/codec` - sheet format readers/writers (JSON, TXT, SkySheet, ABC, MIDI).
- `src/services/` - acyclic domain libraries (`sky_services`), one folder per
  domain: `audio`, `library`, `notifications`, `playback`, `report`,
  `settings`, `store`, `update`.
- `src/ui/` - Qt Widgets UI split into enforced layers:
  - `core`, `render`, `widgets`, `theme*` (`sky_ui_base`: no `services/`,
    `ui/controllers/`, or feature-window includes).
  - `controllers` (`sky_controllers`: `PlayerController`, `ThemeController`).
  - Independent feature windows: `main_window` (includes Quick Start),
    `settings_window`, `editor_window`, `issue` (no cross-window or `app/`
    includes).
- `src/platform` - Windows UI/shell integration (`sky_platform`).
- `src/net` - networking helpers (`sky_net`).
- `src/crash` - standalone out-of-process crash reporting (`sky_crash`,
  `CrashReport.exe`, no Qt).
- `assets/i18n/` - translation files. English is the source: UI strings are
  i18n keys, translated in `vi.json`, `zh_CN.json`, `ja.json`, `es.json`.
- `packaging/` - Inno Setup script (`installer.iss`) and license texts.
- `docs/` - user and developer docs (`ARCHITECTURE.md`, `INSTALL.md`,
  `USER_GUIDE.md`, `THEMES.md`, `commit-convention.md`).
- `tests/` - Catch2 tests split into `skyautopiano_core_tests` (no UI/Widgets)
  and `skyautopiano_tests` (UI/controllers/integration), registered with CTest.

Mandatory architectural rules:

- **Obey [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md):** never introduce upward
  includes, cross-window includes (`main_window` ↔ `settings_window` ↔
  `editor_window` ↔ `issue`), `ui/widgets` → `services` dependencies, or
  `QWidget` dependencies inside `services/` or `LegacyMigrationCoordinator`.
- **Keep responsibilities separated:** UI collects intent and renders state;
  controllers hold presentation state; services own validation, persistence,
  and side effects. Playback timing runs on its own high-priority thread under
  `src/services/playback/` — treat changes there as timing-sensitive and keep
  `PlaybackService` behind `IPlaybackPort`.
- **Preserve library persistence order:** imports write payloads before
  committing `listSheet.json` and roll back uncommitted payloads on failure;
  deletions commit `listSheet.json` → update `playlists.json` → delete
  unreferenced payloads; never ignore `saveList()` or `PlaylistService::save()`
  errors.
- **Run architectural & repository gates:** `tests/repo/test_repository.cpp`
  enforces registered sources, icon parity, and `#include` layer boundaries. Keep
  both `skyautopiano_core_tests` and `skyautopiano_tests` passing.

---

## 5. Build and test

Toolchain (Windows): Visual Studio 2022 Build Tools (MSVC), Qt 6 via
`QT_ROOT`, vcpkg via `VCPKG_ROOT`. CMake presets in `CMakePresets.json`:

```powershell
# Configure + build (Debug)
cmake --preset win-x64-debug
cmake --build --preset win-x64-debug

# Configure + build (Release, RelWithDebInfo)
cmake --preset win-x64-release
cmake --build --preset win-x64-release

# Package (staging + installer artifacts)
cmake --build --preset win-x64-release --target package

# Tests (headless Qt)
$env:QT_QPA_PLATFORM = 'offscreen'
ctest --preset win-x64-debug
```

Rules:

- The `win-x64-ci` preset turns warnings into errors; keep the build
  warning-clean.
- Format C++ with the repo `.clang-format` before committing.
- Never declare a build successful on an exit code alone: confirm the
  expected artifact exists, is fresh, and carries the expected content
  (e.g. the staged exe's FileVersion after a version bump, a marker string
  from the new code). A failed build that leaves stale artifacts behind
  is a failed build - report it as such.
- Known local quirk: in local Debug builds, the test
  "the last version that ran is remembered once" can crash with
  0xc0000409 (Qt ASSERT, no QCoreApplication). It passes in the CI
  release configuration. Treat it as a known local-only failure, not a
  regression - but say so explicitly when reporting test results.

---

## 6. Code, UI and i18n rules

- C++20, Qt 6 Widgets. Follow existing patterns in the touched module
  instead of introducing new frameworks or idioms.
- **Every user-facing string goes through the i18n system.** Adding or
  renaming a string means updating `assets/i18n/vi.json`, `zh_CN.json`,
  `ja.json` and `es.json` in the same change (English is the key itself,
  there is no `en.json`). A change that leaves a language file behind is
  incomplete - the i18n tests check key parity.
- Never show internal or legacy product codenames to users. In particular,
  the old Electron app is named for users by its version
  (e.g. "version 1.3.13 (old version)"), never by its framework name.
- Check new or changed UI in both dark and light themes.
- UI code must not perform destructive actions directly; route them
  through the owning service with its validation and confirmation gates.

---

## 7. Data and legacy-migration safety

- The app stores its data next to the executable (`config/`, `data/`,
  `logs/` under the install or portable folder; `SKYAUTOPIANO_HOME`
  overrides the root). Do not refactor the storage root or rename these
  folders - installed users' data depends on the layout.
- **The old Electron app's data is read-only for us.** Legacy import is
  copy / selective merge only; never modify, move or delete the source
  files.
- Legacy removal is user-initiated and gated: it may only run after a
  verified import, a fresh reconciliation, confirmation that the old app
  is not running, and an explicit user confirmation. It launches the old
  app's official uninstaller (identified by its exact Inno AppId) - never
  delete the old app's files by hand.
- Never run installers or uninstallers outside that gated flow without
  separate, explicit authorization.
- Tests use mocks, fakes and temporary fixtures only. Never run tests or
  experiments against a real user profile or a real installed copy.

---

## 8. Installer and release rules

- Version lives in the root `CMakeLists.txt`:
  `set(SKY_VERSION "x.y.z" CACHE STRING ...)`.
  **Cache trap:** an existing build tree keeps the old cached version.
  After bumping, reconfigure with `-DSKY_VERSION=<new>` (or configure
  fresh) and verify the staged exe's FileVersion before packaging.
- Release artifacts are named exactly `SkyAutoPiano-<version>-setup.exe`
  and `SkyAutoPiano-<version>-portable.zip`. The built-in updater finds
  updates by these names and verifies the SHA-256 digest GitHub reports
  for the asset - a release with renamed or missing assets silently
  breaks self-update.
- Releases are built locally and published on
  `NullVoid780/SkyAutoPianoCpp` with title `SkyAutoPiano v<x.y.z>`,
  targeting `main`. **Never publish a release whose tag points at an
  unmerged branch** - merge first, then tag `main`.
- Release notes: one summary line, then `### New` and `### Fixes`
  sections with bold-lead, user-facing English bullets. No internal
  codenames, no developer-only details.
- **Never change the Inno AppId, `DefaultDirName` or the uninstall
  identity in `packaging/installer.iss`.** The updater, the legacy
  migration and installed users all key on them. Editing the `.iss` at
  all requires the maintainer's explicit request.
- Do not create tags or releases without explicit authorization.

---

## 9. No-go list

Do not:

- Commit or push without explicit approval, or force push `main`.
- Run `git reset --hard`, `git checkout --` or `git clean -fd` on a dirty
  tree without explicit approval.
- Modify the old Electron app's files or data, in code or by hand.
- Run the old app's uninstaller outside the gated removal flow.
- Change the Inno AppId, install directory identity, storage root or
  persisted file formats without an explicit breaking-change decision.
- Violate the layered architecture or `#include` boundary rules in
  `docs/ARCHITECTURE.md` (upward includes, cross-window includes, or `QWidget`
  in services/coordinators).
- Add a user-facing string without updating all four translation files.
- Publish a release from an unmerged branch, or with assets renamed away
  from the `SkyAutoPiano-<version>-*` pattern.
- Claim a build, test or release succeeded without verifying the actual
  artifact or result.
- Fabricate experimental or test results; mark unfinished work as TODO.
