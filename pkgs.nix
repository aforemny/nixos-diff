self: super: {
  dtl = self.callPackage
    ({ fetchFromGitHub
     , scons
     , stdenv
     }: stdenv.mkDerivation (finalAttrs: {
      name = "dtl";
      version = "1.21";
      src = fetchFromGitHub {
        owner = "cubicdaiya";
        repo = finalAttrs.name;
        tag = "v${finalAttrs.version}";
        hash = "sha256-s+syRiJhcxvmE0FBcbCi6DrL1hwu+0IJNMgg5Tldsv4=";
      };
      patchPhase = ''
        sed -i 's/'"'"dtl"'"', '"'"include"'"'/'"'"include"'"', '"'"dtl"'"'/' SConstruct
      '';
      nativeBuildInputs = [ scons ];
      postInstall = ''
        mkdir -p $out/lib/pkgconfig
        cat >$out/lib/pkgconfig/dtl.pc <<'EOF'
        prefix=$out
        libdir=$out/lib
        includedir=$out/include

        Name: ${finalAttrs.name}
        Description: ${finalAttrs.meta.description}
        Version: ${finalAttrs.version}
        Libs: -L''${libdir}
        Cflags: -I''${includedir}/dtl
        EOF
      '';
      meta.description = "diff template library written by C++";
    }))
    { };
  nix = super.nix.overrideAttrs (oldAttrs: {
    patches = oldAttrs.patches or [ ] ++ [
      (self.writeText "nix-lazy-print-drvs.patch" ''
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
}
