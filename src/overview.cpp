#include "overview.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <cairo/cairo.h>
#define private   public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#undef private
#undef protected

#include <xkbcommon/xkbcommon.h>

#include "app_icon.hpp"
#include "globals.hpp"
#include "winview_pass_element.hpp"
#include "workspace/render.hpp"

static const CConfigValue<Config::INTEGER>& PGAP() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:gap_size");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PMARGIN() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:margin");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBG() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:bg_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBACKGROUND() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:background");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBACKGROUNDBLUR() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:background_blur");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBORDER() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:border_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PHOVERBORDER() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:hover_border_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBORDERSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:border_size");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PSHOWAPPICON() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:show_app_icon");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_size");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PAPPICONPOSITION() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:app_icon_position");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONANCHORX() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_anchor_x");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONANCHORY() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_anchor_y");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONMARGINX() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_margin_x");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONMARGINY() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_margin_y");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONMARGINRELX() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_margin_relative_x");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONMARGINRELY() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_margin_relative_y");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONOFFSETX() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_offset_x");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONOFFSETY() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_offset_y");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONBACKPLATE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_backplate_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONBACKPLATEPADDING() {
    static const CConfigValue<Config::INTEGER> VALUE(
        "plugin:hyprwinview:app_icon_backplate_padding");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PSHOWWINDOWTEXT() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:show_window_text");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PWINDOWTEXTFONT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:window_text_font");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:window_text_size");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTCOLOR() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:window_text_color");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTBACKPLATE() {
    static const CConfigValue<Config::INTEGER> VALUE(
        "plugin:hyprwinview:window_text_backplate_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTPADDING() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:window_text_padding");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PFILTERANIMATIONMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:filter_animation_ms");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PANIMATION() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:animation");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONINMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_in_ms");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONOUTMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_out_ms");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PANIMATIONSCALE() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:animation_scale");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PANIMATIONSPEED() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:animation_speed");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONSTAGGERMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_stagger_ms");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONSTAGGERMAXMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_stagger_max_ms");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PANIMATIONWORKSPACEZOOMSTAGERATIO() {
    static const CConfigValue<Config::FLOAT> VALUE(
        "plugin:hyprwinview:animation_workspace_zoom_stage_ratio");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONWORKSPACEZOOMGAP() {
    static const CConfigValue<Config::INTEGER> VALUE(
        "plugin:hyprwinview:animation_workspace_zoom_gap");
    return VALUE;
}

enum class EOverviewAnimation : uint8_t {
    NONE,
    FADE,
    FADE_SCALE,
    STAGGERED_FADE_SCALE,
    WORKSPACE_ZOOM,
};

static constexpr uint64_t DEFAULT_BACKGROUND = 0x99101014;

static std::string        trimmedLower(std::string token) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    token.erase(token.begin(), std::ranges::find_if(token, notSpace));
    token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(), token.end());
    std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });
    return token;
}

static std::string configStringOr(const CConfigValue<Config::STRING>& value,
                                  const std::string&                  fallback) {
    try {
        return *value;
    } catch (...) { return fallback; }
}

static std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

static std::vector<std::string> queryTokens(const std::string& query) {
    std::vector<std::string> result;
    std::stringstream        stream(lower(query));
    std::string              token;

    while (stream >> token)
        result.push_back(token);

    return result;
}

static std::string windowTitle(const PHLWINDOW& window) {
    if (!window)
        return "";

    if (!window->m_title.empty())
        return window->m_title;

    return window->m_initialTitle;
}

static std::string windowClass(const PHLWINDOW& window) {
    if (!window)
        return "";

    if (!window->m_class.empty())
        return window->m_class;

    return window->m_initialClass;
}

static std::string searchableWindowText(const PHLWINDOW& window) {
    if (!window)
        return "";

    return lower(windowTitle(window) + " " + windowClass(window) + " " + window->m_initialTitle +
                 " " + window->m_initialClass);
}

static bool windowMatchesQuery(const PHLWINDOW& window, const std::string& query) {
    const auto TOKENS = queryTokens(query);
    if (TOKENS.empty())
        return true;

    const auto SEARCHABLE = searchableWindowText(window);
    return std::ranges::all_of(TOKENS, [&SEARCHABLE](const std::string& token) {
        return SEARCHABLE.find(token) != std::string::npos;
    });
}

static EOverviewAnimation overviewAnimation() {
    const auto NAME = trimmedLower(configStringOr(PANIMATION(), "fade_scale"));

    if (NAME == "none" || NAME == "off" || NAME == "disable" || NAME == "disabled")
        return EOverviewAnimation::NONE;
    if (NAME == "fade")
        return EOverviewAnimation::FADE;
    if (NAME == "stagger" || NAME == "staggered" || NAME == "staggered_fade_scale")
        return EOverviewAnimation::STAGGERED_FADE_SCALE;
    if (NAME == "workspace_zoom" || NAME == "workspace-zoom" || NAME == "expo" ||
        NAME == "hyprexpo")
        return EOverviewAnimation::WORKSPACE_ZOOM;

    return EOverviewAnimation::FADE_SCALE;
}

static bool animationScalesTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::FADE_SCALE ||
        animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationStaggersTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationUsesWorkspaceZoom(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::WORKSPACE_ZOOM;
}

static double workspaceZoomStageRatio() {
    return std::clamp<double>(*PANIMATIONWORKSPACEZOOMSTAGERATIO(), 0.1, 0.9);
}

static int workspaceGridColsForCount(int count) {
    return std::max(1, (int)std::ceil(std::sqrt(std::max(1, count))));
}

static int workspaceGridRowsForCount(int count, int cols) {
    return std::max(1, (int)std::ceil((double)std::max(1, count) / std::max(1, cols)));
}

static double animationDurationMs(bool closing) {
    const double SPEED = std::clamp<double>(*PANIMATIONSPEED(), 0.1, 10.0);
    return static_cast<double>(
               std::max<Config::INTEGER>(0, closing ? *PANIMATIONOUTMS() : *PANIMATIONINMS())) /
        SPEED;
}

static double easeOutCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 3.0);
}

static double easeInCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t;
}

static double visibleAmountForElapsed(double elapsedMs, double durationMs, bool closing) {
    if (durationMs <= 0.0)
        return closing ? 0.0 : 1.0;

    const auto T = std::clamp(elapsedMs / durationMs, 0.0, 1.0);
    return closing ? 1.0 - easeInCubic(T) : easeOutCubic(T);
}

static double rawProgressForVisibleAmount(double visible, bool closing) {
    visible = std::clamp(visible, 0.0, 1.0);

    if (closing)
        return std::cbrt(1.0 - visible);

    return 1.0 - std::cbrt(1.0 - visible);
}

static CBox scaleBoxFromCenter(const CBox& box, double scale) {
    const auto CENTER = box.middle();
    const auto W      = box.w * scale;
    const auto H      = box.h * scale;

    return {CENTER.x - W / 2.0, CENTER.y - H / 2.0, W, H};
}

static CHyprColor multiplyAlpha(const CHyprColor& color, double alpha) {
    return color.modifyA(static_cast<float>(color.a * std::clamp(alpha, 0.0, 1.0)));
}

static double lerpDouble(double from, double to, double progress) {
    return from + (to - from) * std::clamp(progress, 0.0, 1.0);
}

static CBox lerpBox(const CBox& from, const CBox& to, double progress) {
    return {
        lerpDouble(from.x, to.x, progress),
        lerpDouble(from.y, to.y, progress),
        lerpDouble(from.w, to.w, progress),
        lerpDouble(from.h, to.h, progress),
    };
}

static CHyprColor activeBackgroundColor() {
    const auto BACKGROUND = static_cast<uint64_t>(*PBACKGROUND());
    const auto BG_COL     = static_cast<uint64_t>(*PBG());

    if (BACKGROUND == DEFAULT_BACKGROUND && BG_COL != DEFAULT_BACKGROUND)
        return CHyprColor(BG_COL);

    return CHyprColor(BACKGROUND);
}

struct STextTexture {
    SP<Render::ITexture> texture;
    Vector2D             size;
};

struct STextTextureCacheEntry {
    SP<Render::ITexture> texture;
    Vector2D             size;
};

static std::unordered_map<std::string, STextTextureCacheEntry> g_textTextureCache;

static double cairoTextWidth(cairo_t* cr, const std::string& text) {
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    return extents.x_advance;
}

static std::string elideTextToWidth(cairo_t* cr, const std::string& text, double maxWidth) {
    if (text.empty() || cairoTextWidth(cr, text) <= maxWidth)
        return text;

    static constexpr const char* ELLIPSIS = "...";
    if (cairoTextWidth(cr, ELLIPSIS) > maxWidth)
        return "";

    std::string result = text;
    while (!result.empty() && cairoTextWidth(cr, result + ELLIPSIS) > maxWidth)
        result.pop_back();

    return result + ELLIPSIS;
}

static STextTexture textTextureForLines(const std::vector<std::string>& lines, int fontSizePx,
                                        int maxWidthPx, const CHyprColor& color,
                                        const std::string& font) {
    if (lines.empty() || fontSizePx <= 0 || maxWidthPx <= 0)
        return {};

    std::string key = font + "|" + std::to_string(fontSizePx) + "|" + std::to_string(maxWidthPx) +
        "|" + std::to_string((uint64_t)(color.r * 255.0)) + "," +
        std::to_string((uint64_t)(color.g * 255.0)) + "," +
        std::to_string((uint64_t)(color.b * 255.0)) + "," +
        std::to_string((uint64_t)(color.a * 255.0));
    for (const auto& line : lines)
        key += "|" + line;

    if (const auto IT = g_textTextureCache.find(key); IT != g_textTextureCache.end())
        return {.texture = IT->second.texture, .size = IT->second.size};

    cairo_surface_t* measureSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t*         measure        = cairo_create(measureSurface);
    cairo_select_font_face(measure, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(measure, fontSizePx);

    cairo_font_extents_t fontExtents;
    cairo_font_extents(measure, &fontExtents);

    std::vector<std::string> renderedLines;
    renderedLines.reserve(lines.size());
    double width = 1.0;
    for (const auto& line : lines) {
        auto renderedLine = elideTextToWidth(measure, line, maxWidthPx);
        width             = std::max(width, cairoTextWidth(measure, renderedLine));
        renderedLines.push_back(std::move(renderedLine));
    }

    const int textureWidth  = std::max(1, (int)std::ceil(std::min<double>(width, maxWidthPx)));
    const int lineHeight    = std::max(1, (int)std::ceil(fontExtents.height));
    const int textureHeight = std::max(1, lineHeight * (int)renderedLines.size());

    cairo_destroy(measure);
    cairo_surface_destroy(measureSurface);

    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textureWidth, textureHeight);
    cairo_t* cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_select_font_face(cr, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fontSizePx);
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

    for (size_t i = 0; i < renderedLines.size(); ++i) {
        cairo_move_to(cr, 0.0, fontExtents.ascent + static_cast<double>(i) * lineHeight);
        cairo_show_text(cr, renderedLines[i].c_str());
    }

    cairo_destroy(cr);

    auto texture = g_pHyprRenderer->createTexture(surface);
    cairo_surface_destroy(surface);

    if (!texture)
        return {};

    const Vector2D size{(double)textureWidth, (double)textureHeight};
    g_textTextureCache.emplace(key, STextTextureCacheEntry{.texture = texture, .size = size});
    return {.texture = texture, .size = size};
}

static Vector2D iconAnchorFromPosition(const std::string& position) {
    auto   anchor = Vector2D{1.0, 1.0};
    auto   value  = lower(position);
    size_t start  = 0;
    bool   hasX   = false;
    bool   hasY   = false;
    bool   center = false;

    while (start < value.size()) {
        const auto END = value.find_first_of(" ,", start);
        const auto TOKEN =
            value.substr(start, END == std::string::npos ? std::string::npos : END - start);

        if (TOKEN == "left") {
            anchor.x = 0.0;
            hasX     = true;
        } else if (TOKEN == "right") {
            anchor.x = 1.0;
            hasX     = true;
        } else if (TOKEN == "top") {
            anchor.y = 0.0;
            hasY     = true;
        } else if (TOKEN == "bottom") {
            anchor.y = 1.0;
            hasY     = true;
        } else if (TOKEN == "center" || TOKEN == "middle")
            center = true;

        if (END == std::string::npos)
            break;

        start = value.find_first_not_of(" ,", END);
        if (start == std::string::npos)
            break;
    }

    if (center || (hasY && !hasX))
        anchor.x = hasX ? anchor.x : 0.5;

    if (center || (hasX && !hasY))
        anchor.y = hasY ? anchor.y : 0.5;

    return anchor;
}

static double anchorOverride(double configured, double fallback) {
    return configured >= 0.0 ? std::clamp(configured, 0.0, 1.0) : fallback;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static double edgeSignedMargin(double anchor, double absolute, double relative, double extent) {
    if (anchor < 0.5)
        return absolute + relative * extent;
    if (anchor > 0.5)
        return -(absolute + relative * extent);
    return 0.0;
}

static CBox appIconBoxForTile(const CBox& tileLogical, double scale) {
    const int SIZE   = static_cast<int>(std::max<Config::INTEGER>(1, *PAPPICONSIZE()));
    Vector2D  anchor = iconAnchorFromPosition(configStringOr(PAPPICONPOSITION(), "bottom right"));
    anchor.x         = anchorOverride(*PAPPICONANCHORX(), anchor.x);
    anchor.y         = anchorOverride(*PAPPICONANCHORY(), anchor.y);

    const double xMargin = edgeSignedMargin(anchor.x, static_cast<double>(*PAPPICONMARGINX()),
                                            *PAPPICONMARGINRELX(), tileLogical.w);
    const double yMargin = edgeSignedMargin(anchor.y, static_cast<double>(*PAPPICONMARGINY()),
                                            *PAPPICONMARGINRELY(), tileLogical.h);
    const double x = tileLogical.x + anchor.x * std::max(0.0, tileLogical.w - SIZE) + xMargin +
        static_cast<double>(*PAPPICONOFFSETX());
    const double y = tileLogical.y + anchor.y * std::max(0.0, tileLogical.h - SIZE) + yMargin +
        static_cast<double>(*PAPPICONOFFSETY());

    return CBox{x, y, (double)SIZE, (double)SIZE}.scale(scale).round();
}

static bool previewableWindow(const PHLWINDOW& window) {
    if (!window || !window->m_isMapped || window->isHidden() || !window->m_workspace)
        return false;

    const auto SIZE = window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    return SIZE.x > 1 && SIZE.y > 1 && window->m_realSize->value().x > 1 &&
        window->m_realSize->value().y > 1;
}

static bool minimizedWorkspace(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return false;

    const auto& NAME = workspace->m_name;
    return NAME == "special:minimized" || NAME == "minimized" ||
        (workspace->m_isSpecialWorkspace && NAME.find("minimized") != std::string::npos);
}

static bool scratchpadWorkspace(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return false;

    const auto& NAME = workspace->m_name;
    return NAME == "special:NSP" || NAME == "NSP" || NAME.starts_with("scratch-hidden-");
}

static bool recoverableWindow(const PHLWINDOW& window) {
    if (!window)
        return false;

    return minimizedWorkspace(window->m_workspace) || scratchpadWorkspace(window->m_workspace);
}

CWindowOverview::CWindowOverview(const PHLMONITOR& monitor, SWindowOverviewOptions options_) :
    pMonitor(monitor), options(options_) {
    animationStartedAt       = Time::steadyNow();
    filterAnimationStartedAt = animationStartedAt;
    filterMode               = options.startInFilterMode;
    initialFocusedWindow     = Desktop::focusState()->window();
    initialFocusedWorkspace  = initialFocusedWindow && initialFocusedWindow->m_workspace ?
         initialFocusedWindow->m_workspace :
         (pMonitor ? pMonitor->m_activeWorkspace : nullptr);

    collectWindows();
    rebuildVisiblePreviews(false);

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
    selectedIndex     = hoveredIndex();
    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        info.cancelled     = true;
        lastMousePosLocal  = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
        const auto HOVERED = hoveredIndex();
        if (HOVERED >= 0)
            selectedIndex = HOVERED;
        damage();
    };

    auto onCursorSelect = [this](Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        info.cancelled = true;
        selectHoveredWindow();
        runDefaultSelected();
    };

    auto onKeyboardKey = [this](const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        if (handleKey(event))
            info.cancelled = true;
    };

    mouseMoveHook = Event::bus()->m_events.input.mouse.move.listen(
        [onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook = Event::bus()->m_events.input.touch.motion.listen(
        [onCursorMove](const ITouch::SMotionEvent&, Event::SCallbackInfo& info) {
            onCursorMove(info);
        });
    mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen(
        [onCursorSelect](const IPointer::SButtonEvent&, Event::SCallbackInfo& info) {
            onCursorSelect(info);
        });
    touchDownHook = Event::bus()->m_events.input.touch.down.listen(
        [onCursorSelect](const ITouch::SDownEvent&, Event::SCallbackInfo& info) {
            onCursorSelect(info);
        });
    keyboardHook = Event::bus()->m_events.input.keyboard.key.listen(
        [onKeyboardKey](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) {
            onKeyboardKey(event, info);
        });

    Pointer::Cursor::overrideController->setOverride("left_ptr",
                                                     Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);
    cursorOverrideSet = true;

    damage();
}

CWindowOverview::~CWindowOverview() {
    if (cursorOverrideSet)
        Pointer::Cursor::overrideController->unsetOverride(
            Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);

    stopFilterDeleteRepeat();
    if (filterDeleteRepeatTimer && g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(filterDeleteRepeatTimer);
    filterDeleteRepeatTimer.reset();

    allPreviews.clear();
    previews.clear();
    exitingPreviews.clear();
    framePreviews.clear();
}

void CWindowOverview::collectWindows() {
    allPreviews.clear();

    const auto  CURRENT_WORKSPACE = pMonitor ? pMonitor->m_activeWorkspace : nullptr;

    const auto& WINDOWS = Desktop::windowState()->windows();
    for (auto it = WINDOWS.rbegin(); it != WINDOWS.rend(); ++it) {
        const auto& window = *it;
        if (!previewableWindow(window))
            continue;

        if (!options.includeCurrentWorkspace && CURRENT_WORKSPACE &&
            window->m_workspace == CURRENT_WORKSPACE)
            continue;

        allPreviews.push_back({.window = window});
    }

    std::ranges::reverse(allPreviews);
    applyWindowOrdering(allPreviews);
    updateWorkspaceGrid();
}

void CWindowOverview::updateWorkspaceGrid() {
    int workspaceCount = 1;

    for (const auto& workspace : State::workspaceState()->workspaces()) {
        if (!workspace || workspace->m_isSpecialWorkspace || workspace->m_id <= 0)
            continue;

        workspaceCount = std::max(workspaceCount, (int)workspace->m_id);
    }

    for (const auto& preview : allPreviews) {
        if (!preview.window || !preview.window->m_workspace ||
            preview.window->m_workspace->m_isSpecialWorkspace ||
            preview.window->m_workspace->m_id <= 0)
            continue;

        workspaceCount = std::max(workspaceCount, (int)preview.window->m_workspace->m_id);
    }

    workspaceGridCount = workspaceCount;
    workspaceGridCols  = workspaceGridColsForCount(workspaceGridCount);
    workspaceGridRows  = workspaceGridRowsForCount(workspaceGridCount, workspaceGridCols);
}

void CWindowOverview::rebuildVisiblePreviews(bool animate) {
    auto OLD_PREVIEWS = previews;
    if (filterAnimating) {
        for (auto& preview : OLD_PREVIEWS)
            preview.tileLogical = filterTransitionTileLogicalBox(preview);
    }

    const auto OLD_SELECTED = selectedIndex >= 0 && selectedIndex < (int)previews.size() ?
        previews[selectedIndex].window :
        PHLWINDOW{};

    previews.clear();
    for (const auto& preview : allPreviews) {
        if (windowMatchesQuery(preview.window, filterQuery))
            previews.push_back(preview);
    }

    updateLayout();

    if (animate) {
        exitingPreviews.clear();

        for (const auto& oldPreview : OLD_PREVIEWS) {
            const bool STILL_VISIBLE = std::ranges::any_of(
                previews, [&oldPreview](const auto& p) { return p.window == oldPreview.window; });
            if (!STILL_VISIBLE) {
                auto exiting               = oldPreview;
                exiting.filterStartLogical = oldPreview.tileLogical;
                exitingPreviews.push_back(std::move(exiting));
            }
        }

        for (auto& preview : previews) {
            const auto OLD = std::ranges::find_if(
                OLD_PREVIEWS, [&preview](const auto& old) { return old.window == preview.window; });
            preview.filterStartLogical =
                OLD != OLD_PREVIEWS.end() ? OLD->tileLogical : preview.tileLogical;
        }

        filterAnimating          = true;
        filterAnimationStartedAt = Time::steadyNow();
    } else {
        exitingPreviews.clear();
        filterAnimating = false;
        for (auto& preview : previews)
            preview.filterStartLogical = preview.tileLogical;
    }

    selectedIndex = -1;
    if (OLD_SELECTED) {
        for (size_t i = 0; i < previews.size(); ++i) {
            if (previews[i].window == OLD_SELECTED) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    if (selectedIndex >= 0)
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
}

void CWindowOverview::updateLayout() {
    if (!pMonitor || previews.empty()) {
        gridCols = 1;
        return;
    }

    const double count  = static_cast<double>(previews.size());
    const double aspect = std::max(0.1, pMonitor->m_size.x / std::max(1.0, pMonitor->m_size.y));
    int          cols   = std::max(1, (int)std::ceil(std::sqrt(count * aspect)));
    int          rows   = std::max(1, (int)std::ceil(count / cols));

    while (cols > 1 && (cols - 1) * rows >= (int)previews.size())
        cols--;

    rows     = std::max(1, (int)std::ceil(count / cols));
    gridCols = cols;

    const double margin = static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN()));
    const double gap    = static_cast<double>(std::max<Config::INTEGER>(0, *PGAP()));
    const double areaW  = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH  = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW  = (areaW - gap * (cols - 1)) / cols;
    const double cellH  = (areaH - gap * (rows - 1)) / rows;

    for (size_t i = 0; i < previews.size(); ++i) {
        auto&        preview = previews[i];
        const auto   cell    = visualCellForPreviewIndex((int)i);
        const double row     = cell.first;
        const double col     = cell.second;
        const auto   winSize = preview.window->m_realSize->value();
        const double scale =
            std::min(cellW / std::max(1.0, winSize.x), cellH / std::max(1.0, winSize.y));
        const double w = std::max(1.0, winSize.x * scale);
        const double h = std::max(1.0, winSize.y * scale);
        const double x = margin + col * (cellW + gap) + (cellW - w) / 2.0;
        const double y = margin + row * (cellH + gap) + (cellH - h) / 2.0;

        preview.tileLogical = {x, y, w, h};
    }
}

int CWindowOverview::hoveredIndex() const {
    for (size_t i = 0; i < previews.size(); ++i) {
        if (previews[i].tileLogical.containsPoint(lastMousePosLocal))
            return static_cast<int>(i);
    }

    return -1;
}

void CWindowOverview::render() {
    if (closing && animationComplete()) {
        finishClose();
        return;
    }

    if (!pMonitor)
        return;

    const double SCALE      = pMonitor->m_scale;
    const auto   ANIMATION  = overviewAnimation();
    const auto   VISIBLE    = animationVisibleAmount();
    const auto   BASE_SCALE = std::clamp<double>(*PANIMATIONSCALE(), 0.01, 1.0);

    framePreviews.clear();
    framePreviews.reserve(exitingPreviews.size() + previews.size());

    const auto FILTER_PROGRESS = filterTransitionVisibleAmount();
    for (auto& preview : exitingPreviews) {
        const auto EXIT_VISIBLE = VISIBLE * (1.0 - FILTER_PROGRESS);
        if (EXIT_VISIBLE <= 0.0 || !preview.window)
            continue;

        CBox tilePx = scaleBoxFromCenter(preview.tileLogical, 0.96 + 0.04 * EXIT_VISIBLE)
                          .scale(SCALE)
                          .round();
        framePreviews.push_back({preview.window, tilePx, EXIT_VISIBLE, EXIT_VISIBLE, false});
    }

    for (size_t i = 0; i < previews.size(); ++i) {
        auto& preview = previews[i];
        if (!preview.window)
            continue;

        const auto TILE_VISIBLE = tileAnimationVisibleAmount(i);
        const auto WINDOW_ALPHA = animatedTileTextureAlpha(i, TILE_VISIBLE);
        const auto TILE_SCALE =
            animationScalesTiles(ANIMATION) ? BASE_SCALE + (1.0 - BASE_SCALE) * TILE_VISIBLE : 1.0;
        CBox tilePx = scaleBoxFromCenter(animatedTileLogicalBox(i, TILE_VISIBLE), TILE_SCALE)
                          .scale(SCALE)
                          .round();
        framePreviews.push_back(
            {preview.window, tilePx, TILE_VISIBLE, WINDOW_ALPHA, (int)i == selectedIndex});
    }

    g_pHyprRenderer->m_renderPass.add(makeUnique<CWinviewPassElement>(false));

    const auto MONITOR = pMonitor.lock();
    if (!MONITOR)
        return;
    const auto NOW = Time::steadyNow();
    for (const auto& preview : framePreviews) {
        CBox globalLogical = preview.tilePx.copy().scale(1.0 / SCALE);
        globalLogical      = globalLogical.translate(MONITOR->m_position);
        render_window_at_box(preview.window, MONITOR, NOW, globalLogical,
                             static_cast<float>(preview.windowAlpha));
    }

    g_pHyprRenderer->m_renderPass.add(makeUnique<CWinviewPassElement>(true));

    // Software cursors are queued before RENDER_LAST_MOMENT, so the overview passes
    // above would cover them; queue another cursor draw on top. No-op with hardware
    // cursors (renderSoftwareCursorsFor returns early when none are locked).
    if (g_pHyprRenderer->shouldRenderCursor())
        Pointer::mgr()->renderSoftwareCursorsFor(pMonitor.lock(), Time::steadyNow(),
                                                 g_pHyprRenderer->m_renderData.damage);

    // Windows on inactive workspaces do not necessarily damage this monitor when their
    // contents update. Keep the overview on the monitor's refresh loop so every tile is
    // genuinely live, matching the workspace overview behavior.
    damage();
}

void CWindowOverview::drawBackground() {
    if (!pMonitor)
        return;

    const auto VISIBLE    = animationVisibleAmount();
    const int  BORDER     = static_cast<int>(std::max<Config::INTEGER>(0, *PBORDERSIZE()));
    CRegion    fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    Render::GL::g_pHyprOpenGL->renderRect(
        CBox{{0, 0}, pMonitor->m_pixelSize}, multiplyAlpha(activeBackgroundColor(), VISIBLE),
        {.damage = &fullDamage, .blur = backgroundBlurEnabled(), .blurA = (float)VISIBLE});

    if (BORDER <= 0)
        return;

    for (const auto& preview : framePreviews) {
        const auto COLOR = preview.selected ? CHyprColor(*PHOVERBORDER()) : CHyprColor(*PBORDER());
        Render::GL::g_pHyprOpenGL->renderRect(preview.tilePx.copy().expand(BORDER),
                                              multiplyAlpha(COLOR, preview.visible),
                                              {.damage = &fullDamage, .round = BORDER * 2});
    }
}

void CWindowOverview::drawForeground() {
    if (!pMonitor)
        return;

    const double SCALE      = pMonitor->m_scale;
    const auto   VISIBLE    = animationVisibleAmount();
    CRegion      fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    for (const auto& preview : framePreviews) {
        const auto& tilePx      = preview.tilePx;
        const auto  tileVisible = preview.visible;

        if (*PSHOWAPPICON() != 0) {
            const int ICON_SIZE_PX = std::max(
                1,
                (int)std::round(static_cast<double>(std::max<Config::INTEGER>(1, *PAPPICONSIZE())) *
                                SCALE));
            if (const auto ICON = appIconTextureForWindow(preview.window, ICON_SIZE_PX)) {
                CBox      currentLogicalTile{tilePx.x / SCALE, tilePx.y / SCALE, tilePx.w / SCALE,
                                        tilePx.h / SCALE};
                CBox      iconBox = appIconBoxForTile(currentLogicalTile, SCALE);
                const int PADDING =
                    std::max(0,
                             (int)std::round(static_cast<double>(std::max<Config::INTEGER>(
                                                 0, *PAPPICONBACKPLATEPADDING())) *
                                             SCALE));
                if (PADDING > 0)
                    Render::GL::g_pHyprOpenGL->renderRect(
                        iconBox.copy().expand(PADDING).round(),
                        multiplyAlpha(CHyprColor(*PAPPICONBACKPLATE()), tileVisible),
                        {.damage = &fullDamage, .round = std::max(1, PADDING)});

                Render::GL::g_pHyprOpenGL->renderTexture(
                    ICON, iconBox, {.damage = &fullDamage, .a = (float)tileVisible});
            }
        }

        if (*PSHOWWINDOWTEXT() != 0) {
            const int PADDING =
                std::max(0, (int)std::round(static_cast<double>(*PWINDOWTEXTPADDING()) * SCALE));
            const int FONT_SIZE =
                std::max(1, (int)std::round(static_cast<double>(*PWINDOWTEXTSIZE()) * SCALE));
            const int  MAX_WIDTH = std::max(1, (int)std::round(tilePx.w - PADDING * 2));
            const auto TITLE     = windowTitle(preview.window);
            const auto CLASS     = windowClass(preview.window);
            std::vector<std::string> lines;
            if (!TITLE.empty())
                lines.push_back(TITLE);
            if (!CLASS.empty() && CLASS != TITLE)
                lines.push_back(CLASS);

            if (!lines.empty()) {
                const auto TEXT = textTextureForLines(lines, FONT_SIZE, MAX_WIDTH,
                                                      CHyprColor(*PWINDOWTEXTCOLOR()),
                                                      configStringOr(PWINDOWTEXTFONT(), "Sans"));
                if (TEXT.texture) {
                    const auto WIDTH  = std::min<double>(TEXT.size.x, MAX_WIDTH);
                    const auto HEIGHT = TEXT.size.y;
                    CBox       labelBox{
                        tilePx.x + PADDING,
                        std::max(tilePx.y + PADDING, tilePx.y + tilePx.h - HEIGHT - PADDING),
                        WIDTH,
                        HEIGHT,
                    };

                    Render::GL::g_pHyprOpenGL->renderRect(
                        labelBox.copy().expand(PADDING).round(),
                        multiplyAlpha(CHyprColor(*PWINDOWTEXTBACKPLATE()), tileVisible),
                        {.damage = &fullDamage, .round = std::max(1, PADDING)});
                    Render::GL::g_pHyprOpenGL->renderTexture(
                        TEXT.texture, labelBox, {.damage = &fullDamage, .a = (float)tileVisible});
                }
            }
        }
    }

    if (filterMode || !filterQuery.empty()) {
        const int PADDING = std::max(4, (int)std::round(8.0 * SCALE));
        const int FONT_SIZE =
            std::max(1, (int)std::round(static_cast<double>(*PWINDOWTEXTSIZE() + 2) * SCALE));
        const int MAX_WIDTH = std::max(1, (int)std::round(pMonitor->m_pixelSize.x * 0.72));
        std::vector<std::string> lines = {
            "Filter: " + filterQuery + (filterMode ? "_" : ""),
        };
        if (previews.empty() && !filterQuery.empty())
            lines.push_back("No matches");

        const auto TEXT =
            textTextureForLines(lines, FONT_SIZE, MAX_WIDTH, CHyprColor(*PWINDOWTEXTCOLOR()),
                                configStringOr(PWINDOWTEXTFONT(), "Sans"));
        if (TEXT.texture) {
            CBox promptBox{
                std::round((pMonitor->m_pixelSize.x - TEXT.size.x) / 2.0),
                std::round(static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN())) * SCALE /
                           2.0),
                TEXT.size.x,
                TEXT.size.y,
            };
            Render::GL::g_pHyprOpenGL->renderRect(
                promptBox.copy().expand(PADDING).round(),
                multiplyAlpha(CHyprColor(*PWINDOWTEXTBACKPLATE()), VISIBLE),
                {.damage = &fullDamage, .round = std::max(1, PADDING)});
            Render::GL::g_pHyprOpenGL->renderTexture(TEXT.texture, promptBox,
                                                     {.damage = &fullDamage, .a = (float)VISIBLE});
        }
    }

    if (filterAnimating && filterTransitionVisibleAmount() >= 1.0) {
        filterAnimating = false;
        exitingPreviews.clear();
    }
}

void CWindowOverview::damage() {
    if (pMonitor)
        g_pHyprRenderer->damageMonitor(pMonitor.lock());
}

void CWindowOverview::selectHoveredWindow() {
    selectedIndex = hoveredIndex();
}

std::pair<int, int> CWindowOverview::visualCellForPreviewIndex(int index) const {
    const int cols     = std::max(1, gridCols);
    const int row      = index / cols;
    const int colInRow = index % cols;
    const int col      = row % 2 == 0 ? colInRow : cols - 1 - colInRow;

    return {row, col};
}

int CWindowOverview::previewIndexForVisualCell(int row, int col) const {
    const int cols = std::max(1, gridCols);

    if (row < 0 || col < 0 || col >= cols)
        return -1;

    const int colInRow = row % 2 == 0 ? col : cols - 1 - col;
    const int index    = row * cols + colInRow;

    if (index < 0 || index >= (int)previews.size())
        return -1;

    return index;
}

void CWindowOverview::moveSelection(int dx, int dy) {
    if (previews.empty())
        return;

    if (selectedIndex < 0 || selectedIndex >= (int)previews.size())
        selectedIndex = 0;

    const int  cols   = std::max(1, gridCols);
    const auto cell   = visualCellForPreviewIndex(selectedIndex);
    const int  row    = cell.first;
    const int  col    = cell.second;
    const int  newCol = std::clamp(col + dx, 0, cols - 1);
    const int  maxRow = ((int)previews.size() - 1) / cols;
    const int  newRow = std::clamp(row + dy, 0, maxRow);
    int        next   = previewIndexForVisualCell(newRow, newCol);

    if (next < 0)
        next = static_cast<int>(previews.size()) - 1;

    if (next != selectedIndex) {
        selectedIndex     = next;
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
        damage();
    }
}

void CWindowOverview::runSelected(bool bring, bool replaceInitial) {
    close(true, bring, replaceInitial);
}

void CWindowOverview::runDefaultSelected() {
    switch (options.defaultAction) {
        case EWinviewDefaultAction::BRING_REPLACE: runSelected(true, true); return;
        case EWinviewDefaultAction::BRING: runSelected(true); return;
        case EWinviewDefaultAction::SELECT: runSelected(false); return;
    }
}

double CWindowOverview::animationVisibleAmount() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION =
        animationDurationMs(closing) + (closing ? maxTileAnimationDelayMs() : 0.0);
    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationVisibleAmount(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION = animationDurationMs(closing);
    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count() -
        tileAnimationDelayMs(index);

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationDelayMs(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    const size_t ORDER_INDEX =
        closing ? previews.size() - 1 - std::min(index, previews.size() - 1) : index;
    return std::min<double>(static_cast<double>(ORDER_INDEX) * static_cast<double>(STAGGER_MS),
                            static_cast<double>(MAX_STAGGER_MS));
}

double CWindowOverview::maxTileAnimationDelayMs() const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    return std::min<double>(static_cast<double>(previews.size() - 1) *
                                static_cast<double>(STAGGER_MS),
                            static_cast<double>(MAX_STAGGER_MS));
}

int CWindowOverview::workspacePanelIndexForWorkspace(const PHLWORKSPACE& workspace) const {
    if (workspace && !workspace->m_isSpecialWorkspace && workspace->m_id > 0)
        return std::clamp((int)workspace->m_id - 1, 0, std::max(0, workspaceGridCount - 1));

    return std::clamp(workspaceGridCount / 2, 0, std::max(0, workspaceGridCount - 1));
}

CBox CWindowOverview::workspacePanelCellLogical(int index) const {
    if (!pMonitor)
        return {};

    const int count = std::max(1, workspaceGridCount);
    const int cols  = std::max(1, workspaceGridCols);
    const int rows  = std::max(1, workspaceGridRows);

    index = std::clamp(index, 0, count - 1);

    const double margin = static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN()));
    const double gap =
        static_cast<double>(std::max<Config::INTEGER>(0, *PANIMATIONWORKSPACEZOOMGAP()));
    const double areaW = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW = std::max(1.0, (areaW - gap * (cols - 1)) / cols);
    const double cellH = std::max(1.0, (areaH - gap * (rows - 1)) / rows);
    const int    col   = index % cols;
    const int    row   = index / cols;

    return {margin + col * (cellW + gap), margin + row * (cellH + gap), cellW, cellH};
}

CBox CWindowOverview::workspacePanelBoxForPreview(const SWindowPreview& preview) const {
    if (!preview.window || !pMonitor)
        return {};

    const auto SOURCE_MONITOR =
        preview.window->m_monitor.lock() ? preview.window->m_monitor.lock() : pMonitor.lock();
    if (!SOURCE_MONITOR)
        return preview.tileLogical;

    const auto panelIndex = workspacePanelIndexForWorkspace(preview.window->m_workspace);
    const auto CELL       = workspacePanelCellLogical(panelIndex);
    const auto WIN_POS    = preview.window->m_realPosition->value();
    const auto WIN_SIZE   = preview.window->m_realSize->value();
    const auto MONITOR_W  = std::max(1.0, SOURCE_MONITOR->m_size.x);
    const auto MONITOR_H  = std::max(1.0, SOURCE_MONITOR->m_size.y);
    const auto LOCAL_X    = (WIN_POS.x - SOURCE_MONITOR->m_position.x) / MONITOR_W;
    const auto LOCAL_Y    = (WIN_POS.y - SOURCE_MONITOR->m_position.y) / MONITOR_H;
    const auto LOCAL_W    = WIN_SIZE.x / MONITOR_W;
    const auto LOCAL_H    = WIN_SIZE.y / MONITOR_H;

    return {
        CELL.x + LOCAL_X * CELL.w,
        CELL.y + LOCAL_Y * CELL.h,
        std::max(1.0, LOCAL_W * CELL.w),
        std::max(1.0, LOCAL_H * CELL.h),
    };
}

CBox CWindowOverview::workspaceZoomCameraBoxForPanelBox(const CBox& panelBox,
                                                        double      cameraProgress) const {
    if (!pMonitor)
        return panelBox;

    const auto START_WORKSPACE = initialFocusedWorkspace ?
        initialFocusedWorkspace :
        (pMonitor ? pMonitor->m_activeWorkspace : nullptr);
    const auto START_CELL =
        workspacePanelCellLogical(workspacePanelIndexForWorkspace(START_WORKSPACE));
    const auto   PROGRESS = std::clamp(cameraProgress, 0.0, 1.0);
    const auto   VIEWPORT = CBox{0, 0, pMonitor->m_size.x, pMonitor->m_size.y};

    const double startScaleX = VIEWPORT.w / std::max(1.0, START_CELL.w);
    const double startScaleY = VIEWPORT.h / std::max(1.0, START_CELL.h);
    const double startX      = VIEWPORT.x - START_CELL.x * startScaleX;
    const double startY      = VIEWPORT.y - START_CELL.y * startScaleY;
    const double scaleX      = lerpDouble(startScaleX, 1.0, PROGRESS);
    const double scaleY      = lerpDouble(startScaleY, 1.0, PROGRESS);
    const double x           = lerpDouble(startX, 0.0, PROGRESS);
    const double y           = lerpDouble(startY, 0.0, PROGRESS);

    return {
        panelBox.x * scaleX + x,
        panelBox.y * scaleY + y,
        panelBox.w * scaleX,
        panelBox.h * scaleY,
    };
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
CBox CWindowOverview::animatedTileLogicalBox(size_t index, double progress) const {
    if (index >= previews.size())
        return {};

    if (!animationUsesWorkspaceZoom(overviewAnimation()))
        return filterTransitionTileLogicalBox(previews[index]);

    const auto& PREVIEW = previews[index];
    const auto  SPLIT   = workspaceZoomStageRatio();
    const auto  PANEL   = workspacePanelBoxForPreview(PREVIEW);
    const auto  FINAL   = filterTransitionTileLogicalBox(PREVIEW);
    const auto  RAW     = rawProgressForVisibleAmount(progress, closing);

    if (closing) {
        const auto GATHER_DURATION = 1.0 - SPLIT;
        if (RAW <= GATHER_DURATION)
            return lerpBox(FINAL, PANEL, easeInCubic(RAW / GATHER_DURATION));

        return workspaceZoomCameraBoxForPanelBox(
            PANEL, 1.0 - easeInCubic((RAW - GATHER_DURATION) / SPLIT));
    }

    if (RAW <= SPLIT)
        return workspaceZoomCameraBoxForPanelBox(PANEL, easeOutCubic(RAW / SPLIT));

    return lerpBox(PANEL, FINAL, easeOutCubic((RAW - SPLIT) / (1.0 - SPLIT)));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double CWindowOverview::animatedTileTextureAlpha(size_t index, double progress) const {
    if (index >= previews.size())
        return 0.0;

    if (!animationUsesWorkspaceZoom(overviewAnimation()))
        return progress;

    return 1.0;
}

CBox CWindowOverview::filterTransitionTileLogicalBox(const SWindowPreview& preview) const {
    if (closing || !filterAnimating)
        return preview.tileLogical;

    return lerpBox(preview.filterStartLogical, preview.tileLogical,
                   filterTransitionVisibleAmount());
}

double CWindowOverview::filterTransitionVisibleAmount() const {
    if (!filterAnimating)
        return 1.0;

    const double DURATION =
        static_cast<double>(std::max<Config::INTEGER>(0, *PFILTERANIMATIONMS()));
    if (DURATION <= 0.0)
        return 1.0;

    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - filterAnimationStartedAt)
            .count();
    return easeOutCubic(ELAPSED / DURATION);
}

bool CWindowOverview::animationComplete() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return true;

    const auto DURATION = animationDurationMs(closing);
    if (DURATION <= 0.0)
        return true;

    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();
    const auto TOTAL_TIME_MS = DURATION + maxTileAnimationDelayMs();
    return ELAPSED >= TOTAL_TIME_MS;
}

bool CWindowOverview::isAnimating() const {
    return !animationComplete() || (filterAnimating && filterTransitionVisibleAmount() < 1.0);
}

bool CWindowOverview::backgroundBlurEnabled() const {
    return *PBACKGROUNDBLUR() != 0;
}

bool CWindowOverview::occludesScene() const {
    return !isAnimating() && activeBackgroundColor().a >= 1.0;
}

bool CWindowOverview::deleteFilterCharacter() {
    if (filterQuery.empty())
        return false;

    auto query = filterQuery;
    query.pop_back();
    setFilterQuery(std::move(query));
    return true;
}

void CWindowOverview::startFilterDeleteRepeat() {
    filterDeleteHeld = true;

    if (!filterDeleteRepeatTimer) {
        filterDeleteRepeatTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [this](const SP<CEventLoopTimer>& self, void*) {
                if (!filterDeleteHeld || closing)
                    return;

                deleteFilterCharacter();
                self->updateTimeout(std::chrono::milliseconds(35));
            },
            nullptr);

        if (g_pEventLoopManager)
            g_pEventLoopManager->addTimer(filterDeleteRepeatTimer);
    }

    filterDeleteRepeatTimer->updateTimeout(std::chrono::milliseconds(300));
}

void CWindowOverview::stopFilterDeleteRepeat() {
    filterDeleteHeld = false;
    if (filterDeleteRepeatTimer)
        filterDeleteRepeatTimer->updateTimeout(std::nullopt);
}

void CWindowOverview::toggleFilterMode() {
    filterMode = !filterMode;
    damage();
}

void CWindowOverview::setFilterQuery(std::string query, bool animate) {
    if (filterQuery == query)
        return;

    filterQuery = std::move(query);
    rebuildVisiblePreviews(animate);
    damage();
}

void CWindowOverview::focusWindow(const PHLWINDOW& window, bool bring, bool replaceInitial) {
    if (!window || !window->m_workspace)
        return;

    const auto FOCUSSTATE                  = Desktop::focusState();
    const auto MONITOR                     = FOCUSSTATE->monitor();
    const auto TARGET_WORKSPACE            = replaceInitial && initialFocusedWorkspace ?
                   initialFocusedWorkspace :
                   (MONITOR ? MONITOR->m_activeWorkspace : nullptr);
    const auto SELECTED_ORIGINAL_WORKSPACE = window->m_workspace;
    const auto SELECTED_TARGET             = window->layoutTarget();
    const auto INITIAL_TARGET =
        initialFocusedWindow ? initialFocusedWindow->layoutTarget() : SP<Layout::ITarget>{};
    const bool CAN_REPLACE_INITIAL = replaceInitial && g_layoutManager && initialFocusedWindow &&
        initialFocusedWindow != window && initialFocusedWindow->m_isMapped &&
        initialFocusedWindow->m_workspace && SELECTED_ORIGINAL_WORKSPACE && SELECTED_TARGET &&
        INITIAL_TARGET && !Fullscreen::controller()->isFullscreen(window) &&
        !Fullscreen::controller()->isFullscreen(initialFocusedWindow);

    if (CAN_REPLACE_INITIAL) {
        g_layoutManager->switchTargets(SELECTED_TARGET, INITIAL_TARGET, true);
    } else if (replaceInitial && initialFocusedWindow && initialFocusedWindow != window &&
               initialFocusedWindow->m_isMapped && initialFocusedWindow->m_workspace &&
               SELECTED_ORIGINAL_WORKSPACE &&
               initialFocusedWindow->m_workspace != SELECTED_ORIGINAL_WORKSPACE) {
        Desktop::globalWindowController()->moveWindowToWorkspace(initialFocusedWindow,
                                                                 SELECTED_ORIGINAL_WORKSPACE);
    }

    if ((bring || replaceInitial) && TARGET_WORKSPACE && window->m_workspace != TARGET_WORKSPACE) {
        Desktop::globalWindowController()->moveWindowToWorkspace(window, TARGET_WORKSPACE);
    }

    if (MONITOR && MONITOR->m_activeWorkspace != window->m_workspace)
        MONITOR->changeWorkspace(window->m_workspace);

    FOCUSSTATE->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
    Pointer::pointerController()->warpTo(window->middle());
}

void CWindowOverview::finishClose() {
    const auto MONITOR = pMonitor.lock();

    g_pWindowOverview.reset();

    if (MONITOR)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

void CWindowOverview::close(bool focusSelection, bool bringSelection,
                            bool replaceInitialSelection) {
    if (closing)
        return;

    stopFilterDeleteRepeat();

    if (cursorOverrideSet) {
        Pointer::Cursor::overrideController->unsetOverride(
            Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);
        cursorOverrideSet = false;
    }

    closing            = true;
    animationStartedAt = Time::steadyNow();

    PHLWINDOW selectedWindow;
    if (focusSelection && selectedIndex >= 0 && selectedIndex < (int)previews.size())
        selectedWindow = previews[selectedIndex].window;

    if (selectedWindow)
        focusWindow(selectedWindow, bringSelection || recoverableWindow(selectedWindow),
                    replaceInitialSelection);

    damage();

    if (animationComplete())
        finishClose();
}

bool windowOverviewActive() {
    return g_pWindowOverview != nullptr;
}

void closeWindowOverviewImmediately() {
    const auto MONITOR = g_pWindowOverview ? g_pWindowOverview->pMonitor.lock() : nullptr;
    g_pWindowOverview.reset();

    if (g_pHyprRenderer)
        g_pHyprRenderer->m_renderPass.removeAllOfType("CWinviewPassElement");
    if (MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}
