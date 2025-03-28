{ pkgs ? import (import ./nix/sources.nix).nixpkgs { } }:
let
  inherit (pkgs) lib;
  nixos-diff = import ./. { inherit pkgs; };
in
{
  nixos-modules = pkgs.writeScript "nixos-modules" ''
    cd ${./tests/nixos-modules}
    ${nixos-diff}/bin/nixos-diff ./config1.nix ./config2.nix |& less -S
  '';
  nixos-hardware = pkgs.writeScript "nixos-hardware" ''
    cd ${./tests/nixos-hardware}
    ${nixos-diff}/bin/nixos-diff ./config1.nix ./config2.nix |& less -S
  '';
  derivation = pkgs.writeScript "derivation" ''
    cd ${./tests/derivation}
    ${nixos-diff}/bin/nixos-diff ./config1.nix ./config2.nix |& less -S
  '';
}
