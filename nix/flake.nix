{
  description = "Hegel for C++";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-compat.url = "https://flakehub.com/f/edolstra/flake-compat/1.tar.gz";
  };

  outputs =
    {
      self,
      nixpkgs,
      ...
    }:
    let
      # darwin/amd64 is intentionally absent: no prebuilt libhegel is published
      # for it (build from source and pass HEGEL_LIBHEGEL_LIBRARY).
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Prebuilt libhegel (Hegel's native engine) release. Keep the version and
      # hashes in sync with cmake/libhegel.cmake and libhegel/hegel.h. Hashes
      # are the SHA-256 sidecars published next to each release asset.
      libhegelVersion = "0.43.0";
      libhegelAssets = {
        "x86_64-linux" = {
          asset = "libhegel-linux-amd64.so";
          sha256 = "80d3eed1589635c180baf71b41c27c93215e3835f2152822b5fc556855b26557";
        };
        "aarch64-linux" = {
          asset = "libhegel-linux-arm64.so";
          sha256 = "2f0200500622b54d2cf717597259dc02dfcb92d4fb3ace7a0caa8a5b00a82751";
        };
        "aarch64-darwin" = {
          asset = "libhegel-darwin-arm64.dylib";
          sha256 = "b8d55c1f4c50f254d4e8b0ed488ef3bf1ff2e80fde9627d9faad14bd1a471c7c";
        };
      };

      # Fetch the prebuilt shared library for this platform as a fixed-output
      # derivation (the only network access; everything downstream is offline)
      # and normalize it to the Rust output stem libhegel_c.<ext>.
      mkLibhegel =
        pkgs:
        let
          lib = pkgs.lib;
          system = pkgs.system;
          info = libhegelAssets.${system} or (throw "libhegel: no prebuilt release for ${system}");
          ext = lib.last (lib.splitString "." info.asset);
        in
        pkgs.stdenvNoCC.mkDerivation {
          pname = "libhegel";
          version = libhegelVersion;

          src = pkgs.fetchurl {
            url = "https://github.com/hegeldev/hegel-rust/releases/download/libhegel-v${libhegelVersion}/${info.asset}";
            inherit (info) sha256;
          };
          dontUnpack = true;

          # Linux: patch the .so's NEEDED system libs to the nix store. macOS:
          # rewrite the dylib id (the released one is an absolute CI path).
          nativeBuildInputs =
            lib.optionals pkgs.stdenv.isLinux [ pkgs.autoPatchelfHook ]
            ++ lib.optionals pkgs.stdenv.isDarwin [ pkgs.fixDarwinDylibNames ];
          buildInputs = lib.optionals pkgs.stdenv.isLinux [ (lib.getLib pkgs.stdenv.cc.cc) ];

          installPhase = ''
            runHook preInstall
            mkdir -p $out/lib
            cp "$src" "$out/lib/libhegel_c.${ext}"
            runHook postInstall
          '';

          passthru = { inherit ext; };
        };

      fetchDeps = pkgs: {
        reflectcpp = pkgs.fetchFromGitHub {
          owner = "getml";
          repo = "reflect-cpp";
          tag = "v0.22.0";
          hash = "sha256-5Og3+dM3QuCX6sT+6Rz8vwvyzQb+8qz10ROk9yOMPgE=";
        };
        approvaltests = pkgs.fetchFromGitHub {
          owner = "approvals";
          repo = "ApprovalTests.cpp";
          tag = "v.10.13.0";
          hash = "sha256-Z9VI+OmvGyzBZ5hU0O+xn2hgNMDYTUQCl+k/i965n5Q=";
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
          FETCHCONTENT_SOURCE_DIR_GOOGLETEST = pkgs.gtest.src;
          FETCHCONTENT_SOURCE_DIR_APPROVALTESTS = deps.approvaltests;
          FETCHCONTENT_FULLY_DISCONNECTED = "ON";
        };

      # export builder helper for consumers
      mkHegelCppProject =
        {
          pkgs,
          stdenv ? pkgs.stdenv,
        }@args:
        let
          lib = pkgs.lib;

          libhegel = mkLibhegel pkgs;

          fs = pkgs.lib.fileset;
          baseSrc = fs.unions [
            ./../cmake
            ./../CMakeLists.txt
            ./../src
            ./../include
            ./../libhegel
            ./../tests
            ./../docs
          ];
        in
        stdenv.mkDerivation {
          pname = "hegel-cpp";
          version = "0.1.0";

          src = fs.toSource {
            root = ./..;
            fileset = baseSrc;
          };

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
          ];

          buildInputs = [
            libhegel
          ];

          cmakeFlags = (mkFetchContentFlags pkgs) ++ [
            (lib.cmakeFeature "HEGEL_BUILD_EXAMPLES" "OFF")
            # Use the prebuilt engine fetched above instead of downloading one
            # (the build sandbox has no network).
            (lib.cmakeFeature "HEGEL_LIBHEGEL_LIBRARY" "${libhegel}/lib/libhegel_c.${libhegel.ext}")
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure --verbose 
            runHook postCheck
          '';
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
        in
        {
          default = mkHegelCppProject { inherit pkgs; };
          libhegel = mkLibhegel pkgs;
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
