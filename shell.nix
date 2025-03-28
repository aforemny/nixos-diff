{ pkgs ? import (import ./nix/sources.nix).nixpkgs { }
}:
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.gdb
    pkgs.installShellFiles
    pkgs.meson
    pkgs.ninja
    pkgs.niv
    pkgs.pkg-config
  ];
  buildInputs = [
    pkgs.boost
    pkgs.nix
  ];
}
