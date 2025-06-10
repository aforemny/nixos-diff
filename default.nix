{ pkgs ? import (import ./nix/sources.nix).nixpkgs {
    overlays = [ (import ./pkgs.nix) ];
  }
}:
pkgs.callPackage ./src/nixos-diff.nix { }
