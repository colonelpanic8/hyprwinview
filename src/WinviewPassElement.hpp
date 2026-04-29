#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

class CWindowOverview;

class CWinviewPassElement : public IPassElement {
  public:
    CWinviewPassElement() = default;
    virtual ~CWinviewPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                needsLiveBlur() override;
    virtual bool                needsPrecomputeBlur() override;
    virtual std::optional<CBox> boundingBox() override;
    virtual CRegion             opaqueRegion() override;

    virtual const char*         passName() override {
        return "CWinviewPassElement";
    }

    virtual ePassElementType type() override {
        return EK_CUSTOM;
    }
};
