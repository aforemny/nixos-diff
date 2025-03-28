{ pkgs, ... }:
{
  nixpkgs.hostPlatform = "x86_64-linux";
  fileSystems."/".device = "tmpfs";
  boot.loader.grub.device = "nodev";
  system.stateVersion = "25.05";
  boot.kernelPackages = pkgs.linuxPackages_latest;
  nix.package = pkgs.nixVersions.latest;
}
