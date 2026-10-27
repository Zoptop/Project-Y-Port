// CUnitFactoryDraw.cpp - unit models drawn flat on the screen: the turning
// unit at the centre of the dial menu (CDialMenu::drawCenterMesh ->
// CUnitFactory::drawUnit -> CUnit<type>::drawUnit -> CUnit::drawMeshFlat /
// drawShadowFlat, and the weapons through CWeaponFactory::drawWeapon ->
// CWeapon<type>::drawWeapon -> CWeapon::drawMeshFlat), and the unit type
// names (CUnitFactory::getUnitTypeName).
//
// Every drawUnit reads the unit's "Unit<name>BoundingRadius" (the model's
// size on the screen, x 1.3 for the brains and suits) and
// "Unit<name>ZOffset" from the config, draws the shadow, the body parts at
// their fixed offsets (the driver brain "Robobrain" sits in every vehicle)
// and the weapons at the config's mount points. The offsets are the
// original's constants (decimal values of the float bit patterns), added in
// its order. Drawing only; render maths in plain float.
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "game/CUnitAmplifier.h"
#include "game/CUnitBombSuit.h"
#include "game/CUnitCloakSuit.h"
#include "game/CUnitCommander.h"
#include "game/CUnitEMPTank.h"
#include "game/CUnitHackerSuit.h"
#include "game/CUnitHarvester.h"
#include "game/CUnitHeavyTank.h"
#include "game/CUnitMineDropper.h"
#include "game/CUnitRepair.h"
#include "game/CUnitRoboBrain.h"
#include "game/CUnitScorpion.h"
#include "game/CUnitShieldTank.h"
#include "game/CUnitSniperSuit.h"
#include "game/CUnitSoldierSuit.h"
#include "game/CUnitSpider.h"
#include "game/CUnitSpotter.h"
#include "game/CUnitTank.h"
#include "game/CUnitThinkerBrain.h"
#include "game/CWeapon.h"
#include "game/CWeaponEMPWave.h"
#include "game/CWeaponLaserSingleBTurretGun.h"
#include "game/CWeaponLaserSingleShot.h"
#include "game/CWeaponMiniLaser.h"
#include "game/CWeaponTankGun.h"
#include "game/CWeaponTurretGun.h"
#include "game/CWeaponTurretRocketLauncher.h"
#include "game/CWeaponVisionTower.h"
#include "render/CCameraBase.h"
#include "render/CFakeShadow.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CQuad.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned kTexture0 = 0x84c0;   // GL_TEXTURE0
constexpr unsigned kTriangles = 4;       // GL_TRIANGLES

// engine +0x168 -> getCamera (vtable +0x28): the viewport of the current interface
bool viewportSize(float& w, float& h) {
    CCamera* c = CEngine::getSingletonPtr()->getCurrentCamera();
    if (!c) return false;
    w = static_cast<float>(c->mWidth);
    h = static_cast<float>(c->mHeight);
    return true;
}

// (-1, 1, 1) normalised as the original: 1 / sqrt(3) when the length is
// above 1e-8 (0x322bcc77)
CVector3 flatLight() {
    CVector3 l(-1.0f, 1.0f, 1.0f);
    const float len = fdlibm::sqrtf(3.0f);
    if (len > bitsf(0x322bcc77)) {
        const float inv = 1.0f / len;
        l = CVector3(inv * l.x, inv * l.y, inv * l.z);
    }
    return l;
}

Real cfgReal(const char* key) {
    Real v = 0.0f;
    CConfig::get().updateConfigValue(v, key);
    return v;
}
CVector3 cfgVec3(const char* key) {
    CVector3 v = CVector3::ZERO;
    CConfig::get().updateConfigValue(v, key);
    return v;
}

// The pattern of every drawUnit: the base offset (0, 0, ZOffset) plus a part's
// constant offset, component by component as the original adds them.
CVector3 at(const CVector3& base, float dx, float dy, float dz) {
    return CVector3(base.x + dx, base.y + dy, base.z + dz);
}
}  // namespace

// ---------------------------------------------------------------- names

// Ghidra 0x3bb854
std::string CUnitFactory::getUnitTypeName(unsigned type) {
    static const int kIds[25] = {-1,   0x42, 0x40, 0x41, 0x44, 0x43,  0x45,  0x48,  0x3f,  0x46,  0x47,  -2,   0xa6,
                                 0xa8, 0xaa, 0xad, 300,  0x134, 0x132, 0x13a, 0x138, 0x136, 0x1a4, 0x1a6, 0x278};
    if (type == 0 || type > 24) return "Unknown";
    if (kIds[type] == -2) return "Beacon";
    return CTxtLoc::get().getS(kIds[type]);
}

// ---------------------------------------------------------------- the flat draws

// Ghidra 0x3a7134. MVP = ortho(180, viewport, screenPos, size) x
// makeTransform(q * offset, 1, q); the object shader of gameobjects.3DS.mesh
// with the interface player's colour and the light (-1, 1, 1)/sqrt(3) in
// model space (q^-1 L). [!] LIGHTCOLOR is not set: it keeps the last value
// CGame::draw gave the shader.
void CUnit::drawMeshFlat(const CMatrix4& /*view*/, const std::string& subMesh, const CVector2& screenPos, Real size,
                         const CQuaternion& q, const CVector3& offset) {
    CEngine* e = CEngine::getSingletonPtr();
    CGame* g = e->mGame;
    float w, h;
    if (!g || !g->mGameObjectsMesh || !g->mObjectShader || !viewportSize(w, h)) return;
    const CMatrix4 ortho = CCamera::createOrthogonalProjectionMatrix(180.0f, w, h, screenPos, size);
    CMatrix4 model;
    model.makeTransform(q * offset, CVector3::UNIT_SCALE, q);
    const CMatrix4 mvp = ortho * model;
    const CVector3 light = flatLight();
    g->mGameObjectsMesh->bindBuffer();
    g->mObjectShader->startDraw();
    if (g->mObjectTexture) g->mObjectTexture->bind(kTexture0);
    if (CPlayer* p = CDrawContext::get().player)
        g->mObjectShader->setUniformPlayerColor(CVector3(p->mColor.x, p->mColor.y, p->mColor.z));
    g->mObjectShader->setUniformMVPMatrix(mvp);
    g->mObjectShader->setUniformLightDir(q.Inverse() * light);
    uint32_t first = 0, count = 0;
    g->mGameObjectsMesh->getSubMeshData(subMesh, first, count);
    g->mGameObjectsMesh->draw(first, count);
    g->mObjectShader->endDraw();
}

// Ghidra 0x3d4c04 (the same code)
void CWeapon::drawMeshFlat(const CMatrix4& view, const std::string& subMesh, const CVector2& screenPos, Real size,
                           const CQuaternion& q, const CVector3& offset) {
    CUnit::drawMeshFlat(view, subMesh, screenPos, size, q, offset);
}

// Ghidra 0x3a6474. The cell of shadow.png from "Unit<name>ShadowUVTopleft"
// and "...UVSize" (in eighths of the texture), shrunk on every side by
// "...ShadowUVShinkFactor" k (the quad by the same 1 - 2k), half size
// "...ShadowSizeHalf"; TTL (alpha) 1.3, at (1/sqrt(3), -1/sqrt(3), 1) turned
// by q under the model, with CFakeShadow's shader and texture.
void CUnit::drawShadowFlat(const CMatrix4& /*view*/, const char* name, const CVector2& screenPos, Real size,
                           const CQuaternion& q) {
    CEngine* e = CEngine::getSingletonPtr();
    CGame* g = e->mGame;
    CFakeShadow* fs = g ? g->getFakeShadow() : nullptr;
    float w, h;
    if (!fs || !fs->shader() || !viewportSize(w, h)) return;
    const CConfig& cfg = CConfig::get();
    const std::string base = std::string("Unit") + name;
    Real k = 0.0f;
    CVector2 half = CVector2::ZERO, uvSize = CVector2::ZERO, uvTL = CVector2::ZERO;
    cfg.updateConfigValue(k, base + "ShadowUVShinkFactor");
    cfg.updateConfigValue(half, base + "ShadowSizeHalf");
    cfg.updateConfigValue(uvSize, base + "ShadowUVSize");
    const CVector2 cellSize(uvSize.x * 0.125f, uvSize.y * 0.125f);
    cfg.updateConfigValue(uvTL, base + "ShadowUVTopleft");
    const CVector2 cellTL(uvTL.x * 0.125f, uvTL.y * 0.125f);
    const CVector2 tl(cellTL.x + k * cellSize.x, cellTL.y + k * cellSize.y);
    const Real f = Real(1.0f) - (k + k);
    const CVector2 sz(f * cellSize.x, f * cellSize.y);
    const CVector2 hs(f * half.x, f * half.y);
    const float len = fdlibm::sqrtf(3.0f);
    float lx = 1.0f, ly = -1.0f;
    if (len > bitsf(0x322bcc77)) {
        const float inv = 1.0f / len;
        lx = inv + 0.0f;
        ly = 0.0f - inv;
    }
    const CQuadPosUVTTL quad(CVector3::ZERO, hs, tl, sz, 1.3f, 0.0f, nullptr);
    CMatrix4 model;
    model.makeTransform(CVector3(lx, ly, 1.0f), CVector3::UNIT_SCALE, q);
    const CMatrix4 ortho = CCamera::createOrthogonalProjectionMatrix(180.0f, w, h, screenPos, size);
    const CMatrix4 mvp = ortho * model;
    fs->shader()->startDrawLocalPosUVTTLBlend(quad.v);
    if (fs->texture()) fs->texture()->bind(kTexture0);
    fs->shader()->setUniformMVPMatrix(mvp);
    CGLSLProgram::drawLocal(kTriangles, 0, 6);
    fs->shader()->endDrawLocalPosUVTTLBlend();
}

// ---------------------------------------------------------------- the units

// Ghidra 0x3bb01c (types 4 turret, 7 crate, 0xb beacon, 0xf landmine and
// 0x17 uber brain: their drawUnit is empty)
void CUnitFactory::drawUnit(unsigned type, const CMatrix4& view, const CVector2& p, const CQuaternion& q) {
    switch (type) {
        case 1: CUnitRoboBrain::drawUnit(view, p, q); return;
        case 2: CUnitSpider::drawUnit(view, p, q); return;
        case 3: CUnitScorpion::drawUnit(view, p, q); return;
        case 5: CUnitTank::drawUnit(view, p, q); return;
        case 6: CUnitBombSuit::drawUnit(view, p, q); return;
        case 8: CUnitRepair::drawUnit(view, p, q); return;
        case 9: CUnitSoldierSuit::drawUnit(view, p, q); return;
        case 10: CUnitHeavyTank::drawUnit(view, p, q); return;
        case 0xc: CUnitSpotter::drawUnit(view, p, q); return;
        case 0xd: CUnitEMPTank::drawUnit(view, p, q); return;
        case 0xe: CUnitMineDropper::drawUnit(view, p, q); return;
        case 0x10: CUnitShieldTank::drawUnit(view, p, q); return;
        case 0x11: CUnitCommander::drawUnit(view, p, q); return;
        case 0x12: CUnitAmplifier::drawUnit(view, p, q); return;
        case 0x13: CUnitSniperSuit::drawUnit(view, p, q); return;
        case 0x14: CUnitCloakSuit::drawUnit(view, p, q); return;
        case 0x15: CUnitHackerSuit::drawUnit(view, p, q); return;
        case 0x16: CUnitThinkerBrain::drawUnit(view, p, q); return;
        case 0x18: CUnitHarvester::drawUnit(view, p, q); return;
        default: return;   // 4, 7, 0xb, 0xf, 0x17
    }
}

// Ghidra 0x3c13b8
void CUnitRoboBrain::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitRobobrainBoundingRadius") * bitsf(0x3fa66666);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitRobobrainZOffset"));
    CUnit::drawShadowFlat(v, "Robobrain", p, r, q);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, base);
}

// Ghidra 0x3c7930
void CUnitSpider::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitSpiderBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitSpiderZOffset"));
    CUnit::drawShadowFlat(v, "Spider", p, r, q);
    CUnit::drawMeshFlat(v, "Spider1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, 0.877f, 0.0f, 0.62f));
    const CVector3 m = cfgVec3("UnitSpiderWeaponMountPoint");
    CWeaponFactory::drawWeapon(7, v, p, q, r, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3c49cc. [!] The driver brain's offset is absolute (no ZOffset).
void CUnitScorpion::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitScorpionBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitScorpionZOffset"));
    CUnit::drawShadowFlat(v, "Scorpion", p, r, q);
    CUnit::drawMeshFlat(v, "Scorpion1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(bitsf(0xbed91687), bitsf(0x402af1aa), bitsf(0x3ee04189)));   // (-0.424, 2.671, 0.438)
}

// Ghidra 0x3ccfd0
void CUnitTank::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitTankBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitTankZOffset"));
    CUnit::drawShadowFlat(v, "Tank", p, r, q);
    CUnit::drawMeshFlat(v, "Tank1", p, r, q, CVector3(base.x - 1.5f, base.y + 0.0f, base.z + 0.0f));
    CUnit::drawMeshFlat(v, "TankFront1", p, r, q,
                        CVector3(base.x + bitsf(0x3fb978d5), base.y + 0.0f, base.z - bitsf(0x3f9fdf3b)));   // 1.449, 1.249
    CUnit::drawMeshFlat(v, "TankBack1", p, r, q,
                        CVector3(base.x - bitsf(0x3f8a5e35), base.y + 0.0f, base.z - bitsf(0x3f9fdf3b)));   // 1.081, 1.249
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(base.x - bitsf(0x3e981064), base.y + bitsf(0x403b9581),
                                 base.z - bitsf(0x3fa39581)));   // 0.297, 2.931, 1.278
    const CVector3 m = cfgVec3("UnitTankWeaponMountPoint");
    CWeaponFactory::drawWeapon(2, v, p, q, r, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3b8228
void CUnitBombSuit::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitBombbrainBoundingRadius") * bitsf(0x3fa66666);
    CUnit::drawShadowFlat(v, "Bombbrain", p, r, q);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitBombbrainZOffset"));
    CUnit::drawMeshFlat(v, "Bombbrain1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, 0.0f, 0.0f, 0.5f));
}

// Ghidra 0x3bfee0
void CUnitRepair::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitRepairBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitRepairZOffset"));
    CUnit::drawShadowFlat(v, "Repair", p, r, q);
    CUnit::drawMeshFlat(v, "URepair1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(base.x + bitsf(0x3fde147b), base.y + 0.0f, base.z - bitsf(0x3e5a1cac)));   // 1.735, 0.213
}

// Ghidra 0x3c737c
void CUnitSoldierSuit::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitSoldierBrainBoundingRadius") * bitsf(0x3fa66666);
    CUnit::drawShadowFlat(v, "SoldierBrain", p, r, q);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitSoldierBrainZOffset"));
    CUnit::drawMeshFlat(v, "SoldierBra", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, 0.0f, 0.0f, 0.5f));
    const CVector3 m = cfgVec3("UnitSoldierBrainWeaponMountPoint");
    CWeaponFactory::drawWeapon(5, v, p, q, r, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3bdc1c
void CUnitHeavyTank::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitHeavyTankBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitHeavyTankZOffset"));
    CUnit::drawShadowFlat(v, "HeavyTank", p, r, q);
    CUnit::drawMeshFlat(v, "HeavyT1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(base.x - 1.0f, base.y + 0.0f, base.z + bitsf(0x3ebe76c9)));   // 0.372
    CVector3 m = cfgVec3("UnitHeavyTankWeaponMountPointLaser1");
    CWeaponFactory::drawWeapon(7, v, p, q, r, at(base, m.x, m.y, m.z));
    m = cfgVec3("UnitHeavyTankWeaponMountPointLaser2");
    CWeaponFactory::drawWeapon(7, v, p, q, r, at(base, m.x, m.y, m.z));
    m = cfgVec3("UnitHeavyTankWeaponMountPointMiniLaser1");
    CWeaponFactory::drawWeapon(5, v, p, q, r, at(base, m.x, m.y, m.z));
    m = cfgVec3("UnitHeavyTankWeaponMountPointMiniLaser2");
    CWeaponFactory::drawWeapon(5, v, p, q, r, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3c8c2c: the UAV ("uav1") at the "UAVMountPoint" is a mesh, not a weapon
void CUnitSpotter::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitSpotterBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitSpotterZOffset"));
    CUnit::drawShadowFlat(v, "Spotter", p, r, q);
    CUnit::drawMeshFlat(v, "spotter1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(base.x - bitsf(0x3f847ae1), base.y + 0.0f, base.z + 3.0f));   // 1.035
    const CVector3 m = cfgVec3("UnitSpotterUAVMountPoint");
    CUnit::drawMeshFlat(v, "uav1", p, r, q, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3baa68
void CUnitEMPTank::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitEMPTankBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitEMPTankZOffset"));
    CUnit::drawShadowFlat(v, "EMPTank", p, r, q);
    CUnit::drawMeshFlat(v, "EMPT2B1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, bitsf(0x3f4ccccd), 0.0f, bitsf(0x408722d1)));   // 0.8, 4.223
    const CVector3 m = cfgVec3("UnitEMPTankWeaponMountPoint");
    CWeaponFactory::drawWeapon(0xb, v, p, q, r, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3beca4
void CUnitMineDropper::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitMineDropperBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitMineDropperZOffset"));
    CUnit::drawShadowFlat(v, "MineDropper", p, r, q);
    CUnit::drawMeshFlat(v, "minelayer1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        at(base, bitsf(0x3f939581), bitsf(0x404a1cac), bitsf(0x40823d71)));   // 1.153, 3.158, 4.07
}

// Ghidra 0x3c5a54
void CUnitShieldTank::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitShieldTankBoundingRadius");
    CUnit::drawShadowFlat(v, "ShieldTank", p, r, q);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitShieldTankZOffset"));
    CUnit::drawMeshFlat(v, "emptank1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, bitsf(0x40546a7f), 0.0f, 3.0f));   // 3.319
}

// Ghidra 0x3b9a10
void CUnitCommander::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitCommanderBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitCommanderZOffset"));
    CUnit::drawShadowFlat(v, "Commander", p, r, q);
    CUnit::drawMeshFlat(v, "commander", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(base.x - bitsf(0x408c6a7f), base.y + bitsf(0x402fced9),
                                 base.z - bitsf(0x3c03126f)));   // 4.388, 2.747, 0.008
}

// Ghidra 0x3b7954
void CUnitAmplifier::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitAmplifierBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitAmplifierZOffset"));
    CUnit::drawShadowFlat(v, "Amplifier", p, r, q);
    CUnit::drawMeshFlat(v, "amplifier", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q,
                        CVector3(base.x - bitsf(0x3ffe5604), base.y - bitsf(0x403072b0),
                                 base.z + bitsf(0x406eb852)));   // 1.987, 2.757, 3.73
}

// Ghidra 0x3c6e94
void CUnitSniperSuit::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitSniperSuitBoundingRadius") * bitsf(0x3fa66666);
    CUnit::drawShadowFlat(v, "SniperSuit", p, r, q);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitSniperSuitZOffset"));
    CUnit::drawMeshFlat(v, "sniper1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, 0.0f, 0.0f, bitsf(0x3f451eb8)));   // 0.77
}

// Ghidra 0x3b8c40
void CUnitCloakSuit::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitCloakSuitBoundingRadius") * bitsf(0x3fa66666);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitCloakSuitZOffset"));
    CUnit::drawShadowFlat(v, "CloakSuit", p, r, q);
    CUnit::drawMeshFlat(v, "cloaker", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, 0.0f, 0.0f, bitsf(0x3f67ef9e)));   // 0.906
    const CVector3 m = cfgVec3("UnitCloakSuitWeaponMountPoint");
    CWeaponFactory::drawWeapon(5, v, p, q, r, at(base, m.x, m.y, m.z));
}

// Ghidra 0x3bca8c
void CUnitHackerSuit::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitHackerSuitBoundingRadius") * bitsf(0x3fa66666);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitHackerSuitZOffset"));
    CUnit::drawShadowFlat(v, "HackerSuit", p, r, q);
    CUnit::drawMeshFlat(v, "hacker1", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, 0.0f, 0.0f, bitsf(0x3e88b439)));   // 0.267
}

// Ghidra 0x3ce544
void CUnitThinkerBrain::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitThinkerBrainBoundingRadius") * bitsf(0x3fa66666);
    CUnit::drawShadowFlat(v, "ThinkerBrain", p, r, q);
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitThinkerBrainZOffset"));
    CUnit::drawMeshFlat(v, "tbrain1", p, r, q, base);
}

// Ghidra 0x3bcf1c
void CUnitHarvester::drawUnit(const CMatrix4& v, const CVector2& p, const CQuaternion& q) {
    const Real r = cfgReal("UnitHarvesterBoundingRadius");
    const CVector3 base(0.0f, 0.0f, cfgReal("UnitHarvesterZOffset"));
    CUnit::drawShadowFlat(v, "Harvester", p, r, q);
    CUnit::drawMeshFlat(v, "Harvester", p, r, q, base);
    CUnit::drawMeshFlat(v, "Robobrain", p, r, q, at(base, bitsf(0x4108f5c3), 0.0f, bitsf(0x40d570a4)));   // 8.56, 6.67
}

// ---------------------------------------------------------------- the weapons

// Ghidra 0x3d8b94
void CWeaponFactory::drawWeapon(unsigned type, const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                const CVector3& offset) {
    switch (type) {
        case 1: CWeaponTurretGun::drawWeapon(v, p, q, size, offset); return;
        case 2: CWeaponTankGun::drawWeapon(v, p, q, size, offset); return;
        case 5: CWeaponMiniLaser::drawWeapon(v, p, q, size, offset); return;
        case 7: CWeaponLaserSingleShot::drawWeapon(v, p, q, size, offset); return;
        case 8: CWeaponLaserSingleBTurretGun::drawWeapon(v, p, q, size, offset); return;
        case 10: CWeaponTurretRocketLauncher::drawWeapon(v, p, q, size, offset); return;
        case 0xb: CWeaponEMPWave::drawWeapon(v, p, q, size, offset); return;
        case 0xe: CWeaponVisionTower::drawWeapon(v, p, q, size, offset); return;
        // 4 LaserGatling, 6 HeavyLaserGatling, 0xc SniperGun, 0xd Hacker: empty
        // drawWeapon (Ghidra 0x3db408 / 0x3daaf8 / 0x3e0c6c / 0x3d96f0); 3, 9: none
        default: return;
    }
}

// Ghidra 0x3e3d3c ([!] the mount point is not used)
void CWeaponTurretGun::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                  const CVector3&) {
    CWeapon::drawMeshFlat(v, "TurretTopX", p, size, q, CVector3::ZERO);
}

// Ghidra 0x3e2290
void CWeaponTankGun::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                const CVector3& offset) {
    CWeapon::drawMeshFlat(v, "TankLaunch", p, size, q, offset);
}

// Ghidra 0x3e0410
void CWeaponMiniLaser::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                  const CVector3& offset) {
    CWeapon::drawMeshFlat(v, "SpiderGun1", p, size, q, offset);
}

// Ghidra 0x3de178
void CWeaponLaserSingleShot::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                        const CVector3& offset) {
    CWeapon::drawMeshFlat(v, "SpiderGun1", p, size, q, offset);
}

// Ghidra 0x3dba30 ([!] the mount point is not used)
void CWeaponLaserSingleBTurretGun::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                              const CVector3&) {
    CWeapon::drawMeshFlat(v, "TurretTop1", p, size, q, CVector3::ZERO);
}

// Ghidra 0x3e536c ([!] the mount point is not used)
void CWeaponTurretRocketLauncher::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                             const CVector3&) {
    CWeapon::drawMeshFlat(v, "TankLaunch", p, size, q, CVector3::ZERO);
    CWeapon::drawMeshFlat(v, "Tank1", p, size, q, CVector3::ZERO);
}

// Ghidra 0x3d6a3c
void CWeaponEMPWave::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                const CVector3& offset) {
    CWeapon::drawMeshFlat(v, "EMPT2T1", p, size, q, offset);
}

// Ghidra 0x3e6d44 ([!] the mount point is not used)
void CWeaponVisionTower::drawWeapon(const CMatrix4& v, const CVector2& p, const CQuaternion& q, Real size,
                                    const CVector3&) {
    CWeapon::drawMeshFlat(v, "SpecOpRad", p, size, q, CVector3::ZERO);
}

}  // namespace py
