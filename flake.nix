{
  description = "Hegel C++ SDK";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-compat.url = "https://flakehub.com/f/edolstra/flake-compat/1.tar.gz";
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
          FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON = pkgs.nlohmann_json.src;
          FETCHCONTENT_SOURCE_DIR_GOOGLETEST = pkgs.gtest.src;
          FETCHCONTENT_FULLY_DISCONNECTED = "ON";
        };

      # export builder helper for consumers
      mkHegelCppProject =
        {
          pkgs,
          stdenv ? pkgs.stdenv,
          lib ? pkgs.lib,
          system ? pkgs.system,
        }@args:
        let
          fs = pkgs.lib.fileset;
          baseSrc = fs.unions [
            ./cmake
            ./CMakeLists.txt
            ./src
            ./include
            ./tests
            ./docs
          ];
        in
        stdenv.mkDerivation {
          pname = "hegel-cpp";
          version = "0.1.0";
          src = fs.toSource {
            root = ./.;
            fileset = baseSrc;
          };
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.doxygen
          ];

          buildInputs = [
            hegel.packages.${system}.default
          ];

          cmakeFlags = (mkFetchContentFlags pkgs) ++ [
            (lib.cmakeFeature "HEGEL_BUILD_DOCS" "ON")
            (lib.cmakeFeature "HEGEL_BUILD_EXAMPLES" "OFF")
          ];

          doCheck = true;
        };
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
          default = mkHegelCppProject { inherit pkgs; };
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
