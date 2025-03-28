{ pkgs ? import (import ./nix/sources.nix).nixpkgs {
    overlays = [
      (self: super: {
        nix = super.nix.overrideAttrs (oldAttrs: {
          patches = oldAttrs.patches or [ ] ++ [
            (pkgs.writeText "nix-lazy-print-drvs.patch" ''
              --- a/src/libexpr/print.cc	1970-01-01 01:00:01.000000000 +0100
              +++ b/src/libexpr/print.cc	2025-04-25 14:57:29.546190049 +0200
              @@ -334,7 +334,7 @@ private:
                           return;
                       }
 
              -        if (options.force && options.derivationPaths && state.isDerivation(v)) {
              +        if (state.isDerivation(v)) {
                           printDerivation(v);
                       } else if (depth < options.maxDepth) {
                           increaseIndent();
            '')
          ];
        });
      })
    ];
  }
}:
pkgs.callPackage ./src/nixos-diff.nix { }
