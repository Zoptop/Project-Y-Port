#include "CInterfaceElement.h"

#include <cmath>

#include "core/CMath.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "render/CMesh.h"

namespace py {

// Ghidra 0x18979c
CInterfaceElement::CInterfaceElement(CInterfaceMenu* menu, float x, float y)
    : mX(x), mY(y), mMenu(menu), mEngine(CEngine::getSingletonPtr()) {}

// Ghidra 0x189808
CInterfaceElement::~CInterfaceElement() = default;

void drawSubMesh(const CMesh* mesh, const std::string& name) {
    if (!mesh) return;
    uint32_t first = 0, count = 0;
    mesh->getSubMeshData(name, first, count);   // logs "existiert nicht" and returns 0/0 when missing
    mesh->draw(first, count);
}

float animatePressHighlight(float& anim, bool& clicked, bool pressed, float dt) {
    const float d = dt + dt;
    float t;
    if (clicked) {
        t = anim + d;
        if (t > 1.0f) {
            clicked = false;
            t = 0.0f;
        }
        anim = t;
    } else if (!pressed) {
        t = anim;
        if (t >= 0.5f) {
            t = t + d;
            anim = t;
        }
        if (t <= 0.5f) {
            t = t - d;
            anim = t;
        }
    } else {
        t = anim;
        if (t < 0.5f) {
            t = t + d;
            anim = t;
        }
        if (t >= 0.5f) {
            t = t - d;
            anim = t;
        }
    }
    anim = CUtils::clamp(t, 0.0f, 1.0f);
    CMath::initConstants();
    const float a = anim * CMath::TWO_PI + CMath::HALF_PI;
    const float s = CMath::getTrigTableSize() ? CMath::SinTable(a) : std::sin(a);
    return (1.0f - s) * 0.5f;
}

std::string formatNoArgs(const std::string& fmt) {
    std::string out;
    out.reserve(fmt.size());
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == '%') ++i;
        out.push_back(fmt[i]);
    }
    return out;
}

}  // namespace py
