{ pkgs, ... }:
{
  imports = [
    <nixos-facter-modules/modules/nixos/facter.nix>
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
