#include "pass_element.hpp"

HTPassElement::HTPassElement() {
    ;
}

std::vector<UP<IPassElement>> HTPassElement::draw(Render::CRenderContext&) {
    return {};
}

bool HTPassElement::needsLiveBlur(Render::CRenderContext&) {
    return false;
}

bool HTPassElement::needsPrecomputeBlur(Render::CRenderContext&) {
    // hyprexpo uses false
    return true;
}

bool HTPassElement::disableSimplification(Render::CRenderContext&) {
    return true;
}
