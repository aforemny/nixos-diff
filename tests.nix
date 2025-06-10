{ pkgs ? import sources.nixpkgs {
    overlays = [
      (import ./pkgs.nix)
    ];
  }
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
      fileset = lib.fileset.maybeMissing (./tests/${name}/output.diff);
    }}
    ${lib.optionalString (!dontCheck) "diff -u output.diff ${buildOutput name}"} >$out
  '';
  buildOutput = name:
    if !(lib.hasPrefix "flakes" name) then
      pkgs.runCommand name
        {
          NIX_CONFIG = "store = unix:///build/.nix-socket";
          requiredSystemFeatures = [ "recursive-nix" ];
          NIX_PATH = "nixos-facter-modules=${sources.nixos-facter-modules}:nixos-hardware=${sources.nixos-hardware}:nixpkgs=${sources.nixpkgs}";
        } ''
        cd ${lib.fileset.toSource {
          root = ./tests/${name};
          fileset = lib.fileset.difference ./tests/${name} (lib.fileset.maybeMissing ./tests/${name}/output.diff);
        }}
        ${lib.getExe nixos-diff} ./config{1,2}.nix >$out
      '' else
      pkgs.runCommand name
        {
          NIX_CONFIG = "store = unix:///build/.nix-socket\nexperimental-features = flakes";
          requiredSystemFeatures = [ "recursive-nix" ];
          buildInputs = [
            (builtins.fetchTree {
              narHash = "sha256-V5fbB7XUPz8qtuJntul/7PABtP35tlmcYpriRvJ3EBw=";
              owner = "NixOS";
              repo = "nixpkgs";
              rev = "ba487dbc9d04e0634c64e3b1f0d25839a0a68246";
              type = "github";
            }).outPath
            (builtins.fetchTree {
              narHash = "sha256-NeCCThCEP3eCl2l/+27kNNK7QrwZB1IJCrXfrbv5oqU=";
              rev = "ff81ac966bb2cae68946d5ed5fc4994f96d0ffec";
              type = "tarball";
              url = "https://api.flakehub.com/f/pinned/edolstra/flake-compat/1.1.0/01948eb7-9cba-704f-bbf3-3fa956735b52/source.tar.gz";
            }).outPath
            (builtins.fetchTree {
              "narHash" = "sha256-F82+gS044J1APL0n4hH50GYdPRv/5JWm34oCJYmVKdE=";
              "owner" = "hercules-ci";
              "repo" = "flake-parts";
              "rev" = "49f0870db23e8c1ca0b5259734a02cd9e1e371a1";
              "type" = "github";
            }).outPath
            (builtins.fetchTree {
              "narHash" = "sha256-DUMoY3cf04EgHPF2TVclp8OaGCArlwGmuIQ9M544D4Y=";
              "owner" = "applicative-systems";
              "repo" = "sysmodule-flake";
              "rev" = "f54feb162fe296f53e2ad8e9a3e9cd9e03267667";
              "type" = "github";
            }).outPath
          ];
        } ''
        HOME=$PWD; export HOME
        cd ${lib.fileset.toSource {
          root = ./tests/${name};
          fileset = lib.fileset.difference ./tests/${name} (lib.fileset.maybeMissing ./tests/${name}/output.diff);
        }}
        ${lib.getExe nixos-diff} ./1#nixosConfigurations.example ./2#nixosConfigurations.example >$out
      ''
  ;
in
lib.mapAttrs (name: _: runTest name) (builtins.readDir (toString ./tests))
