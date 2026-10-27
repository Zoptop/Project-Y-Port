#include "CCameraMenu.h"

#include "core/CUtils.h"
#include "core/FloatBits.h"

namespace py {

// Ghidra 0x150c74
void CCameraMenu::refresh() {
    mPosition = Vec3(mTargetX, mTargetY, 520.0f / mZoom);   // 0x44020000 / zoom
    // FromEulerAnglesZYX(0, 0, 0) -> identity orientation (looking down -Z)
    mOrientationQuat = Quat();
    updateViewMatrix();
}

// Ghidra 0x150d4c
void CCameraMenu::timeUpdate(float dt) {
    mVelX = mVelX * bitsf(0x3f666666);   // 0.9
    mVelY = mVelY * bitsf(0x3f666666);
    mTargetX = mVelX + mTargetX;
    mTargetY = mVelY + mTargetY;
    if (mSlowScroll) {
        const float dx = mTargetX - mSlowX;
        const float dy = mTargetY - mSlowY;
        float k = mSlowSpeed * dt;
        if (k > 0.5f) k = 0.5f;
        mTargetX = mTargetX - dx * k;
        mTargetY = mTargetY - dy * k;
    }
    refresh();
}

// Ghidra 0x150eb4
void CCameraMenu::slowScrollTo(const CVector2& target, float speed) {
    CUtils::log("scroll to: %f %f %f", static_cast<double>(target.x), static_cast<double>(target.y),
                static_cast<double>(speed));
    mSlowScroll = true;
    mSlowX = target.x;
    mSlowY = target.y;
    mSlowSpeed = speed;
}

// The tail of CInterfaceMenu::setResolution / CInterfaceIntro::setResolution
// (Ghidra 0x1ad488 / 0x1acf7c).
void CCameraMenu::setZoomForAspect() {
    const float a = mAspect;
    const float lin = a * bitsf(0x4079bd87) - bitsf(0x3deec031);   // a*3.90220 - 0.11658
    const float quad = a * (a * bitsf(0xbfa816ed));                // a*(a*-1.31319)
    mZoom = lin + quad;
    mZoom = mZoom * bitsf(0x3f666666);                             // * 0.9
    refresh();
}

}  // namespace py
