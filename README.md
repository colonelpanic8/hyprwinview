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
            bg_col = "rgba(101014ee)",
            border_col = "rgba(ffffff33)",
            hover_border_col = "rgba(66ccffee)",
            border_size = 3,
            keys_left = "a,h,left",
            keys_right = "d,l,right",
            keys_up = "w,k,up",
            keys_down = "s,j,down",
            keys_go = "return,enter,space,g",
            keys_bring = "b,shift+return,shift+space",
            keys_close = "escape,q",
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
        },
    },
})
```

Keyboard key sets are comma-separated. Modifiers can be written with `+`, for
example `shift+return`.

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

On Hyprland 0.54 and older hyprlang configs, the same options live under
`plugin { hyprwinview { ... } }`.

## Dispatchers

```hyprlang
hyprwinview:overview toggle
hyprwinview:overview select
hyprwinview:overview bring
hyprwinview:overview close
```
