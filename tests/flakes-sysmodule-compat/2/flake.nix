{
  description = "Dummy NixOS (+ nix-darwin + home-manager) config";
  inputs = {
    flake-compat.url = "https://flakehub.com/f/edolstra/flake-compat/1.tar.gz";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    flake-parts.url = "github:hercules-ci/flake-parts";
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    sysmodule-flake.inputs.flake-parts.follows = "flake-parts";
    sysmodule-flake.inputs.nixpkgs.follows = "nixpkgs";
    sysmodule-flake.url = "github:applicative-systems/sysmodule-flake";
  };
  outputs = inputs: inputs.flake-parts.lib.mkFlake { inherit inputs; } {
    systems = [ "x86_64-linux" ];
    imports = [
      inputs.sysmodule-flake.flakeModules.default
    ];
    sysmodules-flake = {
      modulesPath = ./.;
      specialArgs.self = inputs.self;
    };
  };
}
