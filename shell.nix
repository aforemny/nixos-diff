{ pkgs ? import sources.nixpkgs {
    overlays = [ (import ./pkgs.nix) ];
  }
, sources ? import ./nix/sources.nix
}:
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.gdb
    pkgs.installShellFiles
    pkgs.meson
    pkgs.ninja
    pkgs.niv
    pkgs.pkg-config
    (pkgs.writeShellApplication {
      name = "update-tests";
      runtimeInputs = [
        pkgs.coreutils
        pkgs.findutils
        pkgs.git
        pkgs.nix
      ];
      inheritPath = false;
      text = ''
        cd "$(git rev-parse --show-toplevel)"
        find ./tests/* -maxdepth 0 | while read -r filePath; do
          cat "$(nix-build --no-out-link --arg dontCheck true ./tests.nix -A "$(basename "$filePath")")" >"$filePath"/output.diff
        done
      '';
    })
  ];
  buildInputs = [
    pkgs.boost
    pkgs.nix
  ];
  shellHook = ''
    NIX_PATH=nixos-facter-modules=${sources.nixos-facter-modules}:nixos-hardware=${sources.nixos-hardware}:nixpkgs=${sources.nixpkgs}; export NIX_PATH
  '';
}
