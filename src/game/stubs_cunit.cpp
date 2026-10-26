// stubs_cunit.cpp - stubs for the methods CUnit calls that belong to other
// packages and did not exist yet. Each logs once (notPorted); the owners
// replace them with the real ports (and delete them from here).
#include "core/CUtils.h"
#include "game/CPlayer.h"

namespace py {

// ---- CPlayer (players agent): fog of war
void CPlayer::drawFOWCircleTeam(const CVector2&, Real) { notPorted("CPlayer::drawFOWCircleTeam"); }

}  // namespace py
