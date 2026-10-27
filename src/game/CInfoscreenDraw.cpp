// CInfoscreenDraw.cpp - the texts of the object-info screen (the action
// bar's "Info" button: IPP::startObjectInfo / drawInfoScreen, Ghidra
// 0x1a85a4 / 0x1a21c4). IPP::drawInfoScreen calls the object's
// drawInfoscreen (vtable +0x94) with the pixel point (10, 50) x GUI scale;
// each class writes its lines with CTextRender::drawStringPixel (white unless
// noted, left aligned) and moves pos.y below them. The overrides of all
// classes are here, next to each other, as one feature.
//
// Line sizes (x GUI scale g): name 40, description 24 (wrapped at the screen
// width - 20 g), unit and building lines 28, weapon lines 24, the lab's
// technology lists 18. Drawing only.
#include <cstdio>
#include <sstream>
#include <string>

#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CBuildingHQ.h"
#include "game/CBuildingProductionAdvTech.h"
#include "game/CBuildingProductionLarge.h"
#include "game/CBuildingProductionLight.h"
#include "game/CBuildingProductionMedium.h"
#include "game/CBuildingScienceLab.h"
#include "game/CBuildingSpecop.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/CTechManager.h"
#include "game/CUnit.h"
#include "game/CWeapon.h"
#include "render/CCameraBase.h"
#include "render/CTextRender.h"

namespace py {

namespace {
CTextRender* tr() { return CEngine::getSingletonPtr()->getTextRender(); }
float gui() { return CEngine::getSingletonPtr()->mGUIScale; }
const char* C(int id) { return CTxtLoc::get().getC(id); }

// a white line at pos (size 28 g), then pos.y += 28 g
template <typename... A>
void line28(CVector2& pos, const char* fmt, A... args) {
    const float g = gui();
    tr()->drawStringPixel(pos.x, pos.y, g * 28.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, fmt, args...);
    pos.y = pos.y + g * 28.0f;
}

// The pattern of the production buildings: 10 g down, the header (CTxtLoc
// `header`, "%s:") and one line per product "%s (%.1f s)" (CTxtLoc name,
// CBuilding::getProductionTime(type, false)) indented by 10 g, 28 g apart.
// [!] No step after the last line.
void productList(CVector2& pos, int header, std::initializer_list<std::pair<int, unsigned>> items) {
    const float g = gui();
    pos.y = pos.y + g * 10.0f;
    tr()->drawStringPixel(pos.x, pos.y, g * 28.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, "%s:", C(header));
    for (const auto& it : items) {
        pos.y = pos.y + g * 28.0f;
        tr()->drawStringPixel(pos.x + g * 10.0f, pos.y, g * 28.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, "%s (%.1f s)",
                              C(it.first), static_cast<double>(CBuilding::getProductionTime(it.second, false)));
    }
}
}  // namespace

// Ghidra 0x1815d0: the name (+0xc8, size 40 g) and, 40 g below, the
// description (+0xcc, size 24 g) wrapped at the screen width - 20 g (the
// camera's +0x168; +0x16c for the 90 / 270 display rotations), then 5 g
// below its last line.
void CGameObject::drawInfoscreen(CVector2& pos) {
    CTextRender* t = tr();
    if (!t) return;
    const float g = gui();
    t->drawStringPixel(pos.x, pos.y, g * 40.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, "%s", mName.c_str());
    pos.y = pos.y + g * 40.0f;
    CCamera* cam = CEngine::getSingletonPtr()->getCurrentCamera();
    const float w = cam ? static_cast<float>(cam->mOrientation == 0x5a || cam->mOrientation == 0x10e ? cam->mHeight
                                                                                                     : cam->mWidth)
                        : 0.0f;
    const float bottom = t->drawStringPixelBounded(pos.x, pos.y, g * 24.0f, 1.0f, 1.0f, 1.0f, 1.0f, w - g * 20.0f, 0.0f,
                                                   mDesc, 999999);
    pos.y = bottom + g * 5.0f;
}

// Ghidra 0x3af8b4. After the name and description, 28 g apart: speed,
// acceleration, turn rate (all x the speed factor), health (or CTxtLoc
// 0x7e, indestructible), weight with its class, firing range, the armour
// against lasers / projectiles (and the driver's, when commanded) as
// "-x %", CTxtLoc 0x8a for units that turn towards their target (+0x113c),
// the hack resistance (or 0x199, not hackable), the harvester lines in
// GameType 3, level and XP, the modifiers line, the weapons header and
// every weapon's drawInfoscreen (vtable +0x30), and CTxtLoc 0x7d when
// invulnerable.
void CUnit::drawInfoscreen(CVector2& pos) {
    CGameObject::drawInfoscreen(pos);
    if (!tr()) return;
    line28(pos, "%s: %.1f m/s", C(0x74), static_cast<double>(mMaxVelocity * mSpeedAndAccFactor));
    line28(pos, "%s: %.1f m/s^2", C(0x75), static_cast<double>(mAcceleration * mSpeedAndAccFactor));
    line28(pos, "%s: %.0f \xb0/s", C(0x76), static_cast<double>(mTurnrate * mSpeedAndAccFactor * 180.0f / CMath::PI));
    if (!isDestroyable())
        line28(pos, C(0x7e));
    else
        line28(pos, "%s: %.0f of %.0f", C(0x77), static_cast<double>(mHealth),
               static_cast<double>(mMaxHealth * mHealthFactor));
    std::string weightClass;
    if (mWeightClass >= 0 && mWeightClass <= 2) weightClass = CTxtLoc::get().getS(0x215 + mWeightClass);
    line28(pos, "%s: %.1f t (%s)", C(0x211), static_cast<double>(mWeight), weightClass.c_str());
    line28(pos, "%s: %.0f m - %.0f m", C(0x78), static_cast<double>(fdlibm::sqrtf(mMinFiringRangeSquared)),
           static_cast<double>(fdlibm::sqrtf(mMaxFiringRangeSquared)));
    line28(pos, "%s: -%.0f %%", C(0x79),
           static_cast<double>(Real(100.0f) - getArmorfactorVsLaser() / mArmorFactor * 100.0f));
    line28(pos, "%s: -%.0f %%", C(0x7a),
           static_cast<double>(Real(100.0f) - getArmorfactorVsProjectile() / mArmorFactor * 100.0f));
    if (isCommanded()) {
        line28(pos, C(0x89));
        line28(pos, "%s: -%.0f %%", C(0x7b),
               static_cast<double>(Real(100.0f) - getArmorfactorDriver() / mArmorFactor * 100.0f));
    }
    if (mCanHaveDriver) line28(pos, C(0x8a));
    if (!isHackable())
        line28(pos, C(0x199));
    else
        line28(pos, "%s %.0f %%", C(0x19a), static_cast<double>(mHackResistance * 100.0f));
    if (CEngine::getSingletonPtr()->mGameType == 3 && isResourceGatherer()) {
        line28(pos, "%s", C(0x27d));
        line28(pos, "%s %.0f / %.0f", C(0x27f), static_cast<double>(mCurrentResGathered),
               static_cast<double>(mResMaxCapacity));
        line28(pos, "%s %.1f / s", C(0x27e), static_cast<double>(mResGatheringRate));
    }
    // "XP: %.0f" shows getXP; the original passes getXPNextLevel too (unused)
    line28(pos, "%s: %i, XP: %.0f", C(0x8b), getLevel(), static_cast<double>(getXP()),
           static_cast<double>(getXPNextLevel()));
    line28(pos, "  %s: %s %.0f%% %s %.0f%% %s %.0f%% %s %.0f%% %s %.0f%%", C(0xb3), C(0x90),
           static_cast<double>(mArmorFactor * 100.0f), C(0x8f), static_cast<double>(mDamageFactor * 100.0f), C(0x77),
           static_cast<double>(mHealthFactor * 100.0f), C(0x78), static_cast<double>(mRangeFactor * 100.0f), C(0x74),
           static_cast<double>(mSpeedAndAccFactor * 100.0f));
    line28(pos, "%s:", C(0x7c));
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->drawInfoscreen(pos);
    if (mInvulnerable) line28(pos, C(0x7d));
}

// Ghidra 0x3d3f6c. The name (+0x08) at x + 10 g, size 28 g; 28 g below, at
// x + 20 g and size 24 g, the range ("%s: %.0f m - %.0f m", min and max),
// the DPS (+0x4c; not for damage type 3) and the damage type (CTxtLoc
// 0x1bf, then 0x1bb..0x1be by +0x88, "unknown" otherwise); 24 g below, at
// x + 10 [!] (not scaled), the health (+0x7c of +0x80 x the unit's health
// factor) or CTxtLoc 0x7e (not destroyable, +0x84); then 24 g down.
void CWeapon::drawInfoscreen(CVector2& pos) {
    CTextRender* t = tr();
    if (!t) return;
    const float g = gui();
    std::string type = mDamageType >= 0 && mDamageType <= 3 ? CTxtLoc::get().getS(0x1bb + mDamageType)
                                                            : std::string("unknown");
    t->drawStringPixel(pos.x + g * 10.0f, pos.y, g * 28.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, "%s", mName.c_str());
    pos.y = pos.y + g * 28.0f;
    const double rmin = fdlibm::sqrtf(mMinRangeSq), rmax = fdlibm::sqrtf(mMaxRangeSq);
    if (mDamageType == 3)
        t->drawStringPixel(pos.x + g * 20.0f, pos.y, g * 24.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0,
                           "%s: %.0f m - %.0f m, %s: %s", C(0x78), rmin, rmax, C(0x1bf), type.c_str());
    else
        t->drawStringPixel(pos.x + g * 20.0f, pos.y, g * 24.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0,
                           "%s: %.0f m - %.0f m, DPS: %.1f, %s: %s", C(0x78), rmin, rmax,
                           static_cast<double>(mDPS), C(0x1bf), type.c_str());
    pos.y = pos.y + g * 24.0f;
    if (!mDestroyable)
        t->drawStringPixel(pos.x + 10.0f, pos.y, g * 24.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, C(0x7e));
    else
        t->drawStringPixel(pos.x + 10.0f, pos.y, g * 24.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, "%s: %.0f of %.0f", C(0x77),
                           static_cast<double>(mHealth), static_cast<double>(mMaxHealth * mUnit->mHealthFactor));
    pos.y = pos.y + g * 24.0f;
}

// Ghidra 0x136250: in GameType 3 a drop-off for the owner (vtable +0xdc,
// any resource) says so (CTxtLoc 0x280).
void CBuilding::drawInfoscreen(CVector2& pos) {
    CGameObject::drawInfoscreen(pos);
    if (!tr()) return;
    if (CEngine::getSingletonPtr()->mGameType == 3 && isResourceDropOff(mOwner, -1)) line28(pos, "%s", C(0x280));
}

// Ghidra 0x13b348 (CBuildingProduction::drawInfoscreen = CBuilding's)
void CBuildingHQ::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    if (tr()) productList(pos, 0x73, {{0x42, 1}});
}

// Ghidra 0x1415b4
void CBuildingProductionLight::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    if (tr()) productList(pos, 0x73, {{0x46, 9}, {0x45, 6}, {0x136, 0x15}, {0x13a, 0x13}, {0x138, 0x14}});
}

// Ghidra 0x140b9c
void CBuildingProductionMedium::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    if (tr()) productList(pos, 0x73, {{0x40, 2}, {0x41, 3}, {0xa6, 0xc}, {0x134, 0x11}});
}

// Ghidra 0x14024c
void CBuildingProductionAdvTech::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    if (tr()) productList(pos, 0x73, {{0xaa, 0xe}, {0xa8, 0xd}, {300, 0x10}, {0x132, 0x12}});
}

// Ghidra 0x1423b8
void CBuildingProductionLarge::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    if (tr()) productList(pos, 0x73, {{0x43, 5}, {0x47, 10}, {0x3f, 8}});
}

// Ghidra 0x14b360. [!] The times are CBuilding::getProductionTime of the op
// numbers 0..4 taken as unit types, as in the original.
void CBuildingSpecop::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    if (tr()) productList(pos, 0x7f, {{0x80, 0}, {0x82, 1}, {0x81, 2}, {0x11f, 3}, {0x120, 4}});
}

// Ghidra 0x145908: three lists of the 33 techs, 18 g lines at x + 20 [!]
// (not scaled): researched (CTxtLoc 0x20a; "%s: %s" name and description,
// green), researchable (0x20b; "%s (%.0f s): %s" with the research time,
// blue), the others (0x20c; red: "name (time s): description <0x20d> " and
// " [name] " for every required tech not researched yet).
void CBuildingScienceLab::drawInfoscreen(CVector2& pos) {
    CBuilding::drawInfoscreen(pos);
    CTextRender* t = tr();
    if (!t) return;
    const float g = gui();
    const CTechManager& tm = CTechManager::get();
    CPlayer* o = mOwner;
    line28(pos, C(0x20a));
    for (unsigned i = 0; i < 0x21; ++i) {
        if (!o->hasTech(i)) continue;
        t->drawStringPixel(pos.x + 20.0f, pos.y, g * 18.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0, "%s: %s",
                           tm.getTechName(static_cast<int>(i)).c_str(), tm.getTechDesc(static_cast<int>(i)).c_str());
        pos.y = pos.y + g * 18.0f;
    }
    line28(pos, C(0x20b));
    for (unsigned i = 0; i < 0x21; ++i) {
        if (o->hasTech(i) || !o->isTechResearchable(i)) continue;
        t->drawStringPixel(pos.x + 20.0f, pos.y, g * 18.0f, 0.5f, 0.5f, 1.0f, 1.0f, 0, "%s (%.0f s): %s",
                           tm.getTechName(static_cast<int>(i)).c_str(),
                           static_cast<double>(o->getTechResearchTime(i)), tm.getTechDesc(static_cast<int>(i)).c_str());
        pos.y = pos.y + g * 18.0f;
    }
    line28(pos, C(0x20c));
    for (unsigned i = 0; i < 0x21; ++i) {
        if (o->hasTech(i) || o->isTechResearchable(i)) continue;
        std::ostringstream s;
        s << tm.getTechName(static_cast<int>(i)) << " (" << f2i(o->getTechResearchTime(i)) << " s): "
          << tm.getTechDesc(static_cast<int>(i)) << " " << CTxtLoc::get().getS(0x20d) << " ";
        for (int r : tm.getRequiredTechs(static_cast<int>(i)))
            if (!o->hasTech(static_cast<unsigned>(r))) s << " [" << tm.getTechName(r) << "] ";
        t->drawStringPixel(pos.x + 20.0f, pos.y, g * 18.0f, 1.0f, 0.5f, 0.5f, 1.0f, 0, "%s", s.str().c_str());
        pos.y = pos.y + g * 18.0f;
    }
}

}  // namespace py
