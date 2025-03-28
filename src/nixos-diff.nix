{ boost
, lib
, meson
, ninja
, nix
, pkg-config
, stdenv
}:
stdenv.mkDerivation {
  name = "nixos-diff";
  src = lib.cleanSourceWith rec {
    filter = name': type:
      let name = lib.removePrefix (toString src) name'; in
      lib.elem name [
        "/meson.build"
        "/src"
        "/src/main.cc"
        "/src/meson.build"
      ];
    src = ./..;
  };
  strictDeps = true;
  nativeBuildInputs = [
    meson
    ninja
    pkg-config
  ];
  buildInputs = [
    boost
    nix
  ];
  meta = {
    license = lib.licenses.lgpl2Plus;
    mainProgram = "nixos-diff";
    maintainers = [ ];
    inherit (nix.meta) platforms;
  };
}
