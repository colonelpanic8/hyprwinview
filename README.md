# hyprwinview

`hyprwinview` is an experimental Hyprland plugin that shows an overview of open
windows regardless of which workspace they are on.

The current version is a first working foundation:

- snapshots mapped windows into an overview grid
- sizes the grid from window count and monitor aspect ratio
- focuses the hovered window
- can bring the hovered window to the current workspace before focusing it
- supports keyboard navigation while the overview is active

## Build

```sh
nix build
```

The plugin is built at:

```sh
result/lib/libhyprwinview.so
```

For development:

```sh
direnv allow
```

## Hyprland Lua Config

Load the plugin:

```lua
hl.exec_once("hyprctl plugin load /path/to/libhyprwinview.so")
```

Example bindings:

```lua
hl.bind("SUPER + Tab", hl.dsp.exec_cmd("hyprctl dispatch hyprwinview:overview toggle"))
hl.bind("SUPER + SHIFT + Tab", hl.dsp.exec_cmd("hyprctl dispatch hyprwinview:overview toggle other-workspaces"))
hl.bind("SUPER + Return", hl.dsp.exec_cmd("hyprctl dispatch hyprwinview:overview select"))
hl.bind("SUPER + SHIFT + Return", hl.dsp.exec_cmd("hyprctl dispatch hyprwinview:overview bring"))
hl.bind("SUPER + Escape", hl.dsp.exec_cmd("hyprctl dispatch hyprwinview:overview close"))
```

Plugin options:

```lua
hl.config({
    plugin = {
        hyprwinview = {
            gap_size = 24,
            margin = 48,
            background = "rgba(10101499)",
            background_blur = 0,
            border_col = "rgba(ffffff33)",
            hover_border_col = "rgba(66ccffee)",
            border_size = 3,
            window_order = "natural",
            show_app_icon = 1,
            app_icon_size = 48,
            app_icon_theme = "",
            app_icon_theme_source = "auto",
            app_icon_overrides = "",
            app_icon_position = "bottom right",
            app_icon_anchor_x = -1.0,
            app_icon_anchor_y = -1.0,
            app_icon_margin_x = 10,
            app_icon_margin_y = 10,
            app_icon_margin_relative_x = 0.0,
            app_icon_margin_relative_y = 0.0,
            app_icon_offset_x = 0,
            app_icon_offset_y = 0,
            app_icon_backplate_col = "rgba(00000066)",
            app_icon_backplate_padding = 6,
            animation = "workspace_zoom",
            animation_in_ms = 180,
            animation_out_ms = 140,
            animation_scale = 0.94,
            animation_stagger_ms = 16,
            animation_stagger_max_ms = 120,
            animation_workspace_zoom_stage_ratio = 0.45,
            animation_workspace_zoom_gap = 18,
        },
    },
})

hl.plugin.hyprwinview.configure({
    keys = {
        left = { "a", "h", "left" },
        right = { "d", "l", "right" },
        up = { "w", "k", "up" },
        down = { "s", "j", "down" },
        go = { "return", "enter", "space", "g", "f" },
        bring = { "b", "shift+return", "shift+space" },
        bring_replace = { "shift + b" },
        close = { "escape", "q" },
    },
})
```

Keyboard key sets are Lua arrays when configuring through
`hl.plugin.hyprwinview.configure`. Modifiers can be written with `+`, for
example `shift+return` or `shift + b`. Modifier matching is exact for Shift,
Ctrl, Alt, and Super, so `b` and `shift + b` can trigger different actions. The
scalar `keys_*` plugin options still work as a fallback for hyprlang-style
configuration.

`background` controls the full-screen overview tint. Use a low alpha like
`rgba(10101466)` to leave the wallpaper visible, `rgba(10101400)` for no tint,
or `rgba(101014ff)` for an opaque backdrop. Set `background_blur = 1` to blur
the wallpaper behind the overview. The older `bg_col` option still works as a
legacy alias when `background` is left at its default value.

`window_order` accepts `natural` or `none` for the default Hyprland window order,
or `application` to keep windows from the same application grouped together
while preserving the first-seen group order and the within-application window
order. Aliases like `compositor`, `app`, `group_by_app`, and `grouped_by_app`
are also accepted.

`app_icon_position` accepts `left`, `right`, `top`, `bottom`, and `center`,
including combinations like `top left` or `bottom right`; single edges like
`top` or `right` center the other axis. Set `app_icon_anchor_x` or
`app_icon_anchor_y` to a value from `0.0` to `1.0` to override the token position
on an axis (`0.0` is left/top, `0.5` is center, `1.0` is right/bottom). Margins
are logical pixels plus optional relative tile fractions; offsets are final
logical-pixel nudges.

App icons are resolved through freedesktop icon themes before falling back to a
plain filesystem search. `app_icon_theme_source` accepts `auto`, `gtk`, `qt`,
`none`, or `legacy`; `auto` reads GTK settings and Qt/KDE settings, preferring
Qt first on KDE/LXQt sessions. Set `app_icon_theme` to force a specific icon
theme name, for example `Papirus-Dark`. Set `app_icon_overrides` to a
comma-separated list of `app_id=icon` pairs, where `icon` can be a themed icon
name or an absolute path.

`animation` accepts `fade_scale`, `staggered`, `workspace_zoom`, `fade`, or `none`.
`animation_scale` is the starting tile scale for scale-based modes; it is
clamped between `0.01` and `1.0`. `animation_stagger_ms` and
`animation_stagger_max_ms` control the per-tile delay for `staggered`.
`workspace_zoom` treats windows as living in a fixed workspace grid. Workspace
1 is placed in the first cell, workspace 2 in the second cell, and so on; nine
normal workspaces therefore produce a 3x3 plane. Other workspace counts use a
near-square grid sized from the highest normal workspace id Hyprland currently
knows about. The animation starts with the camera zoomed into the initially
focused workspace's fixed cell, zooms out to reveal the workspace plane, then
moves and resizes windows into the final overview grid.
`animation_workspace_zoom_stage_ratio` controls how much of the duration is
spent reaching the workspace-plane view before moving to the final grid, and
`animation_workspace_zoom_gap` controls the spacing between workspace cells in
that intermediate plane.

On Hyprland 0.54 and older hyprlang configs, the same options live under
`plugin { hyprwinview { ... } }`.

## Dispatchers

```hyprlang
hyprwinview:overview toggle
hyprwinview:overview toggle other-workspaces
hyprwinview:overview toggle exclude-current-workspace
hyprwinview:overview select
hyprwinview:overview bring
hyprwinview:overview bring-replace
hyprwinview:overview close
```

`toggle` with no filter keeps the default behavior and includes windows from all
workspaces. Add `other-workspaces`, `exclude-current-workspace`,
`without-current-workspace`, or `no-current-workspace` to omit windows on the
currently active workspace when opening the overview. `open`, `show`, and `on`
are also accepted when you want to open or replace the overview instead of
toggling it closed.

The Lua function accepts the same string arguments, or a table:

```lua
hl.plugin.hyprwinview.overview({
    action = "toggle",
    include_current_workspace = false,
})
```

`bring-replace` swaps the selected window with the window that was focused when
the overview opened, then focuses the selected window. If that original focused
window no longer exists, it falls back to normal bring behavior.
