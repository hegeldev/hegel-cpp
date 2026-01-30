{
  description = "Nix integration test for hegel-cpp";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hegel-cpp.url = "path:../..";
  };

  outputs = { nixpkgs, hegel-cpp, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      lib = pkgs.lib;
      hegel = hegel-cpp.inputs.hegel;

      # Assemble source: tests/nix files + hegel-cpp repo at "hegel-cpp" subdir
      src = pkgs.runCommand "nix-test-src" { } ''
        mkdir $out
        cp -r ${./.}/* $out/
        cp -r ${./../..} $out/hegel-cpp
      '';
    in {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "hegel-cpp-nix-test";
        version = "0.1.0";
        src = src;

        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
          hegel.packages.${system}.default
        ];

        cmakeFlags = lib.mapAttrsToList (k: v: lib.cmakeFeature k (toString v)) {
          # Pre-fetched sources for FetchContent (inherited from hegel-cpp)
          FETCHCONTENT_SOURCE_DIR_REFLECTCPP = pkgs.fetchFromGitHub {
            owner = "getml";
            repo = "reflect-cpp";
            tag = "v0.22.0";
            hash = "sha256-5Og3+dM3QuCX6sT+6Rz8vwvyzQb+8qz10ROk9yOMPgE=";
          };

          FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON = pkgs.fetchFromGitHub {
            owner = "nlohmann";
            repo = "json";
            tag = "v3.12.0";
            hash = "sha256-cECvDOLxgX7Q9R3IE86Hj9JJUxraDQvhoyPDF03B2CY=";
          };

          FETCHCONTENT_SOURCE_DIR_GOOGLETEST = pkgs.fetchFromGitHub {
            owner = "google";
            repo = "googletest";
            tag = "v1.14.0";
            hash = "sha256-t0RchAHTJbuI5YW4uyBPykTvcjy90JW9AOPNjIhwh6U=";
          };
        };

        doCheck = true;
      };

      devShells.${system}.default = pkgs.mkShell {
        buildInputs = [
          pkgs.cmake
          pkgs.ninja
          hegel.packages.${system}.default
        ];
      };
    };
}
