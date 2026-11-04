#include "indicator.hpp"

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>

#define private   public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/X11Backend.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>
#undef private
#undef protected

#include <cairo/cairo.h>
#include <poll.h>
#include <wayland-server-core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <format>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../config_helpers.hpp"
#include "../globals.hpp"
#include "../lua_args.hpp"
#include "../settings.hpp"
#include "../strings.hpp"
#include "lua_rules.hpp"
#include "media_sessions.hpp"
#include "pipewire_streams.hpp"
#include "proc.hpp"
#include "reload_staged.hpp"

namespace {
    Audio::CReloadStaged<Audio::SRules> g_rules;

    // Drives an evidence source from the compositor's wl_event_loop.
    class CLoopBinding {
      public:
        explicit CLoopBinding(Audio::IEvidenceSource& source_) : source(source_) {
            timer = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, onTimer, this);
            sync();
        }

        ~CLoopBinding() {
            recovery.reset();
            removeFd();
            if (timer)
                wl_event_source_remove(timer);
        }

        CLoopBinding(const CLoopBinding&)            = delete;
        CLoopBinding& operator=(const CLoopBinding&) = delete;

      private:
        static int onFd(int, uint32_t, void* data) {
            static_cast<CLoopBinding*>(data)->dispatch();
            return 0;
        }

        static int onTimer(void* data) {
            static_cast<CLoopBinding*>(data)->dispatch();
            return 0;
        }

        void dispatch() {
            if (recovery)
                return;
            source.dispatch();
            if (!source.failed()) {
                sync();
                return;
            }

            // reset() closes the fd and may close a bus whose callback is still on the stack, so
            // it runs from an idle callback after the fd source is gone.
            if (fdSource)
                wl_event_source_fd_update(fdSource, 0);
            recovery = g_pEventLoopManager->doLaterLock([this] {
                removeFd();
                source.reset();
                sync();
                recovery.reset();
            });
        }

        // The fd only changes through reset(), which always follows removeFd().
        void sync() {
            const int   FD     = source.fd();
            const short EVENTS = source.events();
            uint32_t    mask   = 0;
            if (EVENTS & POLLIN)
                mask |= WL_EVENT_READABLE;
            if (EVENTS & POLLOUT)
                mask |= WL_EVENT_WRITABLE;

            if (FD >= 0 && !fdSource)
                fdSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, FD, mask, onFd, this);
            else if (FD < 0)
                removeFd();
            else
                wl_event_source_fd_update(fdSource, mask);

            int timeoutMs = 0;
            if (const auto DEADLINE = source.deadline()) {
                const auto LEFT =
                    std::chrono::ceil<std::chrono::milliseconds>(*DEADLINE - Audio::Clock::now());
                timeoutMs = static_cast<int>(std::clamp<int64_t>(LEFT.count(), 1, INT32_MAX));
            }
            if (timer)
                wl_event_source_timer_update(timer, timeoutMs);
        }

        void removeFd() {
            if (fdSource)
                wl_event_source_remove(fdSource);
            fdSource = nullptr;
        }

        Audio::IEvidenceSource&   source;
        wl_event_source*          fdSource = nullptr;
        wl_event_source*          timer    = nullptr;
        UP<SEventLoopDoLaterLock> recovery;
    };

    void logAudio(std::string_view message) {
        LOG(Log::WARN, "[hyprwinview] {}", message);
    }

    uint64_t windowKey(const PHLWINDOW& window) {
        return reinterpret_cast<uintptr_t>(window.get());
    }

    class CAudioIndicator {
      public:
        CAudioIndicator() {
            auto& events = Event::bus()->m_events;
            auto  bump   = [this] { ++windowChanges; };
            windowListeners.push_back(events.window.open.listen(bump));
            windowListeners.push_back(events.window.close.listen(bump));
            windowListeners.push_back(events.window.title.listen(bump));
            windowListeners.push_back(events.window.class_.listen(bump));
        }

        Audio::eIndicator indicatorFor(const PHLWINDOW& window) {
            const std::array KEY = {pipewire.generation(), mpris.generation(), windowChanges,
                                    g_rules.generation()};
            if (KEY != cachedFor) {
                cachedFor = KEY;
                refresh();
            }
            const auto IT = indicators.find(windowKey(window));
            return IT == indicators.end() ? Audio::eIndicator::None : IT->second;
        }

      private:
        void refresh() {
            indicators.clear();
            if (pipewire.streams().empty())
                return;

            Audio::SSnapshot snapshot{.streams  = pipewire.streams(),
                                      .sessions = mpris.sessions(),
                                      .windows  = {},
                                      .parentOf = Audio::parentPid};
            for (const auto& window : Desktop::windowState()->windows()) {
                if (!window || !window->mapped())
                    continue;
                const auto&             backend = window->backend();
                std::optional<uint32_t> xid;
                if (backend.type() == Desktop::View::eBackendType::WINDOW_BACKEND_X11) {
                    const auto& X11 = static_cast<const Desktop::View::CX11Backend&>(backend);
                    if (const auto SURFACE = X11.m_xwaylandSurface.lock())
                        xid = SURFACE->m_xID;
                }
                const auto& META = window->metadata();
                snapshot.windows.push_back({.key          = windowKey(window),
                                            .pid          = backend.pid(),
                                            .cls          = META.appID(),
                                            .initialClass = META.initialAppID(),
                                            .title        = META.title(),
                                            .x11Xid       = xid});
            }
            indicators = Audio::indicatorsByWindow(Audio::attribute(snapshot, g_rules.active()));
        }

        // Bindings are declared after the sources they drive, so they are destroyed first and
        // remove their event sources before the child is killed and the bus closed.
        Audio::CPipeWireStreams                         pipewire{logAudio};
        Audio::CMediaSessions                           mpris{logAudio};
        CLoopBinding                                    pipewireLoop{pipewire};
        CLoopBinding                                    mprisLoop{mpris};
        std::vector<CHyprSignalListener>                windowListeners;
        uint64_t                                        windowChanges = 0;
        std::optional<std::array<uint64_t, 4>>          cachedFor;
        std::unordered_map<uint64_t, Audio::eIndicator> indicators;
    };

    std::unique_ptr<CAudioIndicator>                                  g_indicator;
    std::map<std::pair<int, Audio::eIndicator>, SP<Render::ITexture>> g_badges;

    // A speaker with sound waves on a 24-unit grid. App badges are dimmer with one wave, since
    // the sound may come from another window of the same app.
    SP<Render::ITexture> drawBadge(Audio::eIndicator indicator, int sizePx) {
        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sizePx, sizePx);
        auto* cr      = cairo_create(surface);
        cairo_scale(cr, sizePx / 24.0, sizePx / 24.0);

        const bool WINDOW = indicator == Audio::eIndicator::Window;
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, WINDOW ? 1.0 : 0.55);
        cairo_move_to(cr, 3.0, 9.0);
        cairo_line_to(cr, 7.0, 9.0);
        cairo_line_to(cr, 12.0, 4.5);
        cairo_line_to(cr, 12.0, 19.5);
        cairo_line_to(cr, 7.0, 15.0);
        cairo_line_to(cr, 3.0, 15.0);
        cairo_close_path(cr);
        cairo_fill(cr);

        cairo_set_line_width(cr, 2.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        constexpr double SPREAD = std::numbers::pi / 4.0;
        for (const double RADIUS : {4.5, 8.0}) {
            if (!WINDOW && RADIUS > 5.0)
                break;
            cairo_new_sub_path(cr);
            cairo_arc(cr, 12.5, 12.0, RADIUS, -SPREAD, SPREAD);
            cairo_stroke(cr);
        }

        cairo_destroy(cr);
        cairo_surface_flush(surface);
        auto texture = g_pHyprRenderer->createTexture(surface);
        cairo_surface_destroy(surface);
        return texture;
    }

    CHyprSignalListener g_preReload;
    CHyprSignalListener g_reloaded;
    CHyprSignalListener g_propsRefreshed;

    void                syncIndicator() {
        if (*WinviewConfig::PSHOWAUDIOICON() == 0) {
            g_indicator.reset();
            g_badges.clear();
        } else if (!g_indicator)
            g_indicator = std::make_unique<CAudioIndicator>();
    }

    int luaConfigureAudio(lua_State* L) {
        return Winview::luaBoundary(L, [](lua_State* L) -> std::expected<void, std::string> {
            auto rules = Audio::readRules(L, 1);
            if (!rules)
                return std::unexpected(
                    std::format("hyprwinview.configure_audio: {}", rules.error()));
            g_rules.configure(std::move(*rules));
            return {};
        });
    }

    int scaledPx(Config::INTEGER logical, double scale) {
        return static_cast<int>(
            std::round(static_cast<double>(std::max<Config::INTEGER>(0, logical)) * scale));
    }

    // The badge sits in a corner of the tile, inset by `marginPx`.
    CBox badgeBox(const CBox& tilePx, int sizePx, const Vector2D& marginPx) {
        const auto POSITION = Winview::lowercase(
            Winview::configStringOr(WinviewConfig::PAUDIOICONPOSITION(), "top right"));
        const double X = POSITION.contains("left") ? tilePx.x + marginPx.x :
                                                     tilePx.x + tilePx.w - sizePx - marginPx.x;
        const double Y = POSITION.contains("bottom") ? tilePx.y + tilePx.h - sizePx - marginPx.y :
                                                       tilePx.y + marginPx.y;
        return CBox{X, Y, static_cast<double>(sizePx), static_cast<double>(sizePx)}.round();
    }
}

void Winview::initAudioIndicator() {
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprwinview", "configure_audio", luaConfigureAudio);
    auto& events = Event::bus()->m_events.config;
    g_preReload  = events.preReload.listen([] { g_rules.beginReload(); });
    // Every reload path, including a syntax error that keeps the old config, ends here.
    g_reloaded = events.reloaded.listen([] {
        g_rules.endReload(!Config::mgr()->getErrors().empty());
        syncIndicator();
    });
    // Runtime hl.config() changes skip the reload events and only refresh properties.
    g_propsRefreshed = events.props_refreshed.listen([](bool) { syncIndicator(); });
}

void Winview::shutdownAudioIndicator() {
    g_preReload.reset();
    g_reloaded.reset();
    g_propsRefreshed.reset();
    g_indicator.reset();
    g_badges.clear();
}

void Winview::drawAudioBadge(Render::CRenderContext& ctx, const PHLMONITOR& monitor,
                             const PHLWINDOW& window, const CBox& tilePx, double alpha,
                             CRegion& damage) {
    if (!g_indicator || !monitor || !window)
        return;
    const auto INDICATOR = g_indicator->indicatorFor(window);
    if (INDICATOR == Audio::eIndicator::None)
        return;

    const double SCALE   = monitor->m_scale;
    const int    SIZE    = std::max(1, scaledPx(*WinviewConfig::PAUDIOICONSIZE(), SCALE));
    auto&        texture = g_badges[{SIZE, INDICATOR}];
    if (!texture)
        texture = drawBadge(INDICATOR, SIZE);

    const Vector2D MARGIN{static_cast<double>(*WinviewConfig::PAPPICONMARGINX()),
                          static_cast<double>(*WinviewConfig::PAPPICONMARGINY())};
    const CBox     BOX     = badgeBox(tilePx, SIZE, MARGIN * SCALE);
    const int      PADDING = scaledPx(*WinviewConfig::PAPPICONBACKPLATEPADDING(), SCALE);
    if (PADDING > 0) {
        CHyprColor backplate{static_cast<uint64_t>(*WinviewConfig::PAPPICONBACKPLATE())};
        backplate.a *= alpha;
        Render::GL::g_pHyprOpenGL->renderRect(ctx, BOX.copy().expand(PADDING).round(), backplate,
                                              {.damage = &damage, .round = PADDING});
    }
    Render::GL::g_pHyprOpenGL->renderTexture(ctx, texture, BOX,
                                             {.damage = &damage, .a = static_cast<float>(alpha)});
}
