{
  description = "A template that shows all standard flake outputs";
  inputs.nixpkgs.url = "nixpkgs";
  inputs.flake-compat.url = "https://flakehub.com/f/edolstra/flake-compat/1.tar.gz";
  outputs = all@{ nixpkgs, ... }: {
    nixosConfigurations.example = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [{
        boot.loader.grub.device = "nodev";
        fileSystems."/".device = "tmpfs";
      }];
    };
  };
}
