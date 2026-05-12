set shell := ["bash", "-euo", "pipefail", "-c"]

default: check

all: check

# Run the same checks as CI.
check:
    nix flake check -L --keep-going

# Check clang-format without modifying files.
format-check:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).clang-format"

# Apply clang-format to the tracked C++ sources.
format:
    nix develop --command clang-format -i \
        src/app_icon.cpp \
        src/app_icon.hpp \
        src/dispatcher.cpp \
        src/dispatcher.hpp \
        src/globals.hpp \
        src/lua_api.cpp \
        src/lua_api.hpp \
        src/main.cpp \
        src/overview.cpp \
        src/overview.hpp \
        src/winview_pass_element.cpp \
        src/winview_pass_element.hpp

# Run clang-tidy through the flake check.
lint:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).clang-tidy"

# Build the plugin package.
build:
    nix build -L .#hyprwinview
