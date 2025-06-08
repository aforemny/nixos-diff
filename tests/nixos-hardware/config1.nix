{ pkgs, ... }:
{
  imports = [
    <nixos-hardware/lenovo/thinkpad/x1-extreme/gen3>
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
