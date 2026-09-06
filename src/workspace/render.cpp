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

        std::vector<UP<IPassElement>> draw() override {
            overview_scaled_render = active;
            overview_scaled_clip   = active ? clip : std::nullopt;
            overview_scaled_alpha  = active ? alpha : 1.F;
            return {};
        }

        bool needsLiveBlur() override {
            return false;
        }

        bool needsPrecomputeBlur() override {
            return false;
        }

        bool undiscardable() override {
            return true;
        }

        bool disableSimplification() override {
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

    void addOverviewRenderContext(bool active, std::optional<CBox> clip = std::nullopt,
                                  float alpha = 1.F) {
        g_pHyprRenderer->m_renderPass.add(
            makeUnique<COverviewRenderContextPassElement>(active, std::move(clip), alpha));
    }
}

// Note: box is relative to (0, 0), not monitor
void render_window_at_box(PHLWINDOW window, PHLMONITOR monitor, const Time::steady_tp& time,
                          CBox box, float alpha) {
    if (!window || !monitor)
        return;

    box.x -= monitor->m_position.x;
    box.y -= monitor->m_position.y;

    const float    scale = box.w / window->sizeAnimation()->value().x;
    const Vector2D transform =
        (monitor->m_position - window->positionAnimation()->value() + box.pos() / scale) *
        monitor->m_scale;

    const CBox clip = CBox{box.pos(), box.size()}.scale(monitor->m_scale).round();
    addOverviewRenderContext(true, clip, alpha);

    SRenderModifData data{};
    data.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, transform});
    data.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale});
    g_pHyprRenderer->m_renderPass.add(
        makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{data}));

    ((render_window_t)render_window)(g_pHyprRenderer.get(), window, monitor, time, true,
                                     RENDER_PASS_MAIN, false, true);

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(
        CRendererHintsPassElement::SData{SRenderModifData{}}));
    addOverviewRenderContext(false);
}

void render_workspace_at_box(PHLMONITOR monitor, PHLWORKSPACE workspace,
                             const Time::steady_tp& time, CBox box) {
    if (!monitor)
        return;

    // renderWorkspace derives scale = box.w / pixelSize and translate = box.pos, and
    // applies the translate BEFORE the scale, so pre-divide the position by the scale.
    // Geometry is in pixel (untransformed) space, so swap w/h for rotated monitors.
    const float ws_scale   = box.w / monitor->m_transformedSize.x;
    CBox        render_box = {box.pos() / ws_scale, box.size()};
    if (monitor->m_transform % 2 == 1)
        std::swap(render_box.w, render_box.h);

    // Hyprland only fully renders the monitor's active workspace, so make this one
    // active+visible while we render it. The caller restores the original active ws.
    if (workspace != nullptr) {
        monitor->m_activeWorkspace = workspace;
        Animation::Workspace::startAnimation(workspace, Animation::Workspace::ANIMATION_TYPE_IN,
                                             false, true);
        workspace->m_visible = true;
    }

    addOverviewRenderContext(true);
    ((render_workspace_t)(render_workspace_hook->m_original))(g_pHyprRenderer.get(), monitor,
                                                              workspace, time, render_box);
    addOverviewRenderContext(false);

    if (workspace != nullptr) {
        Animation::Workspace::startAnimation(workspace, Animation::Workspace::ANIMATION_TYPE_OUT,
                                             false, true);
        workspace->m_visible = false;
    }
}
