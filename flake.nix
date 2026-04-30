{
  description = "hyprwinview, a window overview plugin for Hyprland";

  inputs = {
    hyprland.url = "git+https://github.com/hyprwm/Hyprland?submodules=1";
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
    sourceFiles = [
      "src/AppIcon.cpp"
      "src/AppIcon.hpp"
      "src/WinviewPassElement.cpp"
      "src/WinviewPassElement.hpp"
      "src/globals.hpp"
      "src/main.cpp"
      "src/overview.cpp"
      "src/overview.hpp"
    ];
    sourceFileArgs = lib.concatMapStringsSep " " lib.escapeShellArg sourceFiles;
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
        buildInputs = [pkgs.librsvg];

        meta = {
          description = "A window overview plugin for Hyprland";
          homepage = "https://github.com/colonelpanic8/hyprwinview";
          license = lib.licenses.bsd3;
          platforms = lib.platforms.linux;
        };
      };

      hyprwinview = self.packages.${system}.default;
    });

    checks = eachSystem (system: let
      pkgs = pkgsFor.${system};
      hyprlandPkg = hyprland.packages.${system}.hyprland;
      src = builtins.path {
        path = ./.;
        name = "hyprwinview-source";
      };
    in {
      hyprwinview = self.packages.${system}.default;

      clang-format = pkgs.runCommand "hyprwinview-clang-format-check" {
        inherit src;
        nativeBuildInputs = [pkgs.clang-tools];
      } ''
        cd "$src"
        clang-format --dry-run --Werror ${sourceFileArgs}
        touch "$out"
      '';

      clang-tidy = pkgs.gcc14Stdenv.mkDerivation {
        pname = "hyprwinview-clang-tidy";
        version = "0.1.0";
        inherit src;

        inputsFrom = [
          hyprlandPkg
          self.packages.${system}.default
        ];
        nativeBuildInputs = hyprlandPkg.nativeBuildInputs ++ [
          pkgs.clang-tools
        ];
        buildInputs = [hyprlandPkg pkgs.librsvg] ++ hyprlandPkg.buildInputs;

        configurePhase = ''
          runHook preConfigure
          cmake -S . -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
          runHook postConfigure
        '';
        buildPhase = ''
          runHook preBuild
          clang-tidy -p build ${sourceFileArgs}
          runHook postBuild
        '';
        installPhase = ''
          touch "$out"
        '';
      };
    });

    devShells = eachSystem (system: let
      pkgs = pkgsFor.${system};
      hyprlandPkg = hyprland.packages.${system}.hyprland;
    in {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc14Stdenv;} {
        name = "hyprwinview";
        buildInputs = [hyprlandPkg pkgs.librsvg];
        nativeBuildInputs = [pkgs.clang-tools];
        inputsFrom = [hyprlandPkg self.packages.${system}.default];
      };
    });
  };
}
