{
  description = "Hegel C++ SDK";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hegel.url = "git+ssh://git@github.com/antithesishq/hegel";
  };

  outputs = { self, nixpkgs, hegel, ... }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "hegel-cpp";
            version = "1.0.0";
            src = lib.cleanSource ./.;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              hegel.packages.${system}.default  # hegel CLI on PATH
            ];

            cmakeFlags = lib.mapAttrsToList (k: v: lib.cmakeFeature k (toString v)) {
              # Pre-fetched sources for FetchContent
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

              # Build options
              HEGEL_BUILD_DOCS = "OFF";
              HEGEL_BUILD_EXAMPLES = "OFF";
            };

            doCheck = true;
          };
        }
      );

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
            packages = [
              pkgs.clang-tools  # clang-format, clangd
            ];
          };
        }
      );
    };
}
