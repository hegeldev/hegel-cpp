{
  description = "Nix integration test for hegel-cpp";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hegel-cpp.url = "path:../..";
  };

  outputs =
    {
      self,
      nixpkgs,
      hegel-cpp,
      ...
    }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      stdenv = pkgs.stdenv;
      fs = pkgs.lib.fileset;

      test = stdenv.mkDerivation {
          pname = "hegel-cpp-nix-test";
          version = "0.1.0";
          src = fs.toSource {
            root = ./.;
            fileset = fs.unions [ ./main.cpp ./CMakeLists.txt ];
          };

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
          ];

          buildInputs = [
            hegel-cpp.packages.${system}.default
          ];
        };

    in
    {
      packages.${system}.default = test;

      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ self.packages.${system}.default ];
      };
    };
}
