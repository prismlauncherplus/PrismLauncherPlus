# CLAUDE.md

Guidance for AI coding agents (Claude Code and others; `AGENTS.md` points here) working in this repository.

## Project

**Prism Launcher Plus** is an unofficial fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher), a Qt 6 / C++ launcher
for Minecraft that manages multiple instances. It is not endorsed by or affiliated with Prism Launcher. The fork adds three features on
top of upstream (details in [README.md](README.md)):

- **Bulk instance operations**: multi-selection, bulk rename/delete/export/regroup (`launcher/ui/instanceview/`,
  `launcher/ui/dialogs/BulkRenameDialog.*`, `launcher/InstanceDirUpdate.*`).
- **Group-level settings**: a global → group → instance settings chain (`launcher/settings/InheritableSettings.*`,
  `launcher/settings/InheritedSetting.*`, `launcher/ui/dialogs/GroupSettingsDialog.*`, group settings files in `InstanceList`).
- **Shared game options**: `options.txt` profiles shared between instances (`launcher/minecraft/gameoptions/`,
  `launcher/minecraft/launch/SyncGameOptions.*`, `launcher/ui/pages/global/GameOptionsProfilesPage.*`).

Planned work is tracked in [phases.md](phases.md). Contribution rules are in [CONTRIBUTING.md](CONTRIBUTING.md); this fork **allows
AI-assisted contributions**, but the human submitting them must review, understand and test them.

## Repository layout

| Path | Contents |
| --- | --- |
| `launcher/` | The application: `Application.*` (startup, global settings, data migration), `BaseInstance`/`MinecraftInstance`, `InstanceList`, `launch/` (launch tasks and steps), `minecraft/` (auth, mods, launch steps, game options), `modplatform/` (Modrinth, CurseForge, …), `settings/`, `ui/` (windows, dialogs, pages, widgets), `updater/` |
| `buildconfig/` | `BuildConfig` — compile-time constants (names, URLs, feature flags) generated from CMake by `BuildConfig.cpp.in` |
| `program_info/` | Branding: app ID, names, icons, desktop/metainfo files, Windows installer, man page |
| `libraries/` | Bundled libraries and the Java launcher part (`libraries/launcher`, Java package `org.prismlauncher`) |
| `tests/` | Qt Test unit tests (`*_test.cpp`, data in `tests/testdata/`) |
| `nix/`, `flake.nix` | Nix packaging (`prismlauncherplus`, `prismlauncherplus-unwrapped`) and the dev shell |
| `.github/` | CI workflows and packaging actions |

## Building and testing

The easiest environment is the Nix dev shell, which provides the toolchain (LLVM/clang, Qt 6, CMake, Ninja) and configures a build:

```shell
nix develop                       # enters the shell, inits submodules, configures a Debug build
cmake --build build               # or: cd "$cmakeBuildDir" && ninjaBuildPhase
cd build && ctest --output-on-failure
```

Without Nix, follow upstream's [build instructions](https://prismlauncher.org/wiki/development/build-instructions) (CMake presets are in
`CMakePresets.json`). Submodules: `git submodule update --init --recursive` (`libraries/libnbtplusplus`, `cmake/vcpkg`).

To build the distributable package exactly as users get it: `nix build .#prismlauncherplus` (this also runs the tests).
`nix flake check` runs the formatting checks (clang-format, nixfmt, deadnix, statix, markdownlint).

Before finishing a change:

1. It builds without new warnings.
2. `ctest` passes. Add or extend tests in `tests/` for logic that can be tested without a GUI (register new test files in
   `tests/CMakeLists.txt`).
3. Changed C++ files are formatted with `clang-format` (config in `.clang-format`); changed Nix files with `nixfmt`; Markdown passes
   `markdownlint` (config in `.markdownlint.yaml`).

## Code style

From [CONTRIBUTING.md](CONTRIBUTING.md) — follow it, `.clang-tidy` checks most of it:

- Types and enum constants `PascalCase`; functions, methods and non-const globals `camelCase`.
- Private/protected members `m_camelCase`, private/protected static members `s_camelCase`, public members `camelCase`.
- `static const` members, `const` globals and macros `SCREAMING_SNAKE_CASE`.
- No invented acronyms/abbreviations (`texturePack`, not `tp`).
- `[[nodiscard]]` only where ignoring the result is likely a bug, not on plain getters.
- Don't rename existing names that break these rules unless you are refactoring that whole class anyway.

Other conventions used in this codebase:

- User-visible strings go through `tr()` (`QObject::tr()` outside `QObject`s). Use `BuildConfig.LAUNCHER_DISPLAYNAME` rather than
  hard-coding the product name where practical.
- Use the helpers in `FileSystem.h` (`FS::PathCombine`, `FS::write`, `FS::ResolveExecutable`, …) for file operations, and `Json.h` for
  JSON.
- Settings are registered on `SettingsObject`s; instance settings that can be inherited from groups go through
  `InheritableSettings::registerCommon` / `registerMinecraft`.
- Long-running work is a `Task` (see `launcher/tasks/`); launch steps derive from `LaunchStep`.
- New source files need the GPL-3.0-only license header used by other files, and must be added to `launcher/CMakeLists.txt`.
- Keep existing license/copyright headers intact; never replace upstream copyright notices.

## Branding and identity

The product is **Prism Launcher Plus** (`PrismLauncherPlus`). Identity comes from `program_info/CMakeLists.txt` and `CMakeLists.txt`:
display name `Prism Launcher Plus`, app ID `io.github.prismlauncherplus.PrismLauncherPlus`, binary `prismlauncherplus`, env var prefix
`PRISMLAUNCHERPLUS_`, data directory `PrismLauncherPlus`.

- Files in `program_info/` deliberately **keep their upstream names** (`org.prismlauncher.PrismLauncher.*`, `prismlauncher.*`) and are
  configured/installed under the new names. Don't rename them: it makes upstream merges harder.
- Keep upstream infrastructure that the fork still uses: `meta.prismlauncher.org`, the MSA and CurseForge API keys, the translations
  service, the Java entry point package `org.prismlauncher`, `x-prismlauncher-*` packwiz keys and the `prismlauncher://` import scheme.
- Microsoft login uses the localhost redirect by default (`Launcher_MSA_URL_SCHEME_CALLBACK` is `OFF`) because upstream's MSA client ID
  only allows the `prismlauncher://` redirect.
- Don't point users at upstream's Discord, Matrix, subreddit or issue tracker for this fork; bugs go to
  <https://github.com/prismlauncherplus/PrismLauncherPlus/issues>.

## Working with upstream

- `develop` is the default branch. Upstream is `https://github.com/PrismLauncher/PrismLauncher.git` (add it as the `upstream` remote).
- Sync by merging `upstream/develop` into a branch, resolving conflicts, then building and running the tests. Conflicts are most likely in
  `MainWindow`, `InstanceList`, `InstanceView`, `VisualGroup`, `MinecraftInstance`, the settings widgets and the branding files.
- Prefer additive changes (new files, small hooks into upstream code) so upstream merges stay easy. Don't reformat or refactor upstream
  code you aren't otherwise changing.
- Don't propose AI-assisted changes to upstream Prism Launcher; it does not accept them.

## Commits and pull requests

- Commits must be signed off (`git commit -s`, Developer's Certificate of Origin, see CONTRIBUTING.md). The human submitting the work signs
  off; agents should not add a `Signed-off-by` on the user's behalf unless asked.
- Write clear commit messages describing what changed and why. Mention AI assistance in the PR description.
- Keep pull requests focused; split unrelated changes.
- Update [README.md](README.md) when a fork feature's behaviour, files it writes, or known limitations change.
