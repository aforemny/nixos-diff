{
  nixpkgs.hostPlatform = "x86_64-linux";
  fileSystems."/".device = "tmpfs";
  boot.loader.grub.device = "nodev";
  system.stateVersion = "25.05";
}
