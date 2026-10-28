#include "render.hpp"

#include <utility>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/animation/WorkspaceAnimationController.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "globals.hpp"
#include "src/helpers/time/Time.hpp"
#include "types.hpp"

using namespace Render;

namespace {
    class COverviewRenderContextPassElement : public IPassElement {
      public:
        COverviewRenderContextPassElement(bool active, std::optional<CBox> clip, float alpha) :
            active(active), clip(std::move(clip)), alpha(alpha) {}

        std::vector<UP<IPassElement>> draw(CRenderContext&) override {
            overview_scaled_render = active;
            overview_scaled_clip   = active ? clip : std::nullopt;
            overview_scaled_alpha  = active ? alpha : 1.F;
            return {};
        }

        bool needsLiveBlur(CRenderContext&) override {
            return false;
        }

        bool needsPrecomputeBlur(CRenderContext&) override {
            return false;
        }

        bool undiscardable(CRenderContext&) override {
            return true;
        }

        bool disableSimplification(CRenderContext&) override {
            return true;
        }

        const char* passName() override {
            return "COverviewRenderContextPassElement";
        }

        ePassElementType type() override {
            return EK_CUSTOM;
        }

      private:
        bool                active = false;
        std::optional<CBox> clip;
        float               alpha = 1.F;
    };

    void addOverviewRenderContext(CRenderContext& ctx, bool active,
                                  std::optional<CBox> clip = std::nullopt, float alpha = 1.F) {
        IHyprRenderer::addPassElement(
            ctx, makeUnique<COverviewRenderContextPassElement>(active, std::move(clip), alpha));
    }
}

// Note: box is relative to (0, 0), not monitor
void render_window_at_box(CRenderContext& ctx, PHLWINDOW window, PHLMONITOR monitor,
                          const Time::steady_tp& time, CBox box, float alpha) {
    if (!window || !window->mapped() || !monitor || box.empty())
        return;

    const auto window_size = window->sizeAnimation()->value();
    if (window_size.x <= 0 || window_size.y <= 0)
        return;

    box.x -= monitor->m_position.x;
    box.y -= monitor->m_position.y;

    // ignorePosition draws the window at the monitor origin, sidestepping the workspace
    // render offset that stays non-zero for slid-out workspaces.
    const auto     presentation = window->presentation().renderPresentation();
    const float    scale        = box.w / window_size.x;
    const Vector2D transform = (box.pos() / scale - presentation.floatingOffset) * monitor->m_scale;

    const CBox     clip = CBox{box.pos(), box.size()}.scale(monitor->m_scale).round();
    addOverviewRenderContext(ctx, true, clip, alpha);

    SRenderModifData data{};
    data.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, transform});
    data.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale});
    IHyprRenderer::addPassElement(
        ctx, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{data}));

    ((render_window_t)render_window)(g_pHyprRenderer.get(), ctx, window, monitor, presentation,
                                     time, true, RENDER_PASS_MAIN, true, true);

    IHyprRenderer::addPassElement(ctx,
                                  makeUnique<CRendererHintsPassElement>(
                                      CRendererHintsPassElement::SData{SRenderModifData{}}));
    addOverviewRenderContext(ctx, false);
}

void render_workspace_at_box(CRenderContext& ctx, PHLMONITOR monitor, PHLWORKSPACE workspace,
                             const Time::steady_tp& time, CBox box) {
    if (!monitor)
        return;
    // A stale slot can name a workspace another monitor now owns; swapping it in here
    // would rewrite that monitor's live workspace state.
    if (workspace != nullptr && workspace->m_monitor != monitor)
        workspace = nullptr;

    // renderWorkspace derives scale = box.w / transformedSize and translate = box.pos, and
    // applies the translate BEFORE the scale, so pre-divide the position by the scale.
    const float ws_scale   = box.w / monitor->m_transformedSize.x;
    CBox        render_box = {box.pos() / ws_scale, box.size()};

    // Hyprland only fully renders the monitor's active workspace, so make this one
    // active+visible while we render it. The caller restores the original active ws.
    if (workspace != nullptr) {
        monitor->m_activeWorkspace = workspace;
        Animation::Workspace::startAnimation(workspace, Animation::Workspace::ANIMATION_TYPE_IN,
                                             false, true);
        workspace->setVisible(true);
    }

    addOverviewRenderContext(ctx, true);
    ((render_workspace_t)(render_workspace_hook->m_original))(g_pHyprRenderer.get(), ctx, monitor,
                                                              workspace, time, render_box);
    addOverviewRenderContext(ctx, false);

    if (workspace != nullptr) {
        Animation::Workspace::startAnimation(workspace, Animation::Workspace::ANIMATION_TYPE_OUT,
                                             false, true);
        workspace->setVisible(false);
    }
}
