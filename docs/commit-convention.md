# Commit Message Convention

This project follows a structured commit message convention to keep the history readable, searchable, and easy for both humans and AI agents to follow.

## Format

All commit messages must use the following format:

```txt
TYPE(scope): summary
```

TYPE is mandatory and MUST be uppercase.

### Valid / Invalid Examples

**Valid:**
- `FEAT(library): verified legacy import with conflicts, migration state and snapshot`
- `FIX(i18n): add missing translations for legacy migration strings`
- `DOCS(readme): add local development setup instructions`

**Invalid:**
- `feat(library): verified legacy import`
- `Fix(i18n): add missing translations`
- `docs(readme): add local development setup instructions`

### General Rules

1. **TYPE** is mandatory and MUST be uppercase. Lowercase or mixed-case types (e.g., `feat`, `Fix`) are strictly prohibited.
2. **scope** must be lowercase.
3. **summary** should be in English.
4. **summary** should be concise and use the imperative mood (e.g., "add", "fix", "change", not "added", "fixes", "changing").
5. Do not end the summary with a period.
6. Prefer one logical change per commit.

---

## Allowed Types

Allowed types are:

- **FEAT**: A new user-facing or system feature
  - *Example:* `FEAT(library): optional removal of the legacy Electron version, gated on verified import`
- **FIX**: A bug fix
  - *Example:* `FIX(ui): stop the crash when switching the welcome language back to system`
- **REFACT**: A code refactoring without intended behavior changes
  - *Example:* `REFACT(library): share the deduplicated legacy rows between import and removal`
- **CHORE**: Maintenance, dependency updates, cleanup, release bookkeeping
  - *Example:* `CHORE(release): bump version to 2.0.2`
- **DOCS**: Documentation-only changes
  - *Example:* `DOCS(readme): announce end of updates for the old version`
- **STYLE**: Formatting or visual/styling-only changes
  - *Example:* `STYLE(ui): polish settings card spacing`
- **TEST**: Adding or modifying tests
  - *Example:* `TEST(library): cover conflict detection for byte-different duplicates`
- **PERF**: A performance optimization
  - *Example:* `PERF(playback): reduce allocations in the scheduling loop`
- **SEC**: Security improvements or vulnerability patches
  - *Example:* `SEC(update): require a SHA-256 digest before running a downloaded installer`
- **OPS**: Infrastructure, CI, build or packaging automation
  - *Example:* `OPS(ci): add warnings-as-errors preset to the workflow`
- **REMOVE**: Removing deprecated features, code, or configuration
  - *Example:* `REMOVE(ui): remove the manual import button from Settings`

*Note: Lowercase or mixed-case types are not allowed.*

---

## Recommended Scopes

Use the most specific scope that describes the main changed area. If a change spans multiple areas, use the primary one.

- `library` - Sheet library, playlists, favorites, legacy import and removal (`src/services/library`)
- `playback` - Playback engine, timing, speed and loop control (`src/services/playback`)
- `update` - Self-update checks, download and install (`src/services/update`)
- `store` - In-app Sheet Store browsing and downloads (`src/services/store`)
- `settings` - Settings service and persisted configuration (`src/services/settings`)
- `audio` - Audio output and soundpacks (`src/services/audio`)
- `editor` - Sheet editor window (`src/ui/editor_window`)
- `ui` - Windows, widgets, welcome screen / Quick Start, themes and styles (`src/ui`)
- `i18n` - Translation files and language handling (`assets/i18n`)
- `codec` - Sheet format readers and writers: JSON, TXT, SkySheet, ABC, MIDI (`src/codec`)
- `core` - Core models, shared utilities (`src/core`, `src/model`)
- `platform` - Windows integration, global hotkeys, single-instance (`src/platform`)
- `crash` - Crash reporting (`src/crash`)
- `net` - Networking helpers (`src/net`)
- `installer` - Inno Setup script and packaging (`packaging/`)
- `build` - CMake, presets, vcpkg manifest, build scripts
- `release` - Version bumps and release preparation
- `docs` - Project documentation or developer guides
- `tests` - Test code, fixtures and test infrastructure
- `deps` - Dependency changes (`vcpkg.json`, Qt)
- `ci` - Continuous integration workflows

### Scope Examples

- `FEAT(library): verified legacy import with conflicts, migration state and snapshot`
- `FIX(library): deduplicate the legacy source during removal reconciliation`
- `FEAT(ui): add language selection to the welcome screen`
- `FIX(i18n): add missing translations for legacy migration strings`
- `CHORE(license): merge the copyright lines and show LICENSE in the installer`
- `FIX(build): stage the root LICENSE for the installer info page`

---

## Summary Style

Summaries must be descriptive and action-oriented. Avoid vague descriptions.

- **Good:**
  - `FIX(ui): hide the legacy cleanup UI once the old version is removed`
  - `FEAT(ui): legacy cleanup gets its own step and automatic verification`
  - `DOCS(readme): add local development setup instructions`
- **Bad:**
  - `FIX(ui): fix bug`
  - `CHORE: update`
  - `FEAT: changes`
  - `REFACT(library): improve stuff`

---

## Commit Body

For larger or more complex commits, use an optional body to explain the details.

```txt
FEAT(library): verified legacy import with conflicts, migration state and snapshot

- detect conflicts by normalized name/author key with byte-different payloads
- record versioned migration state with a content fingerprint
- snapshot the destination library before importing
```

- Separate the summary from the body with a blank line.
- Use bullet points for multi-part changes.
- Explain *why* the change was made if the reason is not obvious.
- Keep the body concise.

---

## Breaking Changes

To indicate a breaking change, add a `!` after the scope, or add a `BREAKING CHANGE:` footer at the bottom of the commit body.

### Syntax 1: Using `!` after scope

```txt
FEAT(library)!: change the sheet payload format to version 3
```

### Syntax 2: Using a footer

```txt
FEAT(settings): rename the persisted theme key

BREAKING CHANGE: Settings files written by older versions lose the saved theme.
```

### When to use Breaking Changes

- Sheet or playlist file format changes that older versions cannot read
- Persisted settings or `ui-state.json` schema changes without migration
- Storage layout changes under the app's data root
- Removed features or changed default shortcuts

---

## Multi-Area Changes

To keep the git history clean, prefer splitting changes across separate commits:

1. `FEAT(library): add the legacy removal service`
2. `FEAT(ui): surface legacy removal on the welcome screen`
3. `TEST(library): cover the removal safety gates`
4. `DOCS(user-guide): document moving from the old version`

*Note: If the change is small and tightly coupled, one single commit is acceptable using the primary scope.*

---

## For AI Agents

AI agents cooperating in this workspace must follow these rules:

1. **Adherence:** Always suggest commit messages that follow this convention.
2. **Precision:** Never suggest vague summaries (e.g. `update`, `bug fix`, `refactor`).
3. **Translations:** A change that adds or renames user-facing strings is not complete until every language file in `assets/i18n` is updated in the same commit or a paired `FIX(i18n)` commit.
4. **Breaking Changes:** Explicitly mark any breaking changes with the `!` scope modifier or the `BREAKING CHANGE:` footer.
5. **Casing Rules:**
   - Always write `TYPE` in uppercase.
   - Never suggest lowercase or mixed-case commit types.
   - If the user provides a lowercase type, normalize it to uppercase and mention the normalized message.

---

## Optional Validation

If commit linting is added later, enforce:
- `type-case`: uppercase
- `type-enum`: `FEAT`, `FIX`, `REFACT`, `CHORE`, `DOCS`, `STYLE`, `TEST`, `PERF`, `SEC`, `OPS`, `REMOVE`
- `scope-case`: lowercase
- `subject-case`: sentence-case is not required; keep summaries concise and imperative

*Note: Do not add commitlint config unless explicitly requested. Only document the rules.*
