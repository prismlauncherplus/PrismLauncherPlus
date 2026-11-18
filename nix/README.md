# Prism Launcher Plus Nix Packaging

Prism Launcher Plus is not in nixpkgs and has no binary cache, so it is always built from source on your machine. This takes a while
the first time and after each update.

The flake exposes:

| Output | Contents |
| --- | --- |
| `packages.<system>.prismlauncherplus` (also `default`) | The wrapped launcher, with everything necessary to run it and Minecraft |
| `packages.<system>.prismlauncherplus-unwrapped` | A minimal build for advanced customization of the launcher's runtime environment |
| `overlays.default` | Adds `prismlauncherplus` and `prismlauncherplus-unwrapped` to your nixpkgs |

The installed program is `prismlauncherplus`, with the desktop entry `io.github.prismlauncherplus.PrismLauncherPlus`. It can be installed
next to upstream Prism Launcher (`pkgs.prismlauncher`): both have their own binary, desktop entry and data directory
(`~/.local/share/PrismLauncherPlus` vs `~/.local/share/PrismLauncher`). On first start Prism Launcher Plus offers to copy an existing
Prism Launcher data directory.

## Installing with flakes

### Installing the package directly

Add `github:prismlauncherplus/PrismLauncherPlus` to your flake inputs and use the flake's `packages` output.

Example (NixOS):

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    prismlauncherplus = {
      url = "github:prismlauncherplus/PrismLauncherPlus";

      # Optional: build against the same nixpkgs revision as the rest of your system, instead of the one locked in this flake.
      # This avoids downloading a second copy of nixpkgs and its dependencies. The build needs a recent nixpkgs (unstable).
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    { nixpkgs, prismlauncherplus, ... }:
    {
      nixosConfigurations.foo = nixpkgs.lib.nixosSystem {
        modules = [
          ./configuration.nix

          (
            { pkgs, ... }:
            {
              environment.systemPackages = [
                prismlauncherplus.packages.${pkgs.stdenv.hostPlatform.system}.prismlauncherplus
              ];
            }
          )
        ];
      };
    };
}
```

With [Home Manager](https://github.com/nix-community/home-manager) use `home.packages` instead of `environment.systemPackages`.

### Using the overlay

Alternatively, add the overlay to your nixpkgs instance. This always builds Prism Launcher Plus with your system's packages.

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    prismlauncherplus.url = "github:prismlauncherplus/PrismLauncherPlus";
  };

  outputs =
    { nixpkgs, prismlauncherplus, ... }:
    {
      nixosConfigurations.foo = nixpkgs.lib.nixosSystem {
        modules = [
          ./configuration.nix

          (
            { pkgs, ... }:
            {
              nixpkgs.overlays = [ prismlauncherplus.overlays.default ];

              environment.systemPackages = [ pkgs.prismlauncherplus ];
            }
          )
        ];
      };
    };
}
```

### Updating

The flake input is pinned in your `flake.lock`. To pick up new commits pushed to this repository:

```shell
nix flake update prismlauncherplus
```

### Installing the package ad-hoc (`nix shell`, `nix run`, etc.)

```shell
nix run github:prismlauncherplus/PrismLauncherPlus

nix shell github:prismlauncherplus/PrismLauncherPlus

nix profile install github:prismlauncherplus/PrismLauncherPlus
```

## Installing without flakes

flake-compat allows using this flake on a system that doesn't use flakes.

### Installing the package directly (`fetchTarball`)

```nix
{ pkgs, ... }:
{
  environment.systemPackages = [
    (import (
      builtins.fetchTarball "https://github.com/prismlauncherplus/PrismLauncherPlus/archive/develop.tar.gz"
    )).packages.${pkgs.stdenv.hostPlatform.system}.prismlauncherplus
  ];
}
```

### Using the overlay (`fetchTarball`)

```nix
{ pkgs, ... }:
{
  nixpkgs.overlays = [
    (import (
      builtins.fetchTarball "https://github.com/prismlauncherplus/PrismLauncherPlus/archive/develop.tar.gz"
    )).overlays.default
  ];

  environment.systemPackages = [ pkgs.prismlauncherplus ];
}
```

## Customizing wrapped packages

The wrapped package (`prismlauncherplus`) offers some build parameters to further customize the launcher's environment, for example
`prismlauncherplus.packages.${system}.prismlauncherplus.override { jdks = [ pkgs.jdk21 ]; }`.

The following parameters can be overridden:

- `additionalLibs` (default: `[ ]`) Additional libraries that will be added to `LD_LIBRARY_PATH`
- `additionalPrograms` (default: `[ ]`) Additional libraries that will be added to `PATH`
- `controllerSupport` (default: `isLinux`) Turn on/off support for controllers on Linux (macOS will always have this)
- `gamemodeSupport` (default: `isLinux`) Turn on/off support for [Feral GameMode](https://github.com/FeralInteractive/gamemode) on Linux
- `jdks` (default: `[ jdk25 jdk21 jdk17 jdk8 ]`) Java runtimes added to the `PRISMLAUNCHERPLUS_JAVA_PATHS` variable
- `msaClientID` (default: `null`, requires full rebuild!) Client ID used for Microsoft Authentication
- `textToSpeechSupport` (default: `isLinux`) Turn on/off support for text-to-speech on Linux (macOS will always have this)
