#Workspace overview module

This module is derived from
[raybbian/hyprtasking](https://github.com/raybbian/hyprtasking), imported from
commit `e53b9bd0440a0f85d64d86762fd84111d4de2e3d`.

The original standalone plugin entry points were replaced by the lifecycle
functions in `module.hpp`. The module otherwise retains the Hyprtasking Lua API,
dispatchers, configuration namespace, workspace layouts, renderer hooks, and
input behavior. Combined-mode changes ensure that activating the workspace
overview releases an active window overview and vice versa.

The imported scaled-render hooks are also shared by the live window overview.
Explicit render-pass context markers scope Hyprtasking's texture, border, and blur
adjustments to plugin-owned scaled content, so native Hyprland render modifiers in
the same frame are not mistaken for overview renders.

Hyprtasking's BSD-3-Clause license is retained in
`licenses/HYPRTASKING-BSD-3-CLAUSE.txt`.
