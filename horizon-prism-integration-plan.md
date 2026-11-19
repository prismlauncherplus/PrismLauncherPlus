# Horizon in Prism Launcher Plus: integration plan

**Status:** draft (revised after checking against the Horizon repository and this fork)
**Scope:** Prism Launcher Plus
**Upstream projects:** [PrismLauncher/PrismLauncher](https://github.com/PrismLauncher/PrismLauncher), [horizon-rust/horizon](https://github.com/horizon-rust/horizon)

In this plan, "the data directory" means Prism Launcher Plus's own launcher data directory (for example
`~/.local/share/PrismLauncherPlus`), not upstream Prism's.

## 1. Goal

Horizon instances should sit in the instance list next to Java Edition instances. From the launcher, a player should be able to:

- create, launch, group, copy and delete a Horizon instance;
- manage its worlds, servers, resource packs, data packs, shader packs and screenshots;
- have the launcher fetch everything the game needs: Horizon builds, the Mojang files and the prepared resources;
- later, manage Horizon mods.

The player should never need Horizon's own launcher, and we should not reimplement the game.

### Non-goals

- Running Java mods in Horizon. Horizon does not run Java mod jars.
- Replacing Horizon's own launcher, or matching all of its features (server hosting, Tailscale, telemetry UI).
- Redistributing any Mojang files. Everything is downloaded per user from Mojang, as Prism already does.

## 2. Background: what Horizon needs to run

Horizon is a native Rust client for Minecraft: Java Edition 26.3. It publishes a **Launch API** for third-party
launchers and names Prism Launcher as one of them: `docs/api/launch.md` (the contract), `docs/api/launch-guide.md`
(step by step) and `docs/api/launch-reference.md` (every option). This integration is built on that API.

| Topic | Horizon behaviour | Source |
| --- | --- | --- |
| Launch API | `horizon-client --launch-api` and `horizon-prepare --launch-api` each print one JSON object: `launch_api` (version, 0.1.x today; newer builds in the same line only add), the accepted `options`, and for the preparer the `minecraft` version and `java_major` it needs, plus `schema` / `output_version` of the resources it writes | `docs/api/launch.md` |
| Unknown options | Any option a build does not list is refused with **exit code 2**. Only pass options the `--launch-api` reply lists | `docs/api/launch-reference.md` |
| Game binary | `horizon-client`, shipped with `horizon-prepare`, `research-lock.json` and `exporters/` in each build folder. Keep the folder whole | `docs/api/launch-guide.md` |
| Game folder | `--game-dir DIR`: saves, `options.txt`, `servers.dat`, resource packs, shader packs, mods, screenshots. `--home DIR`: only `logs/latest.log` and `crash-reports/` (default `HORIZON_HOME`, else the platform data folder). `--player-dir` exists for several players in one game folder; we don't need it | `docs/api/launch-reference.md` |
| World format | Vanilla Anvil (`level.dat`, region files) | `docs/native-client.md` |
| Resources | The client loads *prepared* resources (`--resources PATH`) made by `horizon-prepare` from the owner's own 26.3 files. One prepared folder serves every instance of the same build | `docs/api/launch-guide.md` |
| Preparation inputs | Either `--minecraft-dir` (official launcher layout), or each file named: `--version-json`, `--client-jar`, `--libraries-dir` (Maven layout), `--assets-dir` (`indexes/`, `objects/`). The first `--prepare` also needs `--server-jar`. Every file is checked against the pins in `research-lock.json`. The preparer also has `--prism-dir` (reads a Prism/MultiMC root or instance as is), but it is not in the versioned Launch API list | `docs/api/launch.md`, `crates/horizon-prepare/src/main.rs` |
| Preparation | `--prepare --output DIR --complete --progress json`. `--complete` also copies the sounds and fails unless the result passes `--check`. JSON progress lines: `{"message"}`, `{"fraction"}`, `{"error"}` (last line, exit 1). One preparation per folder at a time; a second waits | `docs/api/launch.md` |
| Check | `horizon-prepare --check --output DIR`: fast, reads no game files. Exit 0 means current; exit 1 lists the gaps. Run it before every start and after installing a build | `docs/api/launch-guide.md` |
| Java | A Java of `java_major` (25), passed with `--java-home`. A runtime is enough; no full JDK needed | `docs/api/launch.md` |
| Auth | Session JSON `{"name","uuid","access_token","user_type":"msa"}`, `xuid` optional. Preferred: `--session-stdin` (write it to stdin and close). Alternative: `--session-file` (read once, deleted). Never on the command line. A bad session is reported on stderr and the game still starts offline | `docs/api/launch.md` |
| Owner gate | The launch guide asks other launchers to start Horizon only for a Microsoft account that owns Java Edition, the same gate `horizon-launcher` applies | `docs/api/launch-guide.md` §0 |
| Quick play | `--quickPlaySingleplayer WORLD`, `--quickPlayMultiplayer HOST[:PORT]` | `docs/api/launch-guide.md` |
| Exit codes | 0 quit; 1 error (last stderr line names the crash report); 2 refused command line; 101 panic (treat as 1) | `docs/api/launch-guide.md` |
| Releases | GitHub releases of `horizon-rust/horizon`. Five archives: Windows x64 and ARM64, Linux x86_64, macOS arm64 and x64. The newest is tagged `nightly`; each is also `nightly-<date>`, **deleted after seven days**. `SHA256SUMS` signed with Ed25519 in `SHA256SUMS.sig` over `horizon release <tag>\n` + the file; keys in `crates/horizon-release-sign/release-keys.txt` | `docs/api/launch-guide.md` |
| macOS | Builds are signed ad hoc; the launcher clears the quarantine flag | `docs/api/launch-guide.md` |
| Mods | Mods are `.hmod` files; `.hpack` is a modpack. Content mods load only when `horizon-mods.txt` enables them. The registry is a signed JSON index. Native Rust mods are compiled into the game. WebAssembly mods no longer run. No Java mods | `docs/api/mod-files.md`, `docs/api/mod-index.md`, `docs/modding/` |
| Shader packs | Supported again: `.hshader` files and `shaders/` zips | `docs/api/shaders.md` |
| Telemetry | Without consent nothing is collected, but an anonymous playing-count heartbeat is still sent. `HORIZON_NO_HEARTBEAT=1` keeps a run out of the count; `HORIZON_TELEMETRY_URL=off` turns the count off | `docs/telemetry.md` |
| License | **LGPL-3.0-or-later** (code from before the switch stays MIT). Compatible with this GPL-3.0 fork, but keep Horizon's notices on anything we port | `LICENSE` |

## 3. Architecture overview

```text
Launcher UI ─┬─ Instance list (InstanceType=Horizon) ─► HorizonInstance : MinecraftInstance
             │                                            ├─ PackProfile (net.minecraft 26.3, no loaders)
             │                                            ├─ WorldList, ResourcePack/DataPack/ShaderPack models (reused)
             │                                            ├─ HorizonModModel (new, phase 4)
             │                                            └─ createLaunchTask() ─► Horizon launch steps
             │
             └─ Shared data (in the data directory)
                  ├─ assets/, libraries/                  (Mojang files, existing)
                  ├─ horizon/versions/<tag>/               (Horizon builds, new)
                  ├─ horizon/server-jars/26.3/server.jar   (new)
                  └─ horizon/prepared/<tag>/               (prepared resources, new)
```

Launch steps for a Horizon instance. Generic steps are reused from the Java launch task; the order matches it.

1. Pre-launch command (existing)
2. Account claim and the owner check (existing; refuse offline accounts, see 4.6)
3. Create the game folder (existing `CreateGameFolders`)
4. Update the `net.minecraft` 26.3 component: client jar, libraries, assets (existing)
5. Offline-library check (existing, for offline starts with a cached account)
6. `HorizonEnsureBuild`: install or verify the instance's Horizon build, then read its `--launch-api` replies (new)
7. `HorizonEnsureJava`: find or download Java `java_major` (25), independent of the instance and group Java settings (new)
8. `HorizonEnsureServerJar`: download and verify the 26.3 server jar (new)
9. `HorizonPrepare`: `horizon-prepare --check`; on exit 1, prepare (new)
10. Server address lookup for quick-join (existing)
11. `SyncGameOptions`: only if the instance opted in (see 4.10)
12. Pre-load command (existing)
13. `HorizonLaunch`: start `horizon-client` and write the session to its stdin (new; replaces the Java launch steps)
14. Post-exit command (existing)

## 4. Work items

### 4.1 Instance type (first milestone blocker)

Today the loader cannot hold a second instance type:

- `InstanceList::loadInstance()` skips any `InstanceType` other than `OneSix`.
- `MinecraftInstance::loadSpecificSettings()` rewrites `InstanceType` back to `OneSix`.

Work:

- Add `HorizonInstance`, subclassing `MinecraftInstance`.
- Add an instance factory keyed by `InstanceType`, used by `InstanceList`, the instance creation task, the copy task and the import tasks. Stop `loadSpecificSettings()` from overwriting a non-`OneSix` type.
- The `PackProfile` holds only `net.minecraft` 26.3. Block adding loaders and other components, and refuse other Minecraft versions until Horizon supports them. Take the version from the build's `--launch-api` `minecraft` field rather than hard-coding it.
- Add instance settings `HorizonVersion` (a tag, or `follow-nightly`) and `HorizonExtraArgs`.
- Audit Java assumptions. There is only one `dynamic_cast<MinecraftInstance*>`; the real work is the callers of methods that cannot be overridden:
  - `getPackProfile()` (about 54 call sites);
  - the `JavaPath`, `JvmArgs`, memory and Java version check settings. The launch controller validates `JvmArgs` before every launch, so an inherited bad value would block a Horizon start;
  - the mod loader UI and the component editor.
  Override what can be overridden and guard the rest with `isHorizon()` or an instance capability flag.

*Why subclass rather than write a new `BaseInstance`:* it gives us the world, pack, screenshot and server pages and the Mojang download pipeline for free. The cost is the audit above.

### 4.2 Game folder

- Pass `--game-dir <gameRoot()>` and `--home <gameRoot()>`. Every instance then has its own saves, options, packs, logs and crash reports, in the same folders Prism already uses (Horizon follows vanilla's names).
- Do not rely on `HORIZON_HOME`: it only moves logs and crash reports.
- No `sessions/` folder is needed when the session goes over stdin (4.6).

### 4.3 Horizon build management

- New `HorizonVersionList` that reads GitHub releases for `horizon-rust/horizon`: the current `nightly`, the dated `nightly-<date>` releases still published, and any `vX.Y.Z` releases. Cache the list for offline use.
- New `HorizonInstallTask`: download the archive for this OS and architecture (five platforms), verify `SHA256SUMS` and its Ed25519 signature over `horizon release <tag>\n` + `SHA256SUMS` with the keys from `release-keys.txt`, and unpack the whole folder into `horizon/versions/<tag>/` in the data directory. On macOS clear the quarantine flag.
- **Dated Nightlies are deleted after seven days.** Keep every installed build locally and never assume the tag can be downloaded again. An instance pinned to a tag keeps working from the local copy; on a new machine it offers the current Nightly instead.
- After installing a build: ask it (`--launch-api`), check the resources with it, prepare if the check asks. Keep the previous build until the new one has started once.
- Share builds across instances. Remove builds that no instance uses, never touching worlds.
- Windows: start the game from a stable path (for example a fixed `current/` folder per instance), or the firewall prompt returns after every update.
- Honour the launcher's proxy settings.
- **Open:** ship as a meta component (`dev.playhorizon.client`) or a version picker on a dedicated page? Recommendation: dedicated page first, meta later if useful.

### 4.4 Mojang files

- The client jar, libraries and assets come from the existing `net.minecraft` 26.3 component. This needs a Microsoft account that owns the game, as Prism already requires.
- The server jar is needed for the first preparation. **Prism's meta drops `downloads.server`**, so fetch Mojang's 26.3 version JSON from piston-meta. Store the jar in `horizon/server-jars/26.3/server.jar` and verify it against Mojang's SHA-1 (`horizon-prepare` checks it again against `research-lock.json`). Download it only for a signed-in owner.
- Java for preparation: `java_major` from the preparer's `--launch-api` reply (25). Use the launcher's Java detection or its Mojang runtime download, and pass it with `--java-home`. Pick it independently of the instance's and the group's Java settings: a group with Java 17 or 21 pinned must not reach Horizon.

### 4.5 Preparation

No shim folder and no upstream change are needed:

```sh
horizon-prepare --prepare --output <data>/horizon/prepared/<tag> --complete --progress json \
    --version-json <json> --client-jar <jar> --libraries-dir <data>/libraries --assets-dir <data>/assets \
    --server-jar <data>/horizon/server-jars/26.3/server.jar --java-home <java>
```

- Prefer naming each input (versioned Launch API) over `--prism-dir`. `--prism-dir` reads a Prism root directly, but it is not in the versioned option list; use it only if the build's `--launch-api` reply lists it.
- Before every start run `horizon-prepare --check --output <prepared>`. Exit 0: start. Exit 1: prepare. Horizon keeps its own stamp, so we don't need one.
- Prepared resources depend on the build and the Minecraft version, not the instance: one folder per build.
- Show progress in the launch dialog from the JSON lines (`message`, `fraction`, `error`). The first preparation takes minutes.
- Prepared resources contain Mojang assets. Keep them private and never include them in exports.

### 4.6 Launch and authentication

- Factor a base launch step out of `LauncherPartLaunch` instead of copying it. It already carries the process handling, the ready/proceed handshake, the wrapper command, GameMode, crash-on-exit-code and close-after-launch. `HorizonLaunch` and the Java step then both derive from it.
  - Close-after-launch currently waits for a Java log line; give the base step a hook so Horizon can signal readiness its own way (for example its first `latest.log` line or the process start).
- `HorizonLaunch` starts `<build>/horizon-client` with:
  - `--resources <prepared>`
  - `--game-dir <gameRoot()> --home <gameRoot()>`
  - `--width` / `--height` (or `--fullscreen`) from the window settings
  - `--session-stdin`, then write the session JSON to stdin and close it
  - `--quickPlaySingleplayer <world>` or `--quickPlayMultiplayer <host[:port]>` for quick-join targets
  - `HorizonExtraArgs`. Prism has no separate "extra game arguments" setting (`extraArguments()` returns the JVM arguments), so this is the only source. Check each option against the `--launch-api` `options` list and refuse unknown ones before starting, because the game exits with code 2 on them.
- Session JSON: `{"name","uuid","access_token","user_type":"msa"}`. `xuid` is optional; omit it. Never put the token on the command line or in logs.
- **Owner gate:** Horizon asks third-party launchers to start it only for an account that owns Java Edition. Do not offer `--name` offline starts for offline accounts; require a Microsoft account with ownership, as Prism already does for Java.
- Exit codes: 1 and 101 show the last stderr line (it names the crash report); 2 is a launcher bug and should show the refused option.
- Environment:
  - Keep the launcher's environment variables and wrapper command (GameMode, MangoHud and the like work as wrappers). Hide JVM-only settings.
  - Set `HORIZON_NO_HEARTBEAT=1` (and decide whether to set `HORIZON_TELEMETRY_URL=off`); the playing-count heartbeat is sent even without telemetry consent.
  - Copy the official launcher's environment cleanup: set `PIPEWIRE_PROPS`, remove `LIBGL_ALWAYS_SOFTWARE` and `GALLIUM_DRIVER`.
- Data collection: decide what the fork writes to Horizon's telemetry consent. Default to sending nothing.

### 4.7 UI

| Instance page | Plan |
| --- | --- |
| Version | Replace with a **Horizon version** page: pick a tag or follow Nightly, show install and prepare state, add a "Re-prepare" button |
| Mods | Hidden until phase 4, then `HorizonModModel` |
| Resource packs | Reuse |
| Shader packs | Reuse, accepting `.hshader` files and `shaders/` zips |
| Data packs, Worlds | Reuse |
| Servers | Reuse (`servers.dat`) |
| Screenshots, Notes, Logs | Reuse |
| Settings | Keep custom commands, environment and window options; hide Java and memory |

- There is no hook to hide instance pages by type yet. Add one to the instance page provider.
- New Instance dialog: add a **Horizon** source page (version picker, name, group, icon).
- Add a Horizon badge or icon in the instance list.
- Force "Always use latest Minecraft version" off for Horizon instances, or it would move them off 26.3.
- Guard CurseForge drag-and-drop installs and the CurseForge export for Horizon instances.

### 4.8 Import and export

- Horizon's own export writes a `.mrpack` with no mod loader, plus a game-folder `.zip`. It writes **no marker file**; Horizon's import treats any loader-less `.mrpack` as a Horizon instance. We cannot tell a Horizon pack from a vanilla one, so ask the player on import (vanilla or Horizon) when a loader-less pack targets a Minecraft version Horizon supports.
- The mrpack import currently aborts on unknown dependency types; make it tolerant before relying on it for Horizon packs.
- Export a Horizon instance as `.mrpack`, compatible with Horizon's launcher import. The export exclusions are hard-coded in two dialogs; add `.horizon-cache/`, prepared resources, logs and account data there.
- Instance copy: make sure it does not copy any session or cache files.
- Optional: an "Import from Horizon launcher" action that copies instances from its data folder (`instances.json` plus the instance folders).

### 4.9 Mods (phased)

Horizon mods do not map onto Prism's jar-based `ModFolderModel`. Native mods are compiled into the game binary, so installing one produces a per-instance build of the game rather than a dropped-in file.

1. **4a: read-only.** `HorizonModModel` lists `.hmod` files in the instance's mod folder using Horizon's mod file format (`docs/api/mod-files.md`), with the enabled state from `horizon-mods.txt`.
2. **4b: content mods and the registry.** Add Horizon's registry (a signed JSON index, `docs/api/mod-index.md`) as a new provider next to Modrinth and CurseForge, with signature and checksum verification. Per-mod switches write `horizon-mods.txt`.
3. **4c: native mods.** Delegate to Horizon's build tooling to make a per-instance build. The launch step then picks the instance's custom build over the stock one. This needs explicit user consent for native code, mirroring `--trust-native-code`.
4. **4d: modpacks.** Import and export `.hpack` modpacks. Optionally run Horizon's port coverage report on Java packs and import what can be carried over.

WebAssembly mods are not supported by current Horizon; ignore them.

### 4.10 Interaction with fork features

- **Group settings:** Horizon instances must ignore inherited Java and memory settings (see 4.4). Other inherited settings (window size, wrapper, environment) apply as usual.
- **Shared game options (`SyncGameOptions`):** a shared profile would write Java keybinds into Horizon's `options.txt` and merge Horizon-only keys back into the shared profile. Make it off by default for Horizon instances, and keep separate profiles per game if it is ever enabled.
- **Bulk operations:** rename, export and delete work unchanged; bulk "change version" must skip Horizon instances.

## 5. Milestones

| # | Milestone | Done when |
| --- | --- | --- |
| M1 | Spike | The instance factory and the type rewrite fix land. A hand-made `InstanceType=Horizon` instance launches a manually installed build with manually prepared resources, via `--session-stdin`. Online-mode join works. Logs show in the console. |
| M2 | Automated fetch | A fresh instance on a clean machine goes from Play to the title screen with no manual steps: build, Mojang files, server jar, Java 25 and preparation. Reruns skip finished work (`--check`). |
| M3 | First-class UI | Horizon is in the New Instance dialog and has its version page. Java-only pages are hidden. Worlds, servers, resource packs, shader packs, data packs and screenshots work. Import and export round-trip with Horizon's launcher. |
| M4 | Mods | Phases 4a and 4b. Then 4c behind a consent prompt, and 4d. |
| M5 | Polish | Updates follow Nightly, builds are kept locally past the seven-day deletion, unused builds are cleaned up, and all five platforms are tested. |

## 6. Testing

- **Unit tests:** release list parsing, checksum and signature verification (including the tag-bound message), `--launch-api` reply parsing and option filtering, the `--progress json` parser, session JSON building, mrpack detection, and the instance factory and type preservation.
- **Integration (Linux CI):** create an instance through the CLI and launch it headless with `HORIZON_NO_AUDIO`, `HORIZON_NO_DISCORD=1` and `HORIZON_NO_HEARTBEAT=1`. Use `horizon-ctl` (`--control`) to wait for the title screen, open a world, and check that `logs/latest.log` appears in the instance folder.
- **Exit codes:** pass an unknown option and expect a clear launcher error before start (and code 2 if it slipped through).
- **Auth:** reuse Horizon's mock-services approach (`HORIZON_API_DISCOVERY`) to test online-mode join without real accounts.
- **Manual:** online server join with a real account, switching Horizon versions on an existing world, a group with Java 21 pinned, and a shared game options profile.

## 7. Risks and open questions

- **Upstream churn.** Horizon changes fast. Depend only on the versioned Launch API (0.1.x), read `--launch-api` before every use, and test against Nightly in CI.
- **Build retention.** Dated Nightlies vanish after seven days, and a signed release index or longer-lived builds don't exist yet. Local copies are the only guarantee.
- **Java assumptions in Prism.** The `getPackProfile()` and settings audit may turn up more Java-specific code than expected.
- **Native mod builds** need a toolchain and take time. Check what Horizon's launcher bundles for this.
- **Platform support.** Five archives are built; confirm the support level per platform.
- **Owner gate.** Keep the ownership requirement for every Horizon start, as Horizon's launch guide asks.
- **License.** Anything ported from Horizon (signature checks, launcher logic) is LGPL-3.0-or-later; keep its notices.
- **Microsoft client ID.** The fork still uses Prism's MSA client ID; Horizon starts reuse the launcher's existing token, so this is not new, but a fork-owned client ID would remove the dependency.

## 8. References

- Launch API: `docs/api/launch.md`, `docs/api/launch-guide.md`, `docs/api/launch-reference.md`
- Release signing and data formats: `docs/api/data-formats.md`, `docs/release-flow.md`
- Mods, registry and shaders: `docs/api/mod-files.md`, `docs/api/mod-index.md`, `docs/api/shaders.md`, `docs/modding/`
- Telemetry and heartbeat: `docs/telemetry.md`
- Native client and `horizon-ctl`: `docs/native-client.md`
- Horizon launcher (reference implementation): `launcher/README.md`
- Prism: `launcher/InstanceList.*`, `launcher/BaseInstance.*`, `launcher/minecraft/MinecraftInstance.*`, `launcher/minecraft/launch/` (steps, `LauncherPartLaunch`), `launcher/LaunchController.*`
