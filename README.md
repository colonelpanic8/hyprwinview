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

## Hyprland Config

Load the plugin:

```hyprlang
exec-once = hyprctl plugin load /path/to/libhyprwinview.so
```

Example bindings:

```hyprlang
bind = SUPER, tab, hyprwinview:overview, toggle
bind = SUPER, return, hyprwinview:overview, select
bind = SUPER SHIFT, return, hyprwinview:overview, bring
bind = SUPER, escape, hyprwinview:overview, close
```

Plugin options:

```hyprlang
plugin {
    hyprwinview {
        gap_size = 24
        margin = 48
        bg_col = rgba(101014ee)
        border_col = rgba(ffffff33)
        hover_border_col = rgba(66ccffee)
        border_size = 3
        keys_left = a,h,left
        keys_right = d,l,right
        keys_up = w,k,up
        keys_down = s,j,down
        keys_go = return,enter,space,g
        keys_bring = b,shift+return,shift+space
        keys_close = escape,q
    }
}
```

Keyboard key sets are comma-separated. Modifiers can be written with `+`, for
example `shift+return`.

## Dispatchers

```hyprlang
hyprwinview:overview toggle
hyprwinview:overview select
hyprwinview:overview bring
hyprwinview:overview close
```
