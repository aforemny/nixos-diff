{ pkgs, ... }:
let
  nixos-facter = builtins.fetchTarball "https://github.com/nix-community/nixos-facter-modules/archive/58ad9691670d293a15221d4a78818e0088d2e086.zip";
in
{
  imports = [
    "${nixos-facter}/modules/nixos/facter.nix"
    { config.facter.reportPath = ./facter.json; }
  ];
  config = {
    nixpkgs.hostPlatform = "x86_64-linux";
    fileSystems."/".device = "tmpfs";
    boot.loader.grub.device = "nodev";
    system.stateVersion = "25.05";
    services.xserver.videoDrivers = [ "nvidia" ];
    hardware.nvidia.open = true;
    nixpkgs.config.allowUnfreePredicate = pkg: pkgs.lib.elem (pkgs.lib.getName pkg) [ "nvidia-x11" "nvidia-settings" ];
  };
}
