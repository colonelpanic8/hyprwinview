#ifndef HYPRWINVIEW_WINVIEW_PASS_ELEMENT_HPP
#define HYPRWINVIEW_WINVIEW_PASS_ELEMENT_HPP

#include <hyprland/src/render/pass/PassElement.hpp>

class CWindowOverview;

class CWinviewPassElement : public IPassElement {
  public:
    explicit CWinviewPassElement(bool foreground) : foreground(foreground) {}
    ~CWinviewPassElement() override = default;

    std::vector<UP<IPassElement>> draw(Render::CRenderContext& ctx) override;
    bool                          needsLiveBlur(Render::CRenderContext& ctx) override;
    bool                          needsPrecomputeBlur(Render::CRenderContext& ctx) override;
    std::optional<CBox>           boundingBox(Render::CRenderContext& ctx) override;
    CRegion                       opaqueRegion(Render::CRenderContext& ctx) override;

    const char*                   passName() override {
        return "CWinviewPassElement";
    }

    ePassElementType type() override {
        return EK_CUSTOM;
    }

  private:
    bool foreground = false;
};

#endif
