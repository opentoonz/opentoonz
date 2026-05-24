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
        system: qtMajor:
        import ./nix/opentoonz-env.nix {
          pkgs = pkgsFor system;
          inherit lib;
          src = self;
          inherit qtMajor;
        };
    in
    {
      devShells = forAllSystems (
        system:
        let
          qt5Env = opentoonzFor system 5;
          qt6Env = opentoonzFor system 6;
        in
        {
          default = qt5Env.devShell;
          qt6 = qt6Env.devShell;
        }
      );

      packages = forAllSystems (
        system:
        let
          qt5Env = opentoonzFor system 5;
          qt6Env = opentoonzFor system 6;
        in
        rec {
          opentoonz = qt5Env.package;
          opentoonz-qt6 = qt6Env.package;
          default = opentoonz;
        }
      );

      checks = forAllSystems (
        system:
        let
          qt5Env = opentoonzFor system 5;
          qt6Env = opentoonzFor system 6;
        in
        {
          configure = qt5Env.configureCheck;
          configure-qt6 = qt6Env.configureCheck;
        }
      );

      formatter = forAllSystems (system: (pkgsFor system).nixfmt);
    };
}
