{ pkgs ? import (import ../nix/sources.nix).nixpkgs { } }:
pkgs.mkShell {
  buildInputs = [
    pkgs.mdp
  ];
}
