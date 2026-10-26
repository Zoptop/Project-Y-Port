// CPlayer.cpp - see CPlayer.h.
#include "game/CPlayer.h"

#include <set>

#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CSector.h"
#include "game/CUnit.h"

namespace py {

namespace {
void notPorted(const char* fn) {
    static std::set<std::string> seen;
    if (seen.insert(fn).second) CUtils::log("NOT PORTED YET: %s", fn);
}
}  // namespace

// Ghidra 0x375124
CPlayer::CPlayer(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team)
    : mName(name), mType(type), mColor(color), mId(id), mEngine(CEngine::getSingletonPtr()), mTeam(team) {
    mGame = mEngine->mGame;
}

// Ghidra 0x373a1c: counts the sectors it owns; fog of war.
void CPlayer::timeUpdate() {
    mOwnedSectors = 0;
    for (const auto& s : mEngine->mGame->getLevel()->sectors())
        if (s.second->mOwner == this) ++mOwnedSectors;
    if (mEngine->mUseFOW) {
        FOWtimeupdate();
        updateFOWtexture();
    }
}

// Ghidra 0x374fe4
void CPlayer::addUnit(CUnit* unit) {
    mUnits.insert(unit->mId);
    if (unit->mType == UNIT_ROBOBRAIN) mBrains.insert(unit->mId);
}

// Ghidra 0x375034
void CPlayer::removeUnit(unsigned id) {
    mUnits.erase(id);
    mBrains.erase(id);
}

// Ghidra 0x3732bc: sum of the brain cap bonus of its buildings.
int CPlayer::getBrainCap() {
    int cap = 0;
    CGame* game = mEngine->mGame;
    for (unsigned id : game->objectList()) {
        CBuilding* b = game->getAsBuilding(id);
        if (b && b->mOwner == this) cap += b->getBrainCapBonus();
    }
    return cap;
}

// Ghidra 0x3738d4
bool CPlayer::hasTech(unsigned tech) {
    if (tech > 0x20) {
        CUtils::log("p %i hasTech %i", mId, tech);
        CUtils::log("!! TECH INVALID !!");
        return false;
    }
    return mHasTech[tech];
}

// Ghidra 0x373430
void CPlayer::calcChecksum() {
    CUtils::updateChecksum(mHasTech);
    CUtils::updateChecksum(mCurrentResources);
}

// Ghidra 0x372bc8
void CPlayer::consumeResources(Real amount) {
    const Real r = mCurrentResources - amount;
    mCurrentResources = r < 0.0f ? Real(0.0f) : r;
}

// Ghidra 0x372fd0
void CPlayer::setCaptureAbility(bool can) {
    CUtils::log("Player %i can capture set to %i", mId, can ? 1 : 0);
    mCanCapture = can;
}

// Ghidra 0x375068: collects every player (itself included) with the same
// team; "has team mates" means at least two members.
void CPlayer::initTeamMembers() {
    mTeamMembers.clear();
    unsigned count = 0;
    for (const auto& p : mEngine->mGame->players()) {
        if (p.second->mTeam == mTeam) {
            mTeamMembers.push_back(p.second.get());
            ++count;
        }
    }
    mHasTeamMates = count >= 2;
}

// Ghidra 0x374454. Techs (ETechs): see docs/re/sim_phase4.md 7.
Real CPlayer::getBonus(int bonus, int weightClass, int damageType, bool building, bool flag) {
    const Real k1_2 = bitsf(0x3f99999a), k1_15 = bitsf(0x3f933333), k1_25 = bitsf(0x3fa00000);
    switch (bonus) {
    case 0:
    case 1:
        if (hasTech(2)) return k1_25;
        if (hasTech(1) && weightClass == 2) return k1_2;
        return 1.0f;
    case 2:
        if (hasTech(0x1f)) return bitsf(0x3fb33333);
        if (hasTech(0xb)) return k1_2;
        return 1.0f;
    case 3:
        if (hasTech(4)) return k1_15;
        if (hasTech(3) && weightClass == 0) return bitsf(0x3f8ccccd);
        return 1.0f;
    case 4:
        if (!hasTech(6)) {
            if (hasTech(5) && static_cast<unsigned>(damageType - 1) < 2) return bitsf(0x3f8ccccd);
            return 1.0f;
        }
        if (hasTech(0x20) && damageType == 0) return k1_25;
        return k1_15;
    case 5:
        if (hasTech(8)) return bitsf(0x3fa66666);
        if (hasTech(7) && weightClass == 0) return k1_2;
        return 1.0f;
    case 6:
        if (hasTech(10)) return bitsf(0x3f59999a);
        if (hasTech(9) && flag) return bitsf(0x3f666666);
        return 1.0f;
    case 7:
        return hasTech(0xc) ? k1_25 : Real(1.0f);
    case 8:
        if (hasTech(0x1e)) return 25.0f;
        if (hasTech(0xf)) return 15.0f;
        if (hasTech(0xe)) return 10.0f;
        if (hasTech(0xd)) return 5.0f;
        return 0.0f;
    case 9:
        return hasTech(0x14) ? Real(1.5f) : Real(1.0f);
    case 10:
        return hasTech(0x15) ? Real(bitsf(0x3f333333)) : Real(1.0f);
    case 11:
        // [!] as compiled: 0 with tech 23, or with tech 22 for heavy units; else 1
        if (hasTech(0x17)) return 0.0f;
        if (hasTech(0x16) && weightClass == 2) return 0.0f;
        return 1.0f;
    case 12:
        return hasTech(0x18) ? Real(4.0f) : Real(1.0f);
    case 13:
        if (hasTech(0x1b)) return 0.5f;
        if (hasTech(0x1a)) return bitsf(0x3f333333);
        if (hasTech(0x19)) return bitsf(0x3f59999a);
        return 1.0f;
    case 14:
        if (hasTech(0x12)) return k1_2;
        if ((hasTech(0x11) && static_cast<unsigned>(damageType - 1) < 2) || (hasTech(0x10) && building))
            return k1_15;
        return 1.0f;
    default:
        return 0.0f;
    }
}

// ------------------------------------------------------------------ TODO(4.x CPlayer)

void CPlayer::FOWtimeupdate() { notPorted("CPlayer::FOWtimeupdate"); }
void CPlayer::reInit() { notPorted("CPlayer::reInit"); }
bool CPlayer::isTechResearchable(unsigned) {
    notPorted("CPlayer::isTechResearchable");
    return false;
}
void CPlayer::addTech(unsigned tech) {
    notPorted("CPlayer::addTech");
    if (tech < kNumTechs) mHasTech[tech] = true;
}
Real CPlayer::getTechResearchTime(unsigned) {
    notPorted("CPlayer::getTechResearchTime");
    return 0.0f;
}
void CPlayer::techResearched(unsigned, CBuilding*) { notPorted("CPlayer::techResearched"); }
void CPlayer::updateScore() { notPorted("CPlayer::updateScore"); }
Real CPlayer::getTechResearchCost(unsigned) {
    notPorted("CPlayer::getTechResearchCost");
    return 0.0f;
}
void CPlayer::addRadar(const CVector2&, Real, unsigned) { notPorted("CPlayer::addRadar"); }
void CPlayer::removeRadar(unsigned) { notPorted("CPlayer::removeRadar"); }
void CPlayer::lost(bool) { notPorted("CPlayer::lost"); }
void CPlayer::won() { notPorted("CPlayer::won"); }

}  // namespace py
