{
  description = "Nix integration test for hegel-cpp";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    hegel-cpp.url = "path:../..";
  };

  outputs =
    { self, nixpkgs, hegel-cpp, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      # Assemble source: tests/nix files + hegel-cpp repo at "hegel-cpp" subdir
      src = pkgs.runCommand "nix-test-src" { } ''
        mkdir $out
        cp -r ${./.}/* $out/
        cp -r ${./../..} $out/hegel-cpp
      '';
    in
    {
      packages.${system}.default = hegel-cpp.lib.mkHegelCppProject {
        inherit pkgs src;
        pname = "hegel-cpp-nix-test";
        version = "0.1.0";
      };

      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ self.packages.${system}.default ];
      };
    };
}
