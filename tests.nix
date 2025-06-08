{ pkgs ? import sources.nixpkgs { }
, sources ? import ./nix/sources.nix
, dontCheck ? false
}:
let
  inherit (pkgs) lib;
  nixos-diff = import ./. { inherit pkgs; };
  runTest = if dontCheck then buildOutput else checkOutput;
  checkOutput = name: pkgs.runCommand name { } ''
    cd ${lib.fileset.toSource {
      root = ./tests/${name};
      fileset = ./tests/${name}/output.diff;
    }}
    ${lib.optionalString (!dontCheck) "diff -u output.diff ${buildOutput name}"} >$out
  '';
  buildOutput = name: pkgs.runCommand name
    {
      NIX_CONFIG = "store = dummy://";
      NIX_PATH = "nixos-facter-modules=${sources.nixos-facter-modules}:nixos-hardware=${sources.nixos-hardware}:nixpkgs=${sources.nixpkgs}";
    } ''
    cd ${lib.fileset.toSource {
      root = ./tests/${name};
      fileset = lib.fileset.difference ./tests/${name} ./tests/${name}/output.diff;
    }}
    ${lib.getExe nixos-diff} ./config{1,2}.nix >$out
  '';
in
lib.mapAttrs (name: _: runTest name) (builtins.readDir (toString ./tests))
