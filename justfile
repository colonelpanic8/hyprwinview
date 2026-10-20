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
    nix develop --command bash -c 'find src tests -type f \( -name "*.cpp" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i'

# Run clang-tidy through the flake check.
lint:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).clang-tidy"

# Build the plugin package.
build:
    nix build -L .#hyprwinview

# Exercise icon lookup and cache invalidation without a compositor.
test-icons:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).app-icon-lookup"

# Exercise filter editing and layer commands without a compositor.
test-overview:
    nix build -L ".#checks.$(nix eval --raw --impure --expr builtins.currentSystem).overview-behavior"
