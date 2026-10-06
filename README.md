<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo.svg">
  <img alt="Prism Launcher" src="/program_info/org.prismlauncher.PrismLauncher.logo.svg" width="40%">
</picture>
</p>

<p align="center">
  <b>Prism Launcher Plus</b> — an unofficial fork of <a href="https://prismlauncher.org">Prism Launcher</a> that adds bulk instance
  operations, group-level settings and shared game option profiles.
</p>

> [!WARNING]
> ### Prism Launcher Plus is not Prism Launcher
>
> - This project is **not endorsed, sponsored, maintained or supported** by the Prism Launcher project, its maintainers or its community.
>   The Prism Launcher name, logo and other brand assets belong to that project and are only used here to identify the upstream project
>   this repository was forked from.
> - Prism Launcher Plus is **not affiliated** with the Prism Launcher Discord, Matrix space, subreddit, wiki or issue tracker. You will not
>   get help there for anything that only exists in this fork.
> - **Do not report bugs found in this fork to the Prism Launcher project.** First reproduce the problem against upstream Prism Launcher;
>   if it only happens here, report it in *this* repository.
> - Prism Launcher Plus is in turn a fork of the MultiMC Launcher, which likewise does not endorse it.
> - The installation instructions, community links, translations, build instructions and sponsors listed further down this page are
>   **Prism Launcher's**, and describe upstream Prism Launcher — not this fork.

## About this fork

Prism Launcher Plus is a launcher for Minecraft that manages multiple installations of Minecraft at once, exactly like upstream
Prism Launcher, plus three self-contained features on top of it:

- **Bulk instance operations** — the instance list supports real multi-selection, and rename, delete, change group, export, icon, open
  folder and kill can be applied to a whole selection at once.
- **Group-level settings** — a group can override the global instance settings for all of its instances, using a
  global → group → instance inheritance chain.
- **Shared game options** — named profiles of Minecraft `options.txt` settings that can be shared by instances, groups or the whole
  launcher, applied before a launch and written back when the game ends.

Apart from the code needed for those features the launcher is upstream Prism Launcher: same UI, same instance layout, same
`mmc-pack.cfg` files and same API usage. The changes are mostly additive new files, so upstream commits are generally merged into this
branch without conflict.

| | |
| --- | --- |
| Upstream project | [PrismLauncher/PrismLauncher](https://github.com/PrismLauncher/PrismLauncher) |
| This fork | [prismlauncherplus/PrismLauncherPlus](https://github.com/prismlauncherplus/PrismLauncherPlus) |
| Branched from | upstream `develop` at `50e9321f` ("Remove iOS and OSX icons (#6226)"), the 12.0.0 development line after the 9.4 release |
| Default branch | `develop` |
| Size of the diff | 14 commits over 64 files (excluding this README), roughly 5.5k added lines, mostly new files |
| License | upstream terms, see [License](#license): GPL-3.0-only for the code, CC BY-SA 4.0 for the logo and assets |
| Builds | none are published here; build it yourself (see [Building](#building)) or use upstream Prism Launcher for official builds |
| Development | the new features were written with AI assistance and reviewed by the maintainer of this fork; upstream code is unchanged in origin |

## Changes compared to upstream Prism Launcher

Everything below is the difference between this branch and the upstream commit listed above. The per-commit list, with the reasoning behind
each fix, is in the commit history (`git log 50e9321f..HEAD`); the planned work is in [phases.md](phases.md).

### 1. Bulk and multi-instance operations

New code: `launcher/ui/dialogs/BulkRenameDialog.*`, `launcher/InstanceDirUpdate.*`, plus changes in
`launcher/ui/instanceview/InstanceView.*`, `launcher/ui/MainWindow.*`, `launcher/ui/dialogs/ExportInstanceDialog.*` and
`launcher/InstanceList.*`.

| Area | Upstream Prism Launcher | Prism Launcher Plus |
| --- | --- | --- |
| Selection model | Single selection only. | Multi-selection: `Ctrl`+click toggles, `Shift`+click selects a range, `Ctrl`+`Shift`+click adds a range, rubber-band drag selects (`Ctrl` adds), `Ctrl`+`A` selects all visible instances, `Shift`+arrows extend the selection, type-to-jump selects the instance. Clicking an already selected row keeps the selection for dragging or a context menu and collapses to that row on release. |
| Activation | Double-click or `Enter` launches / opens the edit page for the current instance. | Activation is refused when two or more instances are selected, or when the activated row is not part of the selection. |
| Rename | `F2` renames the current instance inline. | `F2` with one instance renames that instance's row (the inline editor now acts on the instance being edited, not the selected one). With several instances selected it opens the **bulk rename dialog**. |
| Bulk rename dialog | Does not exist. | New dialog titled `Rename %n instance(s)`: a name pattern with `{name}` (current name) and `{n}` (position in the list) placeholders, **Find** / **Replace with** fields applied after the pattern, a live *Current name → New name* preview, an option to rename the instance folders too (pre-selected when the global `InstRenamingMode` is `PhysicalDir`), results trimmed and cut to 128 characters like a single rename. |
| Bulk rename edge cases | — | Placeholders are substituted in one pass, so a literal `{n}` inside an instance name survives. Duplicate names are allowed, as upstream does. A folder name that already exists (or an instance that is running or linked into by another instance) is refused and all such failures are reported in one warning that names were still changed. Instances whose folder — and therefore id — changed are reloaded and re-selected. There is **no undo for renames**. |
| Delete | One instance per confirmation; *Undo Last Instance Deletion* restores one instance. | One confirmation for the whole selection, with up-front checks for running games, instances linked into the batch and registered shortcuts; instances that cannot be trashed are deleted permanently. The trash history is a stack of **batches**, so one `Ctrl`+`Z` restores everything that was deleted together. |
| Change group | `Ctrl`+`G` moves one instance, writing `groups.json` once per instance. | `Ctrl`+`G` moves the whole selection in a single write. When the selection spans several groups, a `(keep current groups)` entry is prepended so that accepting the dialog cannot silently ungroup instances. Dragging a multi-selection onto a group header moves all of them. |
| Export | One instance at a time, with a file tree to pick what goes into the pack. | Exporting a selection asks for one output folder and writes **one zip per instance**, de-duplicating file names case-insensitively (`Foo.zip`, `Foo (2).zip`, …) with a single *Overwrite files?* prompt (default: No) and one combined error report if any instance fails. |
| Bulk export exclusions | — | `logs`, `crash-reports`, `.cache`, `.fabric`, `.quilt`, `gameoptions-session.json`, `.DS_Store`, `thumbs.db` and each instance's own `.packignore`, shared with the single-instance defaults. Instances that cannot be exported disable the action. |
| Change icon | Applies to the current instance. | Applies to every selected instance. |
| Kill / open folder | Current instance only. | Act on every selected instance. |
| Still single-instance | — | Launch, Edit, *Create Shortcut*, Copy and the Modrinth / CurseForge mrpack exports are deliberately **not** bulk: they are disabled for a multi-selection. |
| View housekeeping | — | Instances hidden inside collapsed groups are deselected automatically (collapse, regroup and selection changes), `setSelection()` selects a rubber-band range in one pass, toolbar and context menu show `%n instance(s) selected`, the toolbar falls back to the default icon for a mixed-icon selection, and instance/tool buttons refresh when *any* selected instance changes. |

### 2. Group-level settings and overrides

New code: `launcher/settings/InheritableSettings.*`, `launcher/settings/InheritedSetting.*`,
`launcher/ui/dialogs/GroupSettingsDialog.*`, plus changes in `launcher/settings/Setting.*`,
`launcher/settings/OverrideSetting.*`, `launcher/settings/PassthroughSetting.*`, `launcher/BaseInstance.*`, `launcher/InstanceList.*`,
`launcher/ui/instanceview/VisualGroup.*`, `launcher/ui/widgets/{Minecraft,Java}SettingsWidget.*`,
`launcher/minecraft/launch/AutoInstallJava.cpp` and `launcher/launch/steps/CheckJava.cpp`.

| Area | Upstream Prism Launcher | Prism Launcher Plus |
| --- | --- | --- |
| Group settings | Groups are only a grouping/visual concept. Settings exist globally and per instance (`mmc-pack.cfg`). | A group can override the global settings for all of its instances, reached from a gear button in the group header (dimmed while the group overrides nothing) or *Group &settings* in the group header's context menu. |
| Dialog | Does not exist. | Reuses the normal instance settings widget (General, Java, Tweaks, Custom Commands, Environment Variables) with a checkbox per section to enable that override. Settings that only make sense per instance are hidden: default account, auto-join, global data packs, mod download override, *Count game time*, *Always use the latest Minecraft version* and the Java installer button (detect/test/browse still work). |
| Inheritance chain | Global → instance. | **Global → group → instance.** Lookups resolve on every access, so moving an instance between groups immediately changes what it inherits. An instance that does not override writes through to the group layer, or to the global layer when it has no group. A section is "overridden" if either the instance or its group overrides it. |
| Overridable settings | — | Game & window, console window, game time, legacy tweaks, native libraries (OpenAL / GLFW / SDL, with custom paths), performance (gamemode, MangoHud, discrete GPU, Zink), Java installation (path, compatibility check), memory (min/max/permgen/low-memory warning), JVM arguments, pre-load / pre-launch / wrapper / post-exit commands, environment variables, and the game options profile plus its review toggle. |
| Storage | — | One INI file per group, `groupsettings/group-<percent-encoded name>.cfg` in the launcher data directory, created lazily on the first override. Names are case-unambiguous (upper-case letters are encoded) so they cannot collide on case-insensitive file systems or with Windows reserved names, and fall back to `grouphash-<sha1>.cfg` beyond 200 characters. |
| Group lifecycle | — | Renaming a group moves its settings file (and drops a stale file left by an earlier group of that name), deleting a group deletes it and warns that its instances fall back to the global settings. Files written by the first, prefix-less naming scheme are migrated once on startup, tracked by a `groupsettings/.migrated` marker. |
| Override semantics | A value equal to the inherited one is not stored, so the override silently follows the parent when the parent changes. | An override **is** stored even when its value equals the inherited value — including an explicit empty value such as "no game options profile" — so it keeps its meaning when the parent changes. Side effect: existing instance configs can gain extra keys after editing instance settings. |
| Cached Java info ("passthrough") | The cached Java signature, architecture, real architecture, version and vendor are written to every layer, which repeatedly invalidated the shared cache and made every launch re-check Java for every instance. | Those values are written **only** to the layer that owns the Java path. An instance using the global Java shares the global cache, a group with its own Java caches into the group file, an instance with its own Java caches into its `mmc-pack.cfg`. |
| Automatic Java | An automatically selected Java always wins over a group setting. | If an instance uses an automatic Java and its group defines a Java that actually resolves, the instance override is dropped and the group's Java is used (logged as such). A bare name such as `java` is resolved through `PATH` like in `CheckJava`, for both the group's and the instance's Java, and the "Java not found" error now points at the instance's **or its group's** settings. |
| UI details | — | Long group names are elided so the header settings button stays clickable; settings objects are only written when a value really changes. |

### 3. Shared game options

New code: `launcher/minecraft/gameoptions/` (`OptionsFile`, `GameOptionsProfile`, `GameOptionsProfileList`, `GameOptionsCompat`,
`GameOptionsMerger`, `GameOptionsSync`), `launcher/minecraft/launch/SyncGameOptions.*`,
`launcher/ui/dialogs/GameOptionsReviewDialog.*`, `launcher/ui/pages/global/GameOptionsProfilesPage.*`, plus changes in
`launcher/minecraft/MinecraftInstance.cpp`, `launcher/Application.*`, `launcher/InstanceCopyTask.cpp`,
`launcher/launch/steps/QuitAfterGameStop.cpp` and the Minecraft settings widget.

| Area | Upstream Prism Launcher | Prism Launcher Plus |
| --- | --- | --- |
| Concept | Every instance has its own `options.txt`; nothing is shared between instances. | A **game options profile** is a named set of client options (video, controls, sound, keybinds, …) that instances, groups or the whole launcher can share. It is opt-in and holds no values of its own until used. |
| Assignment | — | A profile is picked in the global settings, in a group's settings or per instance, through a new *Shared Game Options* section that is inheritable like every other setting. The dropdown offers *None*, labels profiles with the Minecraft version they target, warns when that version differs from the instance's, reports a deleted selection as *Missing profile*, and says which layer a profile is inherited from. A *Manage Profiles…* button sits next to it in all three places. |
| Profile manager | Does not exist. | New **Game Options** page in the launcher settings: create, duplicate, rename and delete profiles; import an instance's options into a new or the selected profile (with warnings for same-named instances, version mismatches and unknown versions); set or clear the Minecraft version a profile targets; see which global settings, groups and instances use a profile; and inspect the stored options read-only as *Option / Value / Saved by*. Options are **not** editable there — values change in game or by import. Deleting a profile switches everything using it to *no profile* (instead of silently falling back) and lists the affected users in the confirmation. |
| Applying | — | A new `SyncGameOptions` launch step runs just before the game starts, merging the profile into the instance's `options.txt`. It cannot be aborted. |
| Writing back | — | When the launch ends in any way — normal exit, crash, kill or failed start — the options changed in game are written back to the profile, so a shared profile follows what you actually changed in game. Options the profile did not supply are added to it, which is what seeds new and empty profiles. *Quit after game stop* now finishes the launch first so the write-back happens before quitting. |
| Review | — | Optional dialog when the game closes, **off by default** and inheritable like the profile: changed options first, then new ones, with before/after values, Select All/None, *Save Selected* and *Discard All*. Closing the dialog (or the launcher) keeps the changes for later; discarding drops them. |
| Version compatibility | — | Values are stored per **shape** (number, quoted, structured, word) and per client data version, so a value whose format changed between Minecraft versions — key codes vs key names, `ao` as a number vs a boolean, bare vs quoted strings, Forge keybinds with modifiers — is never written to a client that cannot read it. If the client already has the option, its own value decides the shape; otherwise the value from the closest data version wins, falling back to the most recently stored one. |
| Excluded keys | — | Never shared and never overwritten: `version`, `resourcePacks`, `incompatibleResourcePacks`, `lastServer`, `startedCleanly`. Dismissed-prompt/tutorial options *are* shared so they are only shown once. Removals are not written back, and numbers differing only in precision are not reported as changes. |
| Safety | — | Whenever a profile other than the one used last time replaces values the instance already had, the old file is copied to `options.txt.before-profile-<timestamp>`; if that backup fails the profile is not applied. If `options.txt` is a symlink the profile is skipped with a warning (other instances share that file). Profiles are written under a 1 s lock so two instances writing back at once each keep their own changes, and a failed write is retried later instead of blocking. A profile deleted on disk is never resurrected by a write-back. |
| Crash recovery | — | Pending changes live in `gameoptions-session.json` next to the instance (profile, detected client format, snapshot, game process id and start time) until they are saved or discarded. If the launcher crashes or quits mid-game the sessions are recovered on the next start; sessions of games that are still running are left alone, with pid reuse guarded by the process start time. A leftover session is finished before the profile is applied, and if it still cannot be saved the new launch keeps collecting into the older session instead of overwriting it. Sessions are excluded from instance copy and export. |
| Missing or unknown version | — | For an instance without `options.txt` the data version is read from `version.json` in the client jar. If it cannot be determined, nothing is applied on that launch — the game creates the file itself and its options are added to the profile afterwards. On an empty file a `version` entry is written so Minecraft does not "upgrade" the values. |
| Parsing | — | `options.txt` parsing is lossless: untouched content, unknown keys, malformed lines, CRLF, byte order marks and duplicate keys are preserved byte for byte. |

### Files this fork adds to your system

| Path | Contents | Notes |
| --- | --- | --- |
| `<data dir>/groupsettings/group-<encoded name>.cfg` | a group's setting overrides | one file per group, created lazily, follows renames and is deleted with the group |
| `<data dir>/groupsettings/.migrated` | marker for the one-time group settings filename migration | harmless if deleted; the migration is simply retried |
| `<data dir>/gameoptions/<uuid>.json` | one game options profile per file | launcher data, not portable between launchers |
| `<instance>/gameoptions-session.json` | pending game option write-back of a launch | deleted once saved or discarded; excluded from copy and export |
| `<instance>/options.txt.before-profile-<timestamp>` | backup of `options.txt` before a profile replaced values the instance already had | only when a profile other than the one used last time changes them |

### Tests

| Test binary | Source | Covers |
| --- | --- | --- |
| `InheritedSetting` | [tests/InheritedSetting_test.cpp](tests/InheritedSetting_test.cpp) | the global → group → instance chain, override gates, following group changes, passthrough writes to the owning layer, overrides equal to the parent, empty values surviving a reload, game options profile selection |
| `GameOptions` | [tests/GameOptions_test.cpp](tests/GameOptions_test.cpp) | lossless `options.txt` parsing, shape and keybind-format detection, profile import and JSON round-trip, merging into newer, older and brand new clients, write-back change collection, profile list persistence, concurrent write-backs, session file round-trip |

The bulk-operations and group-settings **UI** code is not covered by automated tests, and none of these features has been reviewed by
the Prism Launcher project or by anyone outside this repository.

### Known limitations

- The multi-selection, bulk rename/export and group-settings dialogs are untested by automated tests.
- Game options profiles are not portable (they live in the launcher data directory) and are not exported with an instance.
- Editing instance settings may now add keys to `mmc-pack.cfg` that equal the inherited value; this is intentional but changes the file.
- Bulk folder renames have no undo, and a rename can be refused because the target folder exists.
- Features listed as single-instance above stay single-instance by design.
- Syncing with upstream `develop` is manual; conflicts are possible in `MainWindow`, `InstanceList`, `InstanceView` and
  `MinecraftInstance`.

## Reporting issues in this fork

Report bugs and feature requests **here**, in [prismlauncherplus/PrismLauncherPlus](https://github.com/prismlauncherplus/PrismLauncherPlus),
and not in the upstream tracker. Include your Prism Launcher Plus version, the launcher log, the instance type and version, and whether the
problem also happens in upstream Prism Launcher. The Prism Launcher Discord, Matrix space and subreddit cannot help with anything that
only exists in this fork.

## Installation

**No builds of Prism Launcher Plus are published.** The following downloads, instructions and badges are Prism Launcher's and install
upstream Prism Launcher, which does not contain the features described above.

- All downloads and instructions for Prism Launcher can be found on our [Website](https://prismlauncher.org/download).
- Last build status can be found in the [GitHub Actions](https://github.com/PrismLauncher/PrismLauncher/actions) tab (this also includes the pull requests status).
- To build Prism Launcher Plus yourself, follow the upstream [build instructions](https://prismlauncher.org/wiki/development/build-instructions) and build this repository's `develop` branch.

<p align="center">
<a href="https://repology.org/project/prismlauncher/versions">
    <img src="https://repology.org/badge/vertical-allrepos/prismlauncher.svg?columns=3" alt="Packaging status">
</a>
</p>

### Development Builds

Please understand that these builds are not intended for most users. There may be bugs, and other instabilities. You have been warned.

There are development builds available through:

- [GitHub Actions](https://github.com/PrismLauncher/PrismLauncher/actions) (includes builds from pull requests opened by contributors)
- [nightly.link](https://prismlauncher.org/nightly) (this will always point only to the latest version of develop)

These have debug information in the binaries, so their file sizes are relatively larger.

Prebuilt Development builds are provided for **Linux**, **Windows** and **macOS**.

On Linux, we also offer our own [Flatpak nightly repository](https://github.com/PrismLauncher/flatpak). Most software centers are able to install it by opening [this link](https://flatpak.prismlauncher.org/prismlauncher-nightly.flatpakref).

## Community & Support

This is the **upstream Prism Launcher community**. It does not support Prism Launcher Plus, and asking there about the features listed
above will only be redirected back to this repository. For this fork, use the issue tracker linked in
[Reporting issues in this fork](#reporting-issues-in-this-fork).

Feel free to create a GitHub issue if you find a bug or want to suggest a new feature. We have multiple community spaces where other community members can help you:

- **Our Discord server:**

[![Prism Launcher Discord server](https://discordapp.com/api/guilds/1031648380885147709/widget.png?style=banner3)](https://prismlauncher.org/discord)

- **Our Matrix space:**

[![Prism Launcher Space](https://img.shields.io/matrix/prismlauncher:matrix.org?style=for-the-badge&label=Matrix%20Space&logo=matrix&color=purple)](https://prismlauncher.org/matrix)

- **Our Subreddit:**

[![r/PrismLauncher](https://img.shields.io/reddit/subreddit-subscribers/prismlauncher?style=for-the-badge&logo=reddit)](https://prismlauncher.org/reddit)

## Translations

The translation effort for Prism Launcher is hosted on [Weblate](https://hosted.weblate.org/projects/prismlauncher/launcher/) and information about translating Prism Launcher is available at <https://github.com/PrismLauncher/Translations>.

Prism Launcher Plus is not part of that translation project, so the strings added by this fork (bulk rename, group settings, game options
profiles) are only available in English unless translations are contributed here.

## Building

If you want to build Prism Launcher Plus, check the upstream [build instructions](https://prismlauncher.org/wiki/development/build-instructions) and build this repository's `develop` branch. The unit tests are built when CMake's `BUILD_TESTING` is enabled and cover the settings inheritance and the game options engine.

Building this fork with the API keys that ship in [CMakeLists.txt](CMakeLists.txt) means using them under the terms listed in
[Forking/Redistributing/Custom builds policy](#forkingredistributingcustom-builds-policy) below; set them to `""` if you do not accept them.

## Sponsors & Partners

We thank all the wonderful backers over at Open Collective! Support Prism Launcher by [becoming a backer](https://opencollective.com/prismlauncher).

[![OpenCollective Backers](https://opencollective.com/prismlauncher/backers.svg?width=890&limit=1000)](https://opencollective.com/prismlauncher#backers)

Thanks to JetBrains for providing us a few licenses for all their products, as part of their [Open Source program](https://www.jetbrains.com/opensource/).

<a href="https://jb.gg/OpenSource">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://www.jetbrains.com/company/brand/img/logo_jb_dos_4.svg">
  <source media="(prefers-color-scheme: light)" srcset="https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg">
  <img alt="JetBrains logo" src="https://resources.jetbrains.com/storage/products/company/brand/logos/jetbrains.svg" width="40%">
</picture>
</a>

Thanks to Weblate for hosting our translation efforts.

<a href="https://hosted.weblate.org/engage/prismlauncher/">
<img src="https://hosted.weblate.org/widgets/prismlauncher/-/open-graph.png" alt="Translation status" width="300" />
</a>

Thanks to Netlify for providing us their excellent web services, as part of their [Open Source program](https://www.netlify.com/open-source/).

<a href="https://www.netlify.com"> <img src="https://www.netlify.com/v3/img/components/netlify-color-accent.svg" alt="Deploys by Netlify" /> </a>

Thanks to the awesome people over at [MacStadium](https://www.macstadium.com/), for providing M1-Macs for development purposes!

<a href="https://www.macstadium.com"><img src="https://uploads-ssl.webflow.com/5ac3c046c82724970fc60918/5c019d917bba312af7553b49_MacStadium-developerlogo.png" alt="Powered by MacStadium" width="300"></a>

## Forking/Redistributing/Custom builds policy

Prism Launcher Plus follows this policy unchanged, and applies it to itself: this repository is a fork, it is not Prism Launcher, and it
is not endorsed by or affiliated with the Prism Launcher project (<https://prismlauncher.org>).

You are free to fork, redistribute and provide custom builds as long as you follow the terms of the [license](LICENSE) (this is a legal responsibility), and if you made code changes rather than just packaging a custom build, please do the following as a basic courtesy:

- Make it clear that your fork is not Prism Launcher and is not endorsed by or affiliated with the Prism Launcher project (<https://prismlauncher.org>).
- Go through [CMakeLists.txt](CMakeLists.txt) and change Prism Launcher's API keys to your own or set them to empty strings (`""`) to disable them (this way the program will still compile but the functionality requiring those keys will be disabled).

If you have any questions or want any clarification on the above conditions please make an issue and ask us.

If you are just building Prism Launcher for your distribution, please make sure to set the `Launcher_BUILD_PLATFORM` to a slug representing your distribution. Examples are `archlinux`, `fedora` and `nixpkgs`.

Note that if you build this software without removing the provided API keys in [CMakeLists.txt](CMakeLists.txt) you are accepting the following terms and conditions:

- [Microsoft Identity Platform Terms of Use](https://docs.microsoft.com/en-us/legal/microsoft-identity-platform/terms-of-use)
- [CurseForge 3rd Party API Terms and Conditions](https://support.curseforge.com/en/support/solutions/articles/9000207405-curse-forge-3rd-party-api-terms-and-conditions)

If you do not agree with these terms and conditions, then remove the associated API keys from the [CMakeLists.txt](CMakeLists.txt) file by setting them to an empty string (`""`).

## License [![https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE](https://img.shields.io/github/license/PrismLauncher/PrismLauncher?label=License&logo=gnu&color=C4282D)](LICENSE)

All launcher code is available under the GPL-3.0-only license.

The logo and related assets are under the CC BY-SA 4.0 license.

Prism Launcher Plus keeps these upstream terms unchanged; as a derivative work it is distributed under the same license. The Prism
Launcher name, logo and brand assets remain the property of the Prism Launcher project, which does not endorse this fork — see the
notice at the top of this file.
