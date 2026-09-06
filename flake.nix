{
  description = "hyprwinview, window and workspace overviews for Hyprland";

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
      "src/app_icon.cpp"
      "src/app_icon.hpp"
      "src/app_icon_loader.cpp"
      "src/app_icon_loader.hpp"
      "src/app_icon_lookup.cpp"
      "src/app_icon_lookup.hpp"
      "src/dispatcher.cpp"
      "src/dispatcher.hpp"
      "src/globals.hpp"
      "src/lua_api.cpp"
      "src/lua_api.hpp"
      "src/main.cpp"
      "src/overview.cpp"
      "src/overview.hpp"
      "src/overview/input.cpp"
      "src/overview/ordering.cpp"
      "src/winview_pass_element.cpp"
      "src/winview_pass_element.hpp"
      "src/workspace/config.hpp"
      "src/workspace/globals.hpp"
      "src/workspace/input.cpp"
      "src/workspace/layout/grid.cpp"
      "src/workspace/layout/grid.hpp"
      "src/workspace/layout/layout_base.cpp"
      "src/workspace/layout/layout_base.hpp"
      "src/workspace/layout/linear.cpp"
      "src/workspace/layout/linear.hpp"
      "src/workspace/manager.cpp"
      "src/workspace/manager.hpp"
      "src/workspace/module.cpp"
      "src/workspace/module.hpp"
      "src/workspace/overview.cpp"
      "src/workspace/overview.hpp"
      "src/workspace/pass/pass_element.cpp"
      "src/workspace/pass/pass_element.hpp"
      "src/workspace/render.cpp"
      "src/workspace/render.hpp"
      "src/workspace/types.hpp"
    ];
    sourceFileArgs = lib.concatMapStringsSep " " lib.escapeShellArg sourceFiles;
    # The imported Hyprtasking module retains its upstream conventions for this
    # first combined release. Format it with the rest of the tree, while keeping
    # the existing warnings-as-errors clang-tidy gate on native hyprwinview code.
    tidySourceFiles = lib.filter (file: !(lib.hasPrefix "src/workspace/" file)) sourceFiles;
    tidySourceFileArgs = lib.concatMapStringsSep " " lib.escapeShellArg tidySourceFiles;
    hyprpmManifest = builtins.fromTOML (builtins.readFile ./hyprpm.toml);
    hyprpmBuildCommands = lib.concatMapStringsSep "\n" (command: ''
      cd "$repoRoot"
      ${command}
    '') hyprpmManifest.hyprwinview.build;
  in {
    packages = eachSystem (system: let
      pkgs = pkgsFor.${system};
      hyprlandPkg = hyprland.packages.${system}.hyprland;
    in {
      default = pkgs.hyprlandPlugins.mkHyprlandPlugin {
        pluginName = "hyprwinview";
        version = "0.2.0";
        src = builtins.path {
          path = ./.;
          name = "hyprwinview-source";
        };

        inherit (hyprlandPkg) nativeBuildInputs;
        buildInputs = [pkgs.librsvg];

        meta = {
          description = "Window and workspace overviews for Hyprland";
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

      app-icon-lookup = pkgs.runCommand "hyprwinview-app-icon-lookup-tests" {
        inherit src;
        nativeBuildInputs = [pkgs.gcc pkgs.pkg-config];
        buildInputs = [pkgs.cairo pkgs.librsvg];
      } ''
        c++ -std=c++23 -O2 -Wall -Wextra -Werror -I"$src/src" \
          "$src/tests/app_icon_lookup.cpp" "$src/src/app_icon_lookup.cpp" -o test-icons
        ./test-icons "$TMPDIR/icons"
        c++ -std=c++23 -O2 -Wall -Wextra -Werror -pthread -I"$src/src" \
          $(pkg-config --cflags cairo librsvg-2.0) \
          "$src/tests/app_icon_loader.cpp" "$src/src/app_icon_loader.cpp" \
          "$src/src/app_icon_lookup.cpp" $(pkg-config --libs cairo librsvg-2.0) -o test-loader
        ./test-loader "$TMPDIR/loader"
        touch "$out"
      '';

      clang-format = pkgs.runCommand "hyprwinview-clang-format-check" {
        inherit src;
        nativeBuildInputs = [pkgs.clang-tools];
      } ''
        cd "$src"
        clang-format --dry-run --Werror ${sourceFileArgs} tests/app_icon_lookup.cpp tests/app_icon_loader.cpp
        touch "$out"
      '';

      hyprland-hook-symbols = pkgs.runCommand "hyprwinview-hyprland-hook-symbols" {
        nativeBuildInputs = [pkgs.binutils pkgs.gawk];
      } ''
        awk '
          /"_ZN/ {
            collecting = 1
            symbol = ""
          }
          collecting {
            line = $0
            while (match(line, /"[^"]*"/)) {
              token = substr(line, RSTART + 1, RLENGTH - 2)
              symbol = symbol token
              line = substr(line, RSTART + RLENGTH)
            }
            if ($0 ~ /\);[[:space:]]*$/) {
              print symbol
              collecting = 0
            }
          }
        ' ${src}/src/workspace/module.cpp > required-symbols
        test -s required-symbols
        nm -D -j ${hyprlandPkg}/bin/.Hyprland-wrapped > available-symbols

        while IFS= read -r symbol; do
          if ! grep -Fx -- "$symbol" available-symbols >/dev/null; then
            echo "Hyprland does not export required hook symbol: $symbol" >&2
            exit 1
          fi
        done < required-symbols

        touch "$out"
      '';

      hyprpm-build = pkgs.gcc14Stdenv.mkDerivation {
        pname = "hyprwinview-hyprpm-build-check";
        version = "0.2.0";
        inherit src;

        inherit (hyprlandPkg) nativeBuildInputs;
        buildInputs = [hyprlandPkg pkgs.librsvg] ++ hyprlandPkg.buildInputs;

        dontConfigure = true;

        buildPhase = ''
          runHook preBuild
          repoRoot="$PWD"
          test ${lib.escapeShellArg hyprpmManifest.repository.name} = hyprwinview
          test ${lib.escapeShellArg hyprpmManifest.hyprwinview.output} = build/libhyprwinview.so
          ${hyprpmBuildCommands}
          test -f ${lib.escapeShellArg hyprpmManifest.hyprwinview.output}
          runHook postBuild
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p "$out"
          cp ${lib.escapeShellArg hyprpmManifest.hyprwinview.output} "$out/"
          runHook postInstall
        '';
      };

      clang-tidy = pkgs.gcc14Stdenv.mkDerivation {
        pname = "hyprwinview-clang-tidy";
        version = "0.2.0";
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
          clang-tidy -p build ${tidySourceFileArgs}
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
