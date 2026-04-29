{
  description = "hyprwinview, a window overview plugin for Hyprland";

  inputs = {
    hyprland.url = "git+https://github.com/hyprwm/Hyprland?submodules=1&ref=refs/pull/13817/head";
    nixpkgs.follows = "hyprland/nixpkgs";
    systems.follows = "hyprland/systems";
  };

  outputs = {
    self,
    hyprland,
    nixpkgs,
    systems,
    ...
  }: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = [hyprland.overlays.hyprland-packages];
      });
  in {
    packages = eachSystem (system: let
      pkgs = pkgsFor.${system};
      hyprlandPkg = hyprland.packages.${system}.hyprland;
    in {
      default = pkgs.hyprlandPlugins.mkHyprlandPlugin {
        pluginName = "hyprwinview";
        version = "0.1.0";
        src = builtins.path {
          path = ./.;
          name = "hyprwinview-source";
        };

        inherit (hyprlandPkg) nativeBuildInputs;

        meta = {
          description = "A window overview plugin for Hyprland";
          homepage = "https://github.com/colonelpanic8/hyprwinview";
          license = lib.licenses.bsd3;
          platforms = lib.platforms.linux;
        };
      };

      hyprwinview = self.packages.${system}.default;
    });

    checks = eachSystem (system: {
      hyprwinview = self.packages.${system}.default;
    });

    devShells = eachSystem (system: let
      pkgs = pkgsFor.${system};
      hyprlandPkg = hyprland.packages.${system}.hyprland;
    in {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc14Stdenv;} {
        name = "hyprwinview";
        buildInputs = [hyprlandPkg];
        inputsFrom = [hyprlandPkg self.packages.${system}.default];
      };
    });
  };
}
