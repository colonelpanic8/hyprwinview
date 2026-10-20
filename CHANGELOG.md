# Changelog

## 0.2.2

- Skip completed opening-animation calculations while retaining live filter transitions.
- Resolve shared icon, text, and border drawing settings once per frame.
- Tokenize each filter query once per preview rebuild instead of once per window.

## 0.2.1

- Fix plugin unload/reload lifetime, and explicitly remove event listeners and GPU caches at shutdown.
- Keep window previews current as windows open, close, change title, or move between workspaces.
- Fix cross-monitor selection, fullscreen overview visibility, rotated-monitor backgrounds, and monitor-removal cleanup.
- Preserve UTF-8 characters when deleting filter text and shortening window titles.
- Reuse label textures during animations and discard stale textures instead of retaining them indefinitely.
- Preserve global shortcuts in filter mode without leaking unhandled keys to applications, balance key and button releases after closing, and allow reopening during close animations.
- Hit-test the current cursor position when selecting after window metadata changes.
- Validate Lua arguments without jumping across C++ destructors. Reset Lua key overrides on successful reload, preserve them on syntax errors, and retain unspecified scalar key settings.
- Invalidate icon lookup caches on configuration reload and cancel outstanding lookup work during shutdown.
- Refresh workspace grids when workspaces change, prevent rendering another monitor's workspace, and reject malformed layer commands safely.
- Prevent stale close callbacks from closing a reopened overview, and clamp invalid grid and gesture settings in both configuration formats.
- Centralize configuration defaults and version metadata, extract animation/text/input helpers, and expand regression and static-analysis checks.
