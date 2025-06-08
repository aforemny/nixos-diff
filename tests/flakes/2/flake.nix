{
  description = "A template that shows all standard flake outputs";
  inputs.nixpkgs.url = "nixpkgs";
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
