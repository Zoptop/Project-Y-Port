// stubs_cunit.cpp - stubs for the methods CUnit calls that belong to other
// packages and did not exist yet. Each logs once (notPorted); the owners
// replace them with the real ports (and delete them from here).
#include "core/CUtils.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"

namespace py {

// ---- CLevel (coordinator): the resource grid of game type 3
int CLevel::getResCellIndex(const CVector2&) {   // Ghidra 0x2dd6a8
    notPorted("CLevel::getResCellIndex");
    return -1;
}
Real CLevel::getResValue(int) {   // Ghidra 0x2dd994
    notPorted("CLevel::getResValue");
    return 0.0f;
}
Real CLevel::consumeRes(const CVector2&, Real) {   // Ghidra 0x2dd4cc
    notPorted("CLevel::consumeRes");
    return 0.0f;
}
bool CLevel::findRes(const CVector2&, const CVector2&, Real, CVector2*) {   // Ghidra 0x2dee28
    notPorted("CLevel::findRes");
    return false;
}

// ---- CPlayer (players agent): fog of war
void CPlayer::drawFOWCircleTeam(const CVector2&, Real) { notPorted("CPlayer::drawFOWCircleTeam"); }

}  // namespace py
