#ifndef HYPRWINVIEW_WINVIEW_PASS_ELEMENT_HPP
#define HYPRWINVIEW_WINVIEW_PASS_ELEMENT_HPP

#include <hyprland/src/render/pass/PassElement.hpp>

class CWindowOverview;

class CWinviewPassElement : public IPassElement {
  public:
    explicit CWinviewPassElement(bool foreground) : foreground(foreground) {}
    ~CWinviewPassElement() override = default;

    std::vector<UP<IPassElement>> draw() override;
    bool                          needsLiveBlur() override;
    bool                          needsPrecomputeBlur() override;
    std::optional<CBox>           boundingBox() override;
    CRegion                       opaqueRegion() override;

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
