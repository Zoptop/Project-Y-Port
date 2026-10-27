// CWeaponDraw.cpp - the weapons' drawMesh (vtable +0x10), called by
// CUnit::draw after the body: each turret part relative to the unit (MVP =
// the unit's MVP (+0x14c) x the part's local transform), in the owner's
// colour (neutral while the unit is hidden by the fog of war), then the
// weapon's own health bar (CWeapon::drawMesh). Drawing state only.
#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "game/CWeaponEMPWave.h"
#include "game/CWeaponHacker.h"
#include "game/CWeaponLaserSingleBTurretGun.h"
#include "game/CWeaponLaserSingleShot.h"
#include "game/CWeaponMiniLaser.h"
#include "game/CWeaponTankGun.h"
#include "game/CWeaponTurretGun.h"
#include "game/CWeaponTurretRocketLauncher.h"
#include "game/CWeaponVisionTower.h"
#include "game/DrawUtil.h"
#include "render/CGLSLProgram.h"
#include "render/CIngameQuads.h"
#include "render/CMesh.h"

namespace py {

namespace {
CQuaternion yawQ(Real yaw) {
    CQuaternion q;
    q.FromAngleAxis(yaw, CVector3::UNIT_Z);
    return q;
}
CMatrix4 localTransform(const CVector3& pos, const CQuaternion& q) {
    CMatrix4 m;
    m.makeTransform(pos, CVector3::UNIT_SCALE, q);
    return m;
}
// FromEulerAnglesZYX(yaw, pitch, 0) as a quaternion (the launchers' tilt)
CQuaternion yawPitchQ(Real yaw, Real pitch) {
    CMatrix3 r;
    r.FromEulerAnglesZYX(yaw, pitch, 0.0f);
    CQuaternion q;
    q.FromRotationMatrix(r);
    return q;
}
}  // namespace

void CWeapon::drawPart(bool alphaPass, float alpha, const CMatrix4& local, const CQuaternion& localRotation,
                       const char* subMesh) {
    CGame* g = draw::game();
    CGLSLProgram* sh = alphaPass ? g->mAlphaShader : g->mObjectShader;
    if (!sh || !g->mGameObjectsMesh) return;
    if (alphaPass) sh->setUniformAlpha(alpha);
    // the light in the part's space: (unit rotation x part rotation)^-1 L
    const CQuaternion q = mUnit->mOrientation * localRotation;
    sh->setUniformMVPMatrix(mUnit->mMVP * local);
    sh->setUniformPlayerColor(draw::colorByAlpha(mUnit->mAlpha, mUnit->mOwner));
    sh->setUniformLightDir(q.Inverse() * draw::lightDir());
    const CGame::SubMesh m = g->subMesh(subMesh);
    g->mGameObjectsMesh->draw(m.first, m.count);
}

// Ghidra 0x3d45e4: the health bar of a destroyable weapon (turret guns),
// 6 above the unit, in the owner's colour x 0.2.
void CWeapon::drawMesh(bool, float) {
    if (!mDestroyable || draw::strategyView() || !mUnit->isAlive()) return;
    CDrawContext& dc = draw::ctx();
    if (!(Real(mUnit->mAlpha) > 0.25f) || dc.objectInfoMode || !dc.quads) return;
    const CVector3 c(mUnit->mPosition.x, mUnit->mPosition.y + 6.0f, 5.0f);
    const Real u = (Real(1.0f) - mHealth / (mMaxHealth * mUnit->mHealthFactor)) * 64.0f * bitsf(0x3a800000);
    const Vec3& o = mUnit->mOwner->mColor;
    const CVector3 col(o.x * bitsf(0x3e4ccccd), o.y * bitsf(0x3e4ccccd), o.z * bitsf(0x3e4ccccd));
    dc.quads->addColoredQuad(c, CVector2(u, 0.25f), mUnit->mSelectionUV0, mUnit->mSelectionUV1, col, draw::cameraYaw());
}

// Ghidra 0x3e3ec8: "TurretTopX" turned by the turret yaw (+0xb0).
void CWeaponTurretGun::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q = yawQ(mYaw);
    drawPart(alphaPass, alpha, localTransform(mMountPoint, q), q, "TurretTopX");
    CWeapon::drawMesh(alphaPass, 0.0f);
}

// Ghidra 0x3d7380: "EMPT2T1" turned by +0xa8.
void CWeaponEMPWave::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q = yawQ(mYaw);
    drawPart(alphaPass, alpha, localTransform(mMountPoint, q), q, "EMPT2T1");
    CWeapon::drawMesh(alphaPass, 0.0f);
}

// Ghidra 0x3dc3f0: "TurretTop1" turned by +0xa8.
void CWeaponLaserSingleBTurretGun::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q = yawQ(mYaw);
    drawPart(alphaPass, alpha, localTransform(mMountPoint, q), q, "TurretTop1");
    CWeapon::drawMesh(alphaPass, 0.0f);
}

// Ghidra 0x3dd740: "SpiderGun1" turned by +0xa8 (no health bar).
void CWeaponLaserSingleShot::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q = yawQ(mYaw);
    drawPart(alphaPass, alpha, localTransform(mMountPoint, q), q, "SpiderGun1");
}

// Ghidra 0x3df1c4: "SpiderGun1" turned by +0xa8 (no health bar).
void CWeaponMiniLaser::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q = yawQ(mYaw);
    drawPart(alphaPass, alpha, localTransform(mMountPoint, q), q, "SpiderGun1");
}

// Ghidra 0x3e144c: the tank's launcher "TankLaunch", raised by 1.1 t and
// tilted up by 35 t degrees with the deploy state t (+0xcc).
void CWeaponTankGun::drawMesh(bool alphaPass, float alpha) {
    const Real pitch = mTransformState * -35.0f * CMath::fDeg2Rad;
    const CQuaternion q = yawPitchQ(mYaw, pitch);
    const Real lift = mTransformState * bitsf(0x3f8ccccd);
    const CVector3 pos(mMountPoint.x + 0.0f, mMountPoint.y + 0.0f, lift + mMountPoint.z);
    drawPart(alphaPass, alpha, localTransform(pos, q), q, "TankLaunch");
    CWeapon::drawMesh(alphaPass, 0.0f);
}

// Ghidra 0x3e5b94: the launcher "TankLaunch" tilted up by 35 degrees,
// 4.262 above the mount, and the turret "Tank1" turned by +0xa8, 2 behind
// and 1.217 above it.
void CWeaponTurretRocketLauncher::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q1 = yawPitchQ(mYaw, CMath::fDeg2Rad * -35.0f);
    const CVector3 p1(mMountPoint.x + 0.0f, mMountPoint.y + 0.0f, mMountPoint.z + bitsf(0x4088624e));
    drawPart(alphaPass, alpha, localTransform(p1, q1), q1, "TankLaunch");
    const CQuaternion q2 = yawQ(mYaw);
    const Real c = CMath::SinTable(mYaw + CMath::HALF_PI);
    const Real s = CMath::SinTable(mYaw);
    const Real ox = c * -2.0f - s * 0.0f;
    const Real oy = s * -2.0f + c * 0.0f;
    const CVector3 p2(mMountPoint.x + ox, mMountPoint.y + oy, mMountPoint.z + bitsf(0x3f9bc6a8));
    drawPart(alphaPass, alpha, localTransform(p2, q2), q2, "Tank1");
    CWeapon::drawMesh(alphaPass, 0.0f);
}

// Ghidra 0x3e6ddc: the dish "SpecOpRad" turned by +0x90, 15.1 below the
// mount point.
void CWeaponVisionTower::drawMesh(bool alphaPass, float alpha) {
    const CQuaternion q = yawQ(mRotation);
    const CVector3 pos(mMountPoint.x, mMountPoint.y, mMountPoint.z - bitsf(0x4171999a));
    drawPart(alphaPass, alpha, localTransform(pos, q), q, "SpecOpRad");
    CWeapon::drawMesh(alphaPass, 0.0f);
}

// Ghidra 0x3d8d8c: only the hacking progress label.
void CWeaponHacker::drawMesh(bool, float) {
    // while hacking: the target's remaining resistance above the unit
    CTextRender* t = draw::text();
    if (!t || !(Real(mUnit->mAlpha) > 0.25f) || mCurrentHackTarget == 0) return;
    const float k = bitsf(0x3f333333);
    const Vec3 col(CVector3::COL_GREEN.x * k, CVector3::COL_GREEN.y * k, CVector3::COL_GREEN.z * k);
    const Real v = Real(100.0f) - mHackTargetResistance * 100.0f;
    t->drawStringScreen(mUnit->getScreenPosForLabel(0.0f, 16.0f, 12.0f), draw::guiScale() * 28.0f, col, 1.0f, 1,
                        "%02.1f%%", static_cast<double>(v));
}

}  // namespace py
