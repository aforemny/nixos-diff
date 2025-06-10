{ ... }:
{
  boot.loader.grub.device = "nodev";
  fileSystems."/".device = "tmpfs";
  nixpkgs.hostPlatform = "x86_64-linux";
  system.stateVersion = "25.05";

  services.nginx = {
    enable = true;
  };
}
