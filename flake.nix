{
  description = "Hegel C++ SDK";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hegel.url = "git+ssh://git@github.com/antithesishq/hegel";
  };

  outputs =
    {
      self,
      nixpkgs,
      hegel,
      ...
    }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      fetchDeps = pkgs: {
        reflectcpp = pkgs.fetchFromGitHub {
          owner = "getml";
          repo = "reflect-cpp";
          tag = "v0.22.0";
          hash = "sha256-5Og3+dM3QuCX6sT+6Rz8vwvyzQb+8qz10ROk9yOMPgE=";
        };
        nlohmann_json = pkgs.fetchFromGitHub {
          owner = "nlohmann";
          repo = "json";
          tag = "v3.12.0";
          hash = "sha256-cECvDOLxgX7Q9R3IE86Hj9JJUxraDQvhoyPDF03B2CY=";
        };
        googletest = pkgs.fetchFromGitHub {
          owner = "google";
          repo = "googletest";
          tag = "v1.14.0";
          hash = "sha256-t0RchAHTJbuI5YW4uyBPykTvcjy90JW9AOPNjIhwh6U=";
        };
      };

      # Generate CMake flags for FetchContent sources
      mkFetchContentFlags =
        pkgs:
        let
          lib = pkgs.lib;
          deps = fetchDeps pkgs;
        in
        lib.mapAttrsToList (k: v: lib.cmakeFeature k (toString v)) {
          FETCHCONTENT_SOURCE_DIR_REFLECTCPP = deps.reflectcpp;
          FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON = deps.nlohmann_json;
          FETCHCONTENT_SOURCE_DIR_GOOGLETEST = deps.googletest;
        };

      # export builder helper for consumers
      mkHegelCppProject =
        {
          pkgs,
          src,
          pname,
          version,
          cmakeFlags ? [ ],
          nativeBuildInputs ? [ ],
          buildInputs ? [ ],
          doCheck ? true,
          ...
        }@args:
        let
          lib = pkgs.lib;
          # Remove our custom args, pass the rest to mkDerivation
          extraArgs = builtins.removeAttrs args [
            "pkgs"
            "cmakeFlags"
            "nativeBuildInputs"
            "buildInputs"
            "doCheck"
          ];
        in
        pkgs.stdenv.mkDerivation (
          extraArgs
          // {
            inherit
              src
              pname
              version
              doCheck
              ;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              hegel.packages.${pkgs.system}.default
            ]
            ++ nativeBuildInputs;

            inherit buildInputs;

            cmakeFlags = (mkFetchContentFlags pkgs) ++ [
              (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_HEGEL" (toString (lib.cleanSource self)))
            ] ++ cmakeFlags;
          }
        );

    in
    {
      # Export the builder for users
      lib = {
        inherit mkHegelCppProject;
      };

      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;
          deps = fetchDeps pkgs;
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "hegel-cpp";
            version = "1.0.0";
            src = lib.cleanSource ./.;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              hegel.packages.${system}.default
            ];

            cmakeFlags = (mkFetchContentFlags pkgs) ++ [
              (lib.cmakeFeature "HEGEL_BUILD_DOCS" "OFF")
              (lib.cmakeFeature "HEGEL_BUILD_EXAMPLES" "OFF")
            ];

            doCheck = true;
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
            packages = [
              pkgs.clang-tools
            ];
          };
        }
      );
    };
}
