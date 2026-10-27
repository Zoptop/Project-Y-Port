// CDialMenuButtons.cpp - the objects' addDialButtons (vtable +0x80), which
// fill a CDialMenu. In the original they are members of the game classes;
// they only read the object, so the port keeps them on the UI side and
// dispatches on the dynamic type (most derived override first), keeping the
// original function names and addresses in the comments.
#include <cstdio>

#include "core/CConfig.h"
#include "core/FloatBits.h"
#include "game/CBuildingHQ.h"
#include "game/CBuildingProduction.h"
#include "game/CBuildingProductionAdvTech.h"
#include "game/CBuildingProductionLarge.h"
#include "game/CBuildingProductionLight.h"
#include "game/CBuildingProductionMedium.h"
#include "game/CBuildingScienceLab.h"
#include "game/CBuildingSpecop.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CTechManager.h"
#include "game/CUnit.h"
#include "game/CUnitRoboBrain.h"
#include "game/CUnitTurret.h"
#include "ui/CDialMenu.h"

namespace py {

namespace {

const Vec3 kDialGreen{bitsf(0x3e4ccccd), bitsf(0x3f4ccccd), bitsf(0x3e4ccccd)};    // (0.2, 0.8, 0.2)
const Vec3 kDialYellow{bitsf(0x3f4ccccd), bitsf(0x3f4ccccd), bitsf(0x3e4ccccd)};   // (0.8, 0.8, 0.2)

bool available(int unitType) { return CConfig::get().mUnitAvailable[unitType]; }

// CGameObject::addDialButtons (Ghidra 0x181d54)
void gameObjectAddDialButtons(CDialMenu* m) { m->addButton(DIAL_CANCEL, 5, ""); }

// CBuildingProduction::addDialButtons (Ghidra 0x13ca04)
void productionAddDialButtons(CBuildingProduction* b, CDialMenu* m) {
    m->addButton(DIAL_CANCEL, 5, "");
    if (b->mUnitType != b->mNextUnitType) m->addButton(DIAL_APPLY, 5, "");
    m->mColor = kDialGreen;
}

void autoCallButton(bool autoCall, CDialMenu* m) {
    m->addButton(autoCall ? DIAL_AUTOCALL_OFF : DIAL_AUTOCALL_ON, 5, "");
}

// one rank button of CUnitRoboBrain::addDialButtons: factor < 2 only;
// label "<factor>%  >>  <factor + bonus>%", value = clamp(trunc((factor - 1) * 5), 0, 5)
void rankButton(CDialMenu* m, int type, float factor, float bonus) {
    if (!(factor < 2.0f)) return;
    const float a = factor * 100.0f;
    const float nb = factor + bonus;
    const float b = nb * 100.0f;
    char buf[100];
    std::snprintf(buf, sizeof(buf), "%.0f%%%%  >>  %.0f%%%%", static_cast<double>(a), static_cast<double>(b));
    const float v = (factor - 1.0f) * 5.0f;
    int value;
    if (v > 5.0f) value = 5;
    else if (v < 0.0f) value = 0;
    else value = f2i(v);
    m->addButton(type, value, buf);
}

void unitAddDialButtons(CUnit* u, CDialMenu* m);

// CUnitRoboBrain::addDialButtons (Ghidra 0x3c0a24)
void roboBrainAddDialButtons(CUnitRoboBrain* u, CDialMenu* m) {
    m->addButton(DIAL_CANCEL, 5, "");
    if (!u->isAlive()) return;
    const float step = u->mXPBonusPerLevel;
    rankButton(m, DIAL_RANK_ARMOR, u->mArmorFactorBase, step + step);
    rankButton(m, DIAL_RANK_DAMAGE, u->mDamageFactorBase, step * 0.75f);
    rankButton(m, DIAL_RANK_RANGE, u->mRangeFactorBase, step * bitsf(0x3ecccccd));
    rankButton(m, DIAL_RANK_SPEED, u->mSpeedAndAccFactorBase, step);
    rankButton(m, DIAL_RANK_HEALTH, u->mHealthFactorBase, step * 1.5f);
    m->mColor = kDialYellow;
}

// CUnitTurret::addDialButtons (Ghidra 0x3d0440): the weapon choice
void turretAddDialButtons(CUnitTurret* u, CDialMenu* m) {
    if (u->hasUpgrade()) {
        unitAddDialButtons(u, m);
        return;
    }
    const unsigned w = u->mNewWeapontype;
    bool add1c = true;
    if (w == 1) {
        m->addButton(0x1e, 5, "");
        add1c = u->mNewWeapontype != 10;
    } else {
        m->addButton(0x1d, 5, "");
        if (u->mNewWeapontype != 8) {
            m->addButton(0x1e, 5, "");
            add1c = u->mNewWeapontype != 10;
        }
    }
    if (add1c) m->addButton(0x1c, 5, "");
    if (CEngine::getSingletonPtr()->mUseFOW && u->mNewWeapontype != 0xe) m->addButton(0x1f, 5, "");
    m->addButton(DIAL_CANCEL, 5, "");
}

// CUnit::addDialButtons (Ghidra 0x3a2b2c): a commanded vehicle shows its brain's menu
void unitAddDialButtons(CUnit* u, CDialMenu* m) {
    if (u->isAlive() && u->isCommanded()) {
        CGame* game = CEngine::getSingletonPtr()->mGame;
        if (CUnit* c = game ? game->getAsUnit(u->mCommander) : nullptr) {
            addDialButtons(c, m);
            return;
        }
    }
    m->addButton(DIAL_CANCEL, 5, "");
}

}  // namespace

void addDialButtons(CGameObject* object, CDialMenu* m) {
    if (!object || !m) return;
    // ---- buildings
    if (auto* b = dynamic_cast<CBuildingHQ*>(object)) {   // CBuildingHQ::addDialButtons (Ghidra 0x13b1e4)
        productionAddDialButtons(b, m);
        if (!b->mProductionStopped) {
            m->addButton(DIAL_STOP_PRODUCTION, 5, "");
            if (!b->mProductionStopped) return;
        }
        m->addButton(DIAL_START_PRODUCTION, 5, "");
        return;
    }
    if (auto* b = dynamic_cast<CBuildingProductionLight*>(object)) {   // Ghidra 0x141318
        productionAddDialButtons(b, m);
        if (available(6)) m->addButton(6, 5, "");
        if (available(9)) m->addButton(7, 5, "");
        if (available(0x15)) m->addButton(0x20, 5, "");
        if (available(0x13)) m->addButton(0x21, 5, "");
        if (available(0x14)) m->addButton(0x22, 5, "");
        autoCallButton(b->mAutoCallClosestBrain, m);
        return;
    }
    if (auto* b = dynamic_cast<CBuildingProductionMedium*>(object)) {   // Ghidra 0x140960
        productionAddDialButtons(b, m);
        if (available(2)) m->addButton(1, 5, "");
        if (available(0xc)) m->addButton(8, 5, "");
        if (available(3)) m->addButton(2, 5, "");
        if (available(0x11)) m->addButton(0x24, 5, "");
        autoCallButton(b->mAutoCallClosestBrain, m);
        return;
    }
    if (auto* b = dynamic_cast<CBuildingProductionLarge*>(object)) {   // Ghidra 0x141cac
        productionAddDialButtons(b, m);
        if (available(5)) m->addButton(3, 5, "");
        if (available(8)) m->addButton(9, 5, "");
        if (available(10)) m->addButton(4, 5, "");
        if (available(0x18)) m->addButton(DIAL_HARVESTER, 5, "");
        autoCallButton(b->mAutoCallClosestBrain, m);
        return;
    }
    if (auto* b = dynamic_cast<CBuildingProductionAdvTech*>(object)) {   // CBuildingProductionAdvTech::addDialButtons
        productionAddDialButtons(b, m);
        if (available(0xd)) m->addButton(0xb, 5, "");
        if (available(0xe)) m->addButton(10, 5, "");
        if (available(0x10)) m->addButton(0xc, 5, "");
        if (available(0x12)) m->addButton(0x23, 5, "");
        autoCallButton(b->mAutoCallClosestBrain, m);
        return;
    }
    if (auto* b = dynamic_cast<CBuildingProduction*>(object)) {
        productionAddDialButtons(b, m);
        return;
    }
    if (auto* b = dynamic_cast<CBuildingSpecop*>(object)) {   // CBuildingSpecop::addDialButtons (Ghidra 0x148f90)
        m->addButton(DIAL_CANCEL, 5, "");
        if (CEngine::getSingletonPtr()->mUnitAvailabilityMode != 8) m->addButton(DIAL_SPECOP_AIRSTRIKE, 5, "");
        m->addButton(DIAL_SPECOP_CRATE, 5, "");
        m->addButton(DIAL_SPECOP_SOLDIER, 5, "");
        m->addButton(DIAL_SPECOP_MINE, 5, "");
        autoCallButton(b->mAutoCallClosestBrain, m);
        m->mColor = kDialGreen;
        return;
    }
    if (auto* b = dynamic_cast<CBuildingScienceLab*>(object)) {   // CBuildingScienceLab::addDialButtons (Ghidra 0x143eb8)
        m->addButton(DIAL_CANCEL, 5, "");
        CPlayer* owner = b->mOwner;
        if (b->mNextTech != -1 && b->mNextTech != b->mCurrentTech && owner &&
            owner->isTechResearchable(static_cast<unsigned>(b->mNextTech)))
            m->addButton(DIAL_APPLY, 5, "");
        m->mColor = kDialGreen;
        for (int t = 0; t < CTechManager::kNumTechs; ++t)
            if (owner && !owner->hasTech(static_cast<unsigned>(t)) &&
                owner->isTechResearchable(static_cast<unsigned>(t)) && b->mCurrentTech != t && b->mNextTech != t)
                m->addButton(DIAL_TECH_FIRST + t, 5, "");
        autoCallButton(b->mAutoCallClosestBrain, m);
        m->addButton(b->mRandomResearchEnabled ? DIAL_RANDOM_RESEARCH_OFF : DIAL_RANDOM_RESEARCH_ON, 5, "");
        m->mColor = kDialGreen;
        return;
    }
    // ---- units
    if (auto* u = dynamic_cast<CUnitRoboBrain*>(object)) {
        roboBrainAddDialButtons(u, m);
        return;
    }
    if (auto* u = dynamic_cast<CUnitTurret*>(object)) {
        turretAddDialButtons(u, m);
        return;
    }
    if (auto* u = dynamic_cast<CUnit*>(object)) {
        unitAddDialButtons(u, m);
        return;
    }
    gameObjectAddDialButtons(m);
}

}  // namespace py
