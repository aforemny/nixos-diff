{
  boot.loader.grub.device = "nodev";
  fileSystems."/".device = "tmpfs";
  nixpkgs.hostPlatform = "x86_64-linux";
}
