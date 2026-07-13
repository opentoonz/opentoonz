{
  description = "OpenToonz C++/Qt development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      supportedSystems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "aarch64-linux"
        "x86_64-linux"
      ];
      forAllSystems = lib.genAttrs supportedSystems;
      pkgsFor = system: import nixpkgs { inherit system; };
      opentoonzFor =
        system:
        import ./nix/opentoonz-env.nix {
          pkgs = pkgsFor system;
          inherit lib;
          src = self;
        };
    in
    {
      devShells = forAllSystems (system: {
        default = (opentoonzFor system).devShell;
      });

      packages = forAllSystems (system: rec {
        opentoonz = (opentoonzFor system).package;
        default = opentoonz;
      });

      checks = forAllSystems (system: {
        configure = (opentoonzFor system).configureCheck;
      });

      formatter = forAllSystems (system: (pkgsFor system).nixfmt);
    };
}
