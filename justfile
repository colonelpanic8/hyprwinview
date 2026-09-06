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
        src/app_icon_loader.cpp \
        src/app_icon_loader.hpp \
        src/app_icon_lookup.cpp \
        src/app_icon_lookup.hpp \
        src/dispatcher.cpp \
        src/dispatcher.hpp \
        src/globals.hpp \
        src/lua_api.cpp \
        src/lua_api.hpp \
        src/main.cpp \
        src/overview.cpp \
        src/overview.hpp \
        src/overview/input.cpp \
        src/overview/ordering.cpp \
        src/winview_pass_element.cpp \
        src/winview_pass_element.hpp \
        src/workspace/config.hpp \
        src/workspace/globals.hpp \
        src/workspace/input.cpp \
        src/workspace/layout/grid.cpp \
        src/workspace/layout/grid.hpp \
        src/workspace/layout/layout_base.cpp \
        src/workspace/layout/layout_base.hpp \
        src/workspace/layout/linear.cpp \
        src/workspace/layout/linear.hpp \
        src/workspace/manager.cpp \
        src/workspace/manager.hpp \
        src/workspace/module.cpp \
        src/workspace/module.hpp \
        src/workspace/overview.cpp \
        src/workspace/overview.hpp \
        src/workspace/pass/pass_element.cpp \
        src/workspace/pass/pass_element.hpp \
        src/workspace/render.cpp \
        src/workspace/render.hpp \
        src/workspace/types.hpp \
        tests/app_icon_lookup.cpp \
        tests/app_icon_loader.cpp

# Run clang-tidy through the flake check.
lint:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).clang-tidy"

# Build the plugin package.
build:
    nix build -L .#hyprwinview

# Exercise icon lookup and cache invalidation without a compositor.
test-icons:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).app-icon-lookup"
