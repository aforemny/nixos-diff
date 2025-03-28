{ pkgs, ... }:
let
  nixos-hardware = builtins.fetchTarball "https://github.com/NixOS/nixos-hardware/archive/f7bee55a5e551bd8e7b5b82c9bc559bc50d868d1.zip";
in
{
  imports = [
    "${nixos-hardware}/lenovo/thinkpad/x1-extreme/gen3"
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
