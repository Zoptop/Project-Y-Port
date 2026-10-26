// CUnitCrate.cpp - see CUnitCrate.h.
#include "game/CUnitCrate.h"

#include "core/CConfig.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// Ghidra 0x3ba0a0 (switch layout checked against the disassembly). Up to 100
// draws of an action 0..10 (CRngSafe 0x15 x 11) until one that is enabled
// for the map (CConfig crate actions) and applicable:
//   0 XP bonus (+50 XP, not at level 25)   1 instant repair (heal 100000)
//   2 invulnerability for 60 s             5 production booster (every
//     commanded production building of the finder's owner: -60 s x speed)
//   7 reinforcements: a unit (1, 2, 3, 8, 12, 14) dropped by a bomber
//   8 reinforcements: 1..3 units (1, 6, 9) dropped by a bomber
//   9 resource blast (resource game type only)
// 3, 4, 6 and 10 do nothing (draw again). The finder's owner counts a crate
// in any case. The message particle is not ported.
void CUnitCrate::doSuprise(CUnit* finder) {
    CGame* game = mEngine->mGame;
    const CConfig& cfg = CConfig::get();
    unsigned tries = 0;
    for (;;) {
        const unsigned action = f2u(Real(CRngSafe::GetUniform(0x15)) * 11.0f);
        ++tries;
        bool done = false;
        if (action < CConfig::kCrateActions && cfg.mCrateActionAvailable[action]) {
            switch (action) {
            case 0:
                if (finder->getLevel() != 25) {
                    finder->gainXP(50.0f, true);   // "XP Bonus!"
                    done = true;
                }
                break;
            case 1:
                if (!(finder->heal(100000.0f, nullptr) == 0.0f)) done = true;   // "Instant repair!"
                break;
            case 2:
                if (finder->isDestroyable()) {
                    finder->makeInvulnerable(60.0f);   // "Invulnerability!"
                    done = true;
                }
                break;
            case 5:   // "Production Booster!"
                for (unsigned id : game->objectList()) {
                    CBuilding* b = game->getAsBuilding(id);
                    if (b && b->hasProduction() && b->mOwner == finder->mOwner && b->isCommanded()) {
                        const Real boost = Real(mEngine->getSpeedFactor()) * 60.0f;
                        b->mProductionCountdown = b->mProductionCountdown - boost;
                    }
                }
                done = true;
                break;
            case 7: {   // "Reinforcements!"
                unsigned type;
                do {
                    if (Real(CRngSafe::GetUniform(0x16)) > bitsf(0x3e99999a)) {
                        if (Real(CRngSafe::GetUniform(0x17)) > bitsf(0x3e99999a))
                            type = UNIT_SCORPION;
                        else
                            type = Real(CRngSafe::GetUniform(0x18)) > bitsf(0x3e99999a) ? UNIT_MINEDROPPER : UNIT_SPIDER;
                    } else {
                        if (Real(CRngSafe::GetUniform(0x19)) > bitsf(0x3e99999a))
                            type = UNIT_SPOTTER;
                        else
                            type = Real(CRngSafe::GetUniform(0x1a)) > bitsf(0x3e99999a) ? UNIT_REPAIR : UNIT_ROBOBRAIN;
                    }
                } while (!cfg.mUnitAvailable[type]);
                // CGame::addBomber(x, y, owner, 2, type, 1, -1, -1)
                notPorted("CGame::addBomber (crate)");   // TODO(4.x bombers)
                done = true;
                break;
            }
            case 8: {   // "Reinforcements!"
                unsigned type;
                do {
                    if (Real(CRngSafe::GetUniform(0x1b)) > bitsf(0x3e4ccccd))   // 0.2
                        type = Real(CRngSafe::GetUniform(0x1c)) > bitsf(0x3ecccccd) ? UNIT_SOLDIERSUIT : UNIT_BOMBSUIT;
                    else
                        type = UNIT_ROBOBRAIN;
                } while (!cfg.mUnitAvailable[type]);
                const unsigned count = f2u(Real(CRngSafe::GetUniform(0x1d)) * 3.0f + 1.0f);
                (void)count;
                // CGame::addBomber(x, y, owner, 2, type, count, -1, -1)
                notPorted("CGame::addBomber (crate)");   // TODO(4.x bombers)
                done = true;
                break;
            }
            case 9:
                if (mEngine->mGameType == 3) {   // "Resource Blast!"
                    // CLevel::addResource(x, y, 1, 50) and the level's resource refresh (vtable +4)
                    notPorted("CLevel::addResource (crate)");   // TODO(4.x resources)
                    done = true;
                }
                break;
            default:
                break;
            }
        }
        if (done || tries > 99) break;
    }
    finder->mOwner->mCratesPickedUp = finder->mOwner->mCratesPickedUp + 1;
}

// Ghidra 0x3ba6b0
void CUnitCrate::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitCrateMaxHealth");
    loadConfigSettings("Crate");
    mType = UNIT_CRATE;
    mFlags = 0x80400;
    mMaxVelocity = 0.0f;
    mAcceleration = 0.0f;
    mTurnrate = 0.0f;
    cfg.updateConfigValue(mPosition.z, "UnitCrateZOffset");
    mArmorDriverFactor = 1.0f;
    mArmorFactorVsLaser = 1.0f;
    mArmorFactorVsProjectile = 1.0f;
    mCanHaveDriver = false;
    mRenderFlag120a = false;
    mDrawBrain = false;
    mIsStatic = true;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3ba804. [!] Only with at least two units in its (near) grid cell;
// the bounding radius is the pick-up distance.
void CUnitCrate::timeUpdate() {
    CGame* game = mEngine->mGame;
    const Real x = mPosition.x, y = mPosition.y;
    CGridCell* cell = game->getLevel()->getGridCell(CVector2(x, y));
    if (cell->units.size() >= 2) {
        for (size_t i = 0; i < cell->units.size(); ++i) {
            CUnit* u = game->getAsUnit(cell->units[i]);
            if (!u || u == this || u->isNeutral() || !u->isAlive()) continue;
            const Real dx = x - u->mPosition.x;
            const Real dy = y - u->mPosition.y;
            if (mBoundingRadius * mBoundingRadius > dx * dx + dy * dy) {
                mUnitstate = UNITSTATE_READYTODELETE;
                doSuprise(u);
                return;
            }
        }
    }
    CUnit::timeUpdate();
}

}  // namespace py
