#ifndef HYPRWINVIEW_SETTINGS_HPP
#define HYPRWINVIEW_SETTINGS_HPP

#include <hyprland/src/config/ConfigValue.hpp>

#define WINVIEW_SETTINGS(X)                                                                        \
    X(CIntValue, INTEGER, PGAP, "gap_size", "gap size", 24)                                        \
    X(CIntValue, INTEGER, PMARGIN, "margin", "margin", 48)                                         \
    X(CColorValue, INTEGER, PBACKGROUND, "background", "background color", 0x99101014)             \
    X(CIntValue, INTEGER, PBACKGROUNDBLUR, "background_blur",                                      \
      "blur the background behind the overview", 0)                                                \
    X(CColorValue, INTEGER, PBG, "bg_col", "legacy background color alias", 0x99101014)            \
    X(CColorValue, INTEGER, PBORDER, "border_col", "border color", 0x33FFFFFF)                     \
    X(CColorValue, INTEGER, PHOVERBORDER, "hover_border_col", "hover border color", 0xEE66CCFF)    \
    X(CIntValue, INTEGER, PBORDERSIZE, "border_size", "border size", 3)                            \
    X(CStringValue, STRING, PWINDOWORDER, "window_order", "overview window ordering strategy",     \
      "natural")                                                                                   \
    X(CStringValue, STRING, PKEYSLEFT, "keys_left", "left keys", "a,h,left")                       \
    X(CStringValue, STRING, PKEYSRIGHT, "keys_right", "right keys", "d,l,right")                   \
    X(CStringValue, STRING, PKEYSUP, "keys_up", "up keys", "w,k,up")                               \
    X(CStringValue, STRING, PKEYSDOWN, "keys_down", "down keys", "s,j,down")                       \
    X(CStringValue, STRING, PKEYSGO, "keys_go", "go keys", "return,enter,space,g,f")               \
    X(CStringValue, STRING, PKEYSDEFAULTACTION, "keys_default_action", "default action keys", "")  \
    X(CStringValue, STRING, PKEYSBRING, "keys_bring", "bring keys", "b,shift+return,shift+space")  \
    X(CStringValue, STRING, PKEYSBRINGREPLACE, "keys_bring_replace", "bring replace keys",         \
      "shift+b")                                                                                   \
    X(CStringValue, STRING, PKEYSCLOSE, "keys_close", "close keys", "escape,q")                    \
    X(CStringValue, STRING, PKEYSFILTERTOGGLE, "keys_filter_toggle", "filter mode toggle keys",    \
      "/")                                                                                         \
    X(CStringValue, STRING, PKEYSFILTERCLOSE, "keys_filter_close", "filter mode close keys",       \
      "escape,ctrl+g")                                                                             \
    X(CStringValue, STRING, PKEYSFILTERBRING, "keys_filter_bring", "filter mode bring keys",       \
      "ctrl+b")                                                                                    \
    X(CStringValue, STRING, PKEYSFILTERBRINGREPLACE, "keys_filter_bring_replace",                  \
      "filter mode bring replace keys", "ctrl+shift+b")                                            \
    X(CStringValue, STRING, PKEYSFILTERLEFT, "keys_filter_left", "filter mode left keys",          \
      "left,ctrl+a,super+a")                                                                       \
    X(CStringValue, STRING, PKEYSFILTERRIGHT, "keys_filter_right", "filter mode right keys",       \
      "right,ctrl+d,super+d")                                                                      \
    X(CStringValue, STRING, PKEYSFILTERUP, "keys_filter_up", "filter mode up keys",                \
      "up,ctrl+p,ctrl+w,super+w")                                                                  \
    X(CStringValue, STRING, PKEYSFILTERDOWN, "keys_filter_down", "filter mode down keys",          \
      "down,ctrl+n,ctrl+s,super+s")                                                                \
    X(CIntValue, INTEGER, PSHOWAPPICON, "show_app_icon", "show app icon overlays", 0)              \
    X(CIntValue, INTEGER, PAPPICONSIZE, "app_icon_size", "app icon size", 48)                      \
    X(CStringValue, STRING, PAPPICONTHEME, "app_icon_theme", "app icon theme override", "")        \
    X(CStringValue, STRING, PAPPICONTHEMESOURCE, "app_icon_theme_source", "app icon theme source", \
      "auto")                                                                                      \
    X(CStringValue, STRING, PAPPICONOVERRIDES, "app_icon_overrides",                               \
      "app icon app_id=icon overrides", "")                                                        \
    X(CStringValue, STRING, PAPPICONPOSITION, "app_icon_position", "app icon position",            \
      "bottom right")                                                                              \
    X(CFloatValue, FLOAT, PAPPICONANCHORX, "app_icon_anchor_x",                                    \
      "app icon normalized x anchor override", -1.0F)                                              \
    X(CFloatValue, FLOAT, PAPPICONANCHORY, "app_icon_anchor_y",                                    \
      "app icon normalized y anchor override", -1.0F)                                              \
    X(CIntValue, INTEGER, PAPPICONMARGINX, "app_icon_margin_x", "app icon horizontal margin", 10)  \
    X(CIntValue, INTEGER, PAPPICONMARGINY, "app_icon_margin_y", "app icon vertical margin", 10)    \
    X(CFloatValue, FLOAT, PAPPICONMARGINRELX, "app_icon_margin_relative_x",                        \
      "app icon relative horizontal margin", 0.0F)                                                 \
    X(CFloatValue, FLOAT, PAPPICONMARGINRELY, "app_icon_margin_relative_y",                        \
      "app icon relative vertical margin", 0.0F)                                                   \
    X(CIntValue, INTEGER, PAPPICONOFFSETX, "app_icon_offset_x", "app icon horizontal offset", 0)   \
    X(CIntValue, INTEGER, PAPPICONOFFSETY, "app_icon_offset_y", "app icon vertical offset", 0)     \
    X(CColorValue, INTEGER, PAPPICONBACKPLATE, "app_icon_backplate_col",                           \
      "app icon backplate color", 0x66000000)                                                      \
    X(CIntValue, INTEGER, PAPPICONBACKPLATEPADDING, "app_icon_backplate_padding",                  \
      "app icon backplate padding", 6)                                                             \
    X(CIntValue, INTEGER, PSHOWWINDOWTEXT, "show_window_text",                                     \
      "show window title and class labels", 1)                                                     \
    X(CStringValue, STRING, PWINDOWTEXTFONT, "window_text_font", "window text font", "Sans")       \
    X(CIntValue, INTEGER, PWINDOWTEXTSIZE, "window_text_size", "window text size", 14)             \
    X(CColorValue, INTEGER, PWINDOWTEXTCOLOR, "window_text_color", "window text color",            \
      0xFFFFFFFF)                                                                                  \
    X(CColorValue, INTEGER, PWINDOWTEXTBACKPLATE, "window_text_backplate_col",                     \
      "window text backplate color", 0x99000000)                                                   \
    X(CIntValue, INTEGER, PWINDOWTEXTPADDING, "window_text_padding", "window text padding", 6)     \
    X(CIntValue, INTEGER, PFILTERANIMATIONMS, "filter_animation_ms",                               \
      "filter narrowing animation duration", 140)                                                  \
    X(CStringValue, STRING, PANIMATION, "animation", "overview animation mode", "workspace_zoom")  \
    X(CIntValue, INTEGER, PANIMATIONINMS, "animation_in_ms",                                       \
      "overview open animation duration in milliseconds", 280)                                     \
    X(CIntValue, INTEGER, PANIMATIONOUTMS, "animation_out_ms",                                     \
      "overview close animation duration in milliseconds", 220)                                    \
    X(CFloatValue, FLOAT, PANIMATIONSPEED, "animation_speed",                                      \
      "overview animation speed multiplier", 1.0F)                                                 \
    X(CFloatValue, FLOAT, PANIMATIONSCALE, "animation_scale",                                      \
      "overview fade_scale starting scale", 0.94F)                                                 \
    X(CIntValue, INTEGER, PANIMATIONSTAGGERMS, "animation_stagger_ms",                             \
      "overview staggered animation delay between tiles in milliseconds", 16)                      \
    X(CIntValue, INTEGER, PANIMATIONSTAGGERMAXMS, "animation_stagger_max_ms",                      \
      "overview staggered animation maximum tile delay in milliseconds", 120)                      \
    X(CFloatValue, FLOAT, PANIMATIONWORKSPACEZOOMSTAGERATIO,                                       \
      "animation_workspace_zoom_stage_ratio", "overview workspace_zoom first-stage fraction",      \
      0.45F)                                                                                       \
    X(CIntValue, INTEGER, PANIMATIONWORKSPACEZOOMGAP, "animation_workspace_zoom_gap",              \
      "overview workspace_zoom panel gap", 18)

namespace WinviewConfig {
#define DECLARE_SETTING(factory, valueType, name, key, description, initial)                       \
    inline constexpr auto                         name##Default = initial;                         \
    inline const CConfigValue<Config::valueType>& name() {                                         \
        static const CConfigValue<Config::valueType> value("plugin:hyprwinview:" key);             \
        return value;                                                                              \
    }
    WINVIEW_SETTINGS(DECLARE_SETTING)
#undef DECLARE_SETTING
}

#endif
