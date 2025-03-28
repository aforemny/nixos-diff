{ modulesPath, ... }:
{
  imports = [ "${modulesPath}/installer/cd-dvd/installation-cd-minimal.nix" ];
  config = { networking.hostName = "machine"; };
}
