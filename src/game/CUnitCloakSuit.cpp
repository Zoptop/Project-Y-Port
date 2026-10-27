// CUnitCloakSuit.cpp - see CUnitCloakSuit.h.
#include "game/CUnitCloakSuit.h"

#include "core/CConfig.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CPlayer.h"
#include "game/CWeapon.h"

namespace py {

// Ghidra 0x3b8ae4
void CUnitCloakSuit::createWeapons() {
    CVector3 mountPoint;   // [?] uninitialised in the original if the key is missing
    CConfig::get().updateConfigValue(mountPoint, "UnitCloakSuitWeaponMountPoint");
    createWeapon(WEAPON_MINILASER, mountPoint);
}

// Ghidra 0x3b8f8c
void CUnitCloakSuit::init(unsigned playerId, const CVector2& position, Real rotation, unsigned sectorId) {
    const CConfig& cfg = CConfig::get();
    cfg.updateConfigValue(mMaxHealth, "UnitCloakSuitMaxHealth");
    loadConfigSettings("CloakSuit");
    getSubMeshData("cloaker");   // +0x1c8/+0x1cc
    mFlags = 0x88e57;   // (Ghidra shows a string address: the literal from the disassembly)
    mType = UNIT_CLOAKSUIT;
    cfg.updateConfigValue(mHackResistance, "UnitCloakSuitHackResistance");
    cfg.updateConfigValue(mMaxVelocity, "UnitCloakSuitMaxVelocity");
    cfg.updateConfigValue(mAcceleration, "UnitCloakSuitAcceleration");
    cfg.updateConfigValue(mTurnrate, "UnitCloakSuitTurnrate");
    cfg.updateConfigValue(mPosition.z, "UnitCloakSuitZOffset");
    cfg.updateConfigValue(mArmorDriverFactor, "UnitCloakSuitArmorDriverFactor");
    cfg.updateConfigValue(mArmorFactorVsLaser, "UnitCloakSuitArmorFactorVsLaser");
    cfg.updateConfigValue(mArmorFactorVsProjectile, "UnitCloakSuitArmorFactorVsProjectile");
    mBrainDrawOffset = CVector3(0.0f, 0.0f, bitsf(0x3f67ef9e));
    mCanHaveDriver = false;
    mInitVal12ec = 5;
    mInitVal12f0 = 2;
    CUnit::init(playerId, position, rotation, sectorId);
}

// Ghidra 0x3b9550. Neutral or dead: visible (5).
void CUnitCloakSuit::timeUpdate() {
    if (!isNeutral() && isAlive()) {
        Real t = mCloakTimeout;
        if (t > 0.0f) {
            t = t - mEngine->mFrameDt;
            mCloakTimeout = t;
        }
        if (t < 0.0f) mCloakTimeout = 0.0f;
    } else {
        mCloakTimeout = 5.0f;
    }
    CUnit::timeUpdate();
}

// Ghidra 0x3b9608: cloaked, only friends of the owner (who cannot attack
// anyway) get past to CUnit::isAttackable.
bool CUnitCloakSuit::isAttackable(CGameObject* attacker) {
    if (mCloakTimeout == 0.0f && !mOwner->isFriend(attacker->mOwner)) return false;
    return CUnit::isAttackable(attacker);
}

void CUnitCloakSuit::calcChecksum() {
    CUnit::calcChecksum();
    CUtils::updateChecksum(mCloakTimeout);
}

}  // namespace py
