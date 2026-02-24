# Nix integration test for hegel-cpp
# Makes sure we can import and use the output of the hegel-cpp flake
{
}:
let
  pkgs = import <nixpkgs> { };
  system = "x86_64-linux";
  stdenv = pkgs.stdenv;
  fs = pkgs.lib.fileset;

  hegel-cpp = import ../..;
in
stdenv.mkDerivation {
  pname = "hegel-cpp-nix-test";
  version = "0.1.0";
  src = fs.toSource {
    root = ./.;
    fileset = fs.unions [
      ./main.cpp
      ./CMakeLists.txt
    ];
  };

  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
  ];

  buildInputs = [
    hegel-cpp.packages.${system}.default
  ];
}
