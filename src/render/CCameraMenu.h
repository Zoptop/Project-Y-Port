// CCameraMenu.h - port of ProjectY::CCameraMenu (Ghidra 0x150be4..0x150f4c),
// the camera of the intro and menu screens. It looks straight down (-Z,
// orientation identity) at the z = 0 plane, where menu.3DS.mesh and the
// menu elements live, from height 520 / zoom above (x, y).
//
// Fields: +0x210 target x/y, +0x218 scroll velocity, +0x220 (unused here),
// +0x228 zoom (ctor 3.0; setResolution of the menu/intro sets it from the
// aspect), +0x22c slow-scroll active, +0x230 slow-scroll target, +0x238 speed.
#pragma once

#include "render/CCameraBase.h"

namespace py {

class CCameraMenu : public CCamera {
public:
    CCameraMenu() = default;

    void refresh();                 // Ghidra 0x150c74
    void timeUpdate(float dt);      // Ghidra 0x150d4c
    void slowScrollTo(const CVector2& target, float speed);   // Ghidra 0x150eb4

    // CInterfaceMenu/CInterfaceIntro::setResolution (Ghidra 0x1ad488):
    // zoom = (aspect*3.902 - 0.1165 + aspect*(aspect*-1.3132)) * 0.9, refresh.
    void setZoomForAspect();

    float mTargetX = 0.0f, mTargetY = 0.0f;   // +0x210
    float mVelX = 0.0f, mVelY = 0.0f;         // +0x218
    float mUnusedX = 0.0f, mUnusedY = 0.0f;   // +0x220
    float mZoom = 3.0f;                       // +0x228
    bool mSlowScroll = false;                 // +0x22c
    float mSlowX = 0.0f, mSlowY = 0.0f;       // +0x230
    float mSlowSpeed = 0.0f;                  // +0x238
};

}  // namespace py
