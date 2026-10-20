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
    version = lib.strings.trim (builtins.readFile ./VERSION);
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = [hyprland.overlays.hyprland-packages];
      });
    sourceFiles =
      map (path: lib.removePrefix ((toString ./.) + "/") (toString path))
      (lib.filter (path: lib.hasSuffix ".cpp" (toString path) || lib.hasSuffix ".hpp" (toString path))
        (lib.filesystem.listFilesRecursive ./src));
    testFiles =
      map (path: lib.removePrefix ((toString ./.) + "/") (toString path))
      (lib.filter (path: lib.hasSuffix ".cpp" (toString path))
        (lib.filesystem.listFilesRecursive ./tests));
    testFileArgs = lib.concatMapStringsSep " " lib.escapeShellArg testFiles;
    sourceFileArgs = lib.concatMapStringsSep " " lib.escapeShellArg sourceFiles;
    # Keep style checks on native code and analyze imported workspace code for correctness.
    workspaceSourceFileArgs =
      lib.concatMapStringsSep " " lib.escapeShellArg
      (lib.filter (file: lib.hasPrefix "src/workspace/" file && lib.hasSuffix ".cpp" file) sourceFiles);
    tidySourceFiles = lib.filter (file: !(lib.hasPrefix "src/workspace/" file) && lib.hasSuffix ".cpp" file) sourceFiles;
    tidySourceFileArgs = lib.concatMapStringsSep " " lib.escapeShellArg tidySourceFiles;
    hyprpmManifest = builtins.fromTOML (builtins.readFile ./hyprpm.toml);
    hyprpmBuildCommands =
      lib.concatMapStringsSep "\n" (command: ''
        cd "$repoRoot"
        ${command}
      '')
      hyprpmManifest.hyprwinview.build;
  in {
    packages = eachSystem (system: let
      pkgs = pkgsFor.${system};
      hyprlandPkg = hyprland.packages.${system}.hyprland;
    in {
      default = pkgs.hyprlandPlugins.mkHyprlandPlugin {
        pluginName = "hyprwinview";
        inherit version;
        src = lib.fileset.toSource {
          root = ./.;
          fileset = lib.fileset.unions [./CMakeLists.txt ./VERSION ./src ./LICENSE ./licenses];
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
      src = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [./CMakeLists.txt ./VERSION ./src ./tests ./LICENSE ./licenses ./.clang-format ./.clang-tidy];
      };
    in {
      hyprwinview = self.packages.${system}.default;

      overview-behavior =
        pkgs.runCommand "hyprwinview-overview-behavior-tests" {
          inherit src;
          nativeBuildInputs = [pkgs.gcc pkgs.pkg-config];
          buildInputs = [pkgs.cairo];
          FONTCONFIG_FILE = pkgs.makeFontsConf {fontDirectories = [pkgs.dejavu_fonts];};
        } ''
          export XDG_CACHE_HOME="$TMPDIR/cache"
          mkdir -p "$XDG_CACHE_HOME"
          c++ -std=c++23 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined \
            -fno-sanitize-recover=undefined -I"$src/src" \
            $(pkg-config --cflags cairo) "$src/tests/overview_behavior.cpp" \
            $(pkg-config --libs cairo) -o test-overview
          ./test-overview
          touch "$out"
        '';

      input-lua-behavior =
        pkgs.runCommand "hyprwinview-input-lua-behavior-tests" {
          inherit src;
          nativeBuildInputs = [pkgs.gcc pkgs.pkg-config];
          buildInputs = [pkgs.lua5_5 pkgs.libxkbcommon];
        } ''
          c++ -std=c++23 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
            -fno-sanitize-recover=undefined -I"$src/src" \
            $(pkg-config --cflags lua xkbcommon) \
            "$src/tests/input_lua_behavior.cpp" "$src/src/lua_args.cpp" \
            "$src/src/overview/command.cpp" "$src/src/overview/keys.cpp" \
            $(pkg-config --libs lua xkbcommon) -o test-input-lua
          ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=use_stacks=0:use_registers=0 ./test-input-lua
          touch "$out"
        '';

      workspace-behavior =
        pkgs.runCommand "hyprwinview-workspace-behavior-tests" {
          inherit src;
          nativeBuildInputs = [pkgs.gcc];
        } ''
          c++ -std=c++23 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined \
            -fno-sanitize-recover=undefined -I"$src/src" \
            "$src/tests/workspace_limits.cpp" -o test-workspace
          ./test-workspace
          touch "$out"
        '';

      app-icon-lookup =
        pkgs.runCommand "hyprwinview-app-icon-lookup-tests" {
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

      clang-format =
        pkgs.runCommand "hyprwinview-clang-format-check" {
          inherit src;
          nativeBuildInputs = [pkgs.clang-tools];
        } ''
          cd "$src"
          clang-format --dry-run --Werror ${sourceFileArgs} ${testFileArgs}
          touch "$out"
        '';

      plugin-unloadable =
        pkgs.runCommand "hyprwinview-plugin-unloadable" {
          nativeBuildInputs = [pkgs.binutils pkgs.gawk];
        } ''
          readelf --dyn-syms --wide ${self.packages.${system}.default}/lib/libhyprwinview.so > symbols
          if awk '$5 == "UNIQUE" || /<OS specific>: 10/ { found = 1 } END { exit !found }' symbols; then
            echo "Plugin contains GNU_UNIQUE symbols that prevent unload" >&2
            exit 1
          fi
          touch "$out"
        '';

      hyprland-hook-symbols =
        pkgs.runCommand "hyprwinview-hyprland-hook-symbols" {
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
        inherit version;
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
        inherit version;
        inherit src;

        nativeBuildInputs =
          hyprlandPkg.nativeBuildInputs
          ++ [
            pkgs.clang-tools
          ];
        buildInputs = [hyprlandPkg pkgs.librsvg] ++ hyprlandPkg.buildInputs;

        configurePhase = ''
          runHook preConfigure
          cmake -S . -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
          # clang-tidy uses Clang's driver, which does not recognize this GCC-only symbol flag.
          sed -i 's/ -fno-gnu-unique / /g' build/compile_commands.json
          runHook postConfigure
        '';
        buildPhase = ''
          runHook preBuild
          clang-tidy -p build ${tidySourceFileArgs}
          clang-tidy -p build --checks='-*,clang-analyzer-*' ${workspaceSourceFileArgs}
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
