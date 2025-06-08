# /!\ WORK-IN-PROGRESS. DO NOT USE! /!\

# nixos-diff

Diff NixOS configurations.

## Usage

```console
nixos-diff ./config1.nix ./config1.nix
nixos-diff ./config1.nix
nixos-diff --expr 'import <nixpkgs/nixos> { configuration = ./config1.nix; }'
nixos-diff .#nixosConfigurations.machine1 .#nixosConfigurations.machine2
nixos-diff .#nixosConfigurations.machine1
```

## Installation

```console
nix-env -f. -i
```

## Development

```console
$ meson setup build
$ meson compile
```

## Tests

```console
$ nix-build --no-out-link tests.nix
$ update-tests
```
