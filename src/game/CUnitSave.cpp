// CUnitSave.cpp - savegames: CGameObject::save, CUnit::save/load and the
// unit classes that add lines (CUnitCloakSuit, CUnitLandmine,
// CUnitMineDropper, CUnitRoboBrain, CUnitShieldTank, CUnitSpotter, CUnitTank,
// CUnitTurret). The other unit types use CUnit's (tools/re/vtables.py).
//
// A unit is loaded by CGame::load (Ghidra 0x17d3e0): it reads the lines
// CGameObject::save wrote, creates the unit with CUnitFactory::createUnit
// (id, type, owner, position, yaw, no driver; the weapons are not created
// while CEngine +0x1cc "loading" is set), sets z, then calls load. The lines
// come back in exactly this order (CUtils::safeReadFromFile does not search).
#include <cstdio>

#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "game/CUnitCloakSuit.h"
#include "game/CUnitLandmine.h"
#include "game/CUnitMineDropper.h"
#include "game/CUnitRoboBrain.h"
#include "game/CUnitShieldTank.h"
#include "game/CUnitSpotter.h"
#include "game/CUnitTank.h"
#include "game/CUnitTurret.h"
#include "game/CWeapon.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x3875c4
void CSceneObject::save(FILE* f) {
    writef(f, "create SceneObject %i\n", I(mSceneId));
    writef(f, "Type %i\n", mSceneType);
    writef(f, "ReadyToDelete %i\n", mReadyToDelete ? 1 : 0);
}

// Ghidra 0x38761c
void CSceneObject::load(FILE* f) { CUtils::safeReadFromFile(f, "ReadyToDelete", &mReadyToDelete); }

// Ghidra 0x1814ac
void CGameObject::save(FILE* f) {
    writef(f, "create GameObject %i\n", I(mId));
    writef(f, "Type %i\n", mCategory);
    writef(f, "SubType %i\n", mType);
    writef(f, "Position %f %f %f\n", D(mPosition.x), D(mPosition.y), D(mPosition.z));
    writef(f, "Yaw %f\n", D(mYaw));
    writef(f, "EnableRender %i\n", mEnableRender ? 1 : 0);
    writef(f, "OwnerID %i\n", I(mOwner->mId));
}

// Ghidra 0x3b4bb4
void CUnit::save(FILE* f) {
    CGameObject::save(f);
    writef(f, "MoveOnlyToFiringRange %i\n", mMoveOnlyToFiringRange ? 1 : 0);
    writef(f, "Nextwaypoint %i\n", mNextwaypoint);
    writef(f, "NextCurrentTargetUpdateTime %f\n", D(mCurrentTargetUpdateCountdown));
    writef(f, "NumStraightPath %i\n", mNumStraightPath);
    for (int i = 0; i < mNumStraightPath; ++i)
        writef(f, "StraightPathPoint %f %f %f\n", D(mStraightPath[3 * i]), D(mStraightPath[3 * i + 1]),
                D(mStraightPath[3 * i + 2]));
    writef(f, "Unitstate %i\n", mUnitstate);
    writef(f, "CommandedUnit %i\n", I(mCommandedUnit));
    if (!mCommandedBuilding)
        writef(f, "CommandedBuilding 0\n");
    else
        writef(f, "CommandedBuilding %i\n", I(mCommandedBuilding->mId));
    writef(f, "Commander %i\n", I(mCommander));
    writef(f, "WaypointLastDistance %f\n", D(mWaypointLastDistance));
    writef(f, "WayPointFailTime %f\n", D(mWayPointFailTime));
    writef(f, "WaypointWaitingForRetry %i\n", mWaypointWaitingForRetry ? 1 : 0);
    writef(f, "Velocity %f\n", D(mVelocity));
    writef(f, "OrderDestination %f %f\n", D(mOrderDestination.x), D(mOrderDestination.y));
    writef(f, "OrderYaw %f\n", D(mOrderYaw));
    writef(f, "OrderTarget %i\n", I(mOrderTarget));
    writef(f, "CurrentTarget %i\n", I(mCurrentTarget));
    writef(f, "DeleteUnitTimeout %f\n", D(mDeleteUnitTimeout));
    writef(f, "Health %f\n", D(mHealth));
    writef(f, "UnitIsHealing %i\n", mUnitIsHealing ? 1 : 0);
    writef(f, "UnitIsHealingCountdown %f\n", D(mUnitIsHealingCountdown));
    writef(f, "CurrentParticleSystemInvul %i\n", I(mParticleInvul));
    writef(f, "CurrentParticleSystemStun %i\n", I(mParticleStun));
    writef(f, "CurrentParticleSystemHeal %i\n", I(mParticleHeal));
    writef(f, "CurrentParticleSystemUpgrade %i\n", I(mParticleUpgrade));
    writef(f, "CurrentParticleSystemSmoke %i\n", I(mParticleSmoke));
    writef(f, "CurrentParticleSystemResGather %i\n", I(mParticleResGather));
    writef(f, "AttackNeutralUnitActive %i\n", mAttackNeutralUnitActive ? 1 : 0);
    writef(f, "OrientationIsWanted %i\n", mOrientationIsWanted ? 1 : 0);
    writef(f, "WantedOrientation %f\n", D(mWantedOrientation));
    writef(f, "WeaponParked %i\n", mWeaponParked ? 1 : 0);
    writef(f, "RenderUnitEnabled %i\n", mRenderUnitEnabled ? 1 : 0);
    writef(f, "UnitOrder %i\n", mUnitOrder);
    writef(f, "BuildingToEnter %i\n", I(mEngine->mGame->getGameObjectID(mBuildingToEnter)));
    // the "base" factors (the ones the modifiers start from each step)
    writef(f, "ArmorFactor %f\n", D(mArmorFactorBase));
    writef(f, "DamageFactor %f\n", D(mDamageFactorBase));
    writef(f, "HealthFactor %f\n", D(mHealthFactorBase));
    writef(f, "RangeFactor %f\n", D(mRangeFactorBase));
    writef(f, "RangeFactorSq %f\n", D(mRangeFactorSqBase));
    writef(f, "SpeedAndAccFactor %f\n", D(mSpeedAndAccFactorBase));
    writef(f, "LookAtSet %i\n", mLookAtSet ? 1 : 0);
    writef(f, "IsUnderAttack %i\n", mIsUnderAttack ? 1 : 0);
    writef(f, "IsUnderAttackCountdown %f\n", D(mIsUnderAttackCountdown));
    writef(f, "Invulnerable %i\n", mInvulnerable ? 1 : 0);
    writef(f, "InvulnerableCountdown %f\n", D(mInvulnerableCountdown));
    writef(f, "Stunned %i\n", mStunned ? 1 : 0);
    writef(f, "StunCountdown %f\n", D(mStunCountdown));
    writef(f, "CurrentResGathered %f\n", D(mCurrentResGathered));
    writef(f, "GatherResourceDestination %f %f\n", D(mGatherResourceDestination.x),
            D(mGatherResourceDestination.y));
    writef(f, "LastAttackerID %i\n", I(mLastAttackerID));
    if (!mLastAttackingPlayer)
        writef(f, "LastAttackingPlayerID 0\n");
    else
        writef(f, "LastAttackingPlayerID %i\n", I(mLastAttackingPlayer->mId));
    writef(f, "DrawBrain %i\n", mDrawBrain ? 1 : 0);
    writef(f, "BrainDrawOffset %f %f %f\n", D(mBrainDrawOffset.x), D(mBrainDrawOffset.y), D(mBrainDrawOffset.z));
    writef(f, "DontChase %i\n", mDontChase ? 1 : 0);
    writef(f, "ScriptControlled %i\n", mScriptControlled ? 1 : 0);
    writef(f, "ChaseStartPosition %f %f\n", D(mChaseStartPosition.x), D(mChaseStartPosition.y));
    writef(f, "DefendedUnit %i\n", I(mDefendedUnit));
    writef(f, "HoldPositionOnArrival %i\n", mHoldPositionOnArrival ? 1 : 0);
    writef(f, "PrimaryDamageType %i\n", mPrimaryDamageType);
    writef(f, "NumWeapons %i\n", static_cast<int>(mWeaponIds.size()));
    for (unsigned id : mWeaponIds)
        if (CWeapon* w = mEngine->mGame->getWeapon(id)) w->save(f);
}

// Ghidra 0x3b0f14. Each read stops the load when its key does not match.
void CUnit::load(FILE* f) {
    CGameObject::load(f);
    CGame* g = mEngine->mGame;
    if (!CUtils::safeReadFromFile(f, "MoveOnlyToFiringRange", &mMoveOnlyToFiringRange)) return;
    if (!CUtils::safeReadFromFile(f, "Nextwaypoint", &mNextwaypoint)) return;
    if (!CUtils::safeReadFromFile(f, "NextCurrentTargetUpdateTime", &mCurrentTargetUpdateCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "NumStraightPath", &mNumStraightPath)) return;
    for (int i = 0; i < mNumStraightPath; ++i) {
        CVector3 p;
        if (!CUtils::safeReadFromFile(f, "StraightPathPoint", &p)) return;
        if (i < kMaxStraightPath) {   // [PC] the original writes past the array
            mStraightPath[3 * i] = p.x;
            mStraightPath[3 * i + 1] = p.y;
            mStraightPath[3 * i + 2] = p.z;
        }
    }
    if (!CUtils::safeReadFromFile(f, "Unitstate", &mUnitstate)) return;
    if (!CUtils::safeReadFromFile(f, "CommandedUnit", &mCommandedUnit)) return;
    unsigned building = 0;
    if (!CUtils::safeReadFromFile(f, "CommandedBuilding", &building)) return;
    if (building != 0) mCommandedBuilding = g->getAsBuilding(building);
    if (mUnitstate == 6) {
        // a unit inside a vehicle or building is in no grid cell: createUnit
        // put it in (it keeps its cell pointers, as in the original)
        g->getLevel()->removeUnitFromCells(mGridCell, mId);
        g->getLevel()->removeUnitFromCellsFar(mGridCellFar, mId);
    }
    if (!CUtils::safeReadFromFile(f, "Commander", &mCommander)) return;
    if (!CUtils::safeReadFromFile(f, "WaypointLastDistance", &mWaypointLastDistance)) return;
    if (!CUtils::safeReadFromFile(f, "WayPointFailTime", &mWayPointFailTime)) return;
    if (!CUtils::safeReadFromFile(f, "WaypointWaitingForRetry", &mWaypointWaitingForRetry)) return;
    if (!CUtils::safeReadFromFile(f, "Velocity", &mVelocity)) return;
    if (!CUtils::safeReadFromFile(f, "OrderDestination", &mOrderDestination)) return;
    if (!CUtils::safeReadFromFile(f, "OrderYaw", &mOrderYaw)) return;
    if (!CUtils::safeReadFromFile(f, "OrderTarget", &mOrderTarget)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentTarget", &mCurrentTarget)) return;
    if (!CUtils::safeReadFromFile(f, "DeleteUnitTimeout", &mDeleteUnitTimeout)) return;
    if (!CUtils::safeReadFromFile(f, "Health", &mHealth)) return;
    if (!CUtils::safeReadFromFile(f, "UnitIsHealing", &mUnitIsHealing)) return;
    if (!CUtils::safeReadFromFile(f, "UnitIsHealingCountdown", &mUnitIsHealingCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystemInvul", &mParticleInvul)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystemStun", &mParticleStun)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystemHeal", &mParticleHeal)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystemUpgrade", &mParticleUpgrade)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystemSmoke", &mParticleSmoke)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentParticleSystemResGather", &mParticleResGather)) return;
    if (!CUtils::safeReadFromFile(f, "AttackNeutralUnitActive", &mAttackNeutralUnitActive)) return;
    if (!CUtils::safeReadFromFile(f, "OrientationIsWanted", &mOrientationIsWanted)) return;
    if (!CUtils::safeReadFromFile(f, "WantedOrientation", &mWantedOrientation)) return;
    if (!CUtils::safeReadFromFile(f, "WeaponParked", &mWeaponParked)) return;
    if (!CUtils::safeReadFromFile(f, "RenderUnitEnabled", &mRenderUnitEnabled)) return;
    if (!CUtils::safeReadFromFile(f, "UnitOrder", &mUnitOrder)) return;
    unsigned toEnter = 0;
    if (!CUtils::safeReadFromFile(f, "BuildingToEnter", &toEnter)) return;
    if (toEnter != 0) mBuildingToEnter = g->getAsBuilding(toEnter);
    if (!CUtils::safeReadFromFile(f, "ArmorFactor", &mArmorFactorBase)) return;
    if (!CUtils::safeReadFromFile(f, "DamageFactor", &mDamageFactorBase)) return;
    if (!CUtils::safeReadFromFile(f, "HealthFactor", &mHealthFactorBase)) return;
    if (!CUtils::safeReadFromFile(f, "RangeFactor", &mRangeFactorBase)) return;
    if (!CUtils::safeReadFromFile(f, "RangeFactorSq", &mRangeFactorSqBase)) return;
    if (!CUtils::safeReadFromFile(f, "SpeedAndAccFactor", &mSpeedAndAccFactorBase)) return;
    if (!CUtils::safeReadFromFile(f, "LookAtSet", &mLookAtSet)) return;
    if (!CUtils::safeReadFromFile(f, "IsUnderAttack", &mIsUnderAttack)) return;
    if (!CUtils::safeReadFromFile(f, "IsUnderAttackCountdown", &mIsUnderAttackCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "Invulnerable", &mInvulnerable)) return;
    if (!CUtils::safeReadFromFile(f, "InvulnerableCountdown", &mInvulnerableCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "Stunned", &mStunned)) return;
    if (!CUtils::safeReadFromFile(f, "StunCountdown", &mStunCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "CurrentResGathered", &mCurrentResGathered)) return;
    if (!CUtils::safeReadFromFile(f, "GatherResourceDestination", &mGatherResourceDestination)) return;
    if (!CUtils::safeReadFromFile(f, "LastAttackerID", &mLastAttackerID)) return;
    unsigned attacker = 0;
    if (!CUtils::safeReadFromFile(f, "LastAttackingPlayerID", &attacker)) return;
    // [!] 0 is "none": an attack by the neutral player (id 0) is forgotten
    mLastAttackingPlayer = attacker == 0 ? nullptr : g->getPlayer(attacker);
    if (!CUtils::safeReadFromFile(f, "DrawBrain", &mDrawBrain)) return;
    if (!CUtils::safeReadFromFile(f, "BrainDrawOffset", &mBrainDrawOffset)) return;
    if (!CUtils::safeReadFromFile(f, "DontChase", &mDontChase)) return;
    if (!CUtils::safeReadFromFile(f, "ScriptControlled", &mScriptControlled)) return;
    if (!CUtils::safeReadFromFile(f, "ChaseStartPosition", &mChaseStartPosition)) return;
    if (!CUtils::safeReadFromFile(f, "DefendedUnit", &mDefendedUnit)) return;
    if (!CUtils::safeReadFromFile(f, "HoldPositionOnArrival", &mHoldPositionOnArrival)) return;
    int damageType = 0;
    if (!CUtils::safeReadFromFile(f, "PrimaryDamageType", &damageType)) return;
    mPrimaryDamageType = damageType;
    unsigned numWeapons = 0;
    if (!CUtils::safeReadFromFile(f, "NumWeapons", &numWeapons)) return;
    for (unsigned i = 0; i < numWeapons; ++i) {
        unsigned id = 0, type = 0;
        CVector3 mount;
        if (!CUtils::safeReadFromFile(f, "create", "Weapon")) return;
        if (!CUtils::safeReadFromFile(f, "ID", &id)) return;
        if (!CUtils::safeReadFromFile(f, "Type", &type)) return;
        if (!CUtils::safeReadFromFile(f, "MountPoint", &mount)) return;
        CWeaponFactory::getSingletonPtr()->createWeapon(static_cast<int>(type), this, mount, id);
        if (CWeapon* w = g->getWeapon(id)) {
            mWeaponIds.push_back(id);
            w->load(f);
        }
    }
    calcUnitFiringRangesFromWeapons();
    mLoadedVal1494 = 0;
    mVal1498 = 0.0f;
    mLoadedVal1488 = 0;
    mLoadedVal148c = 0;
    mVal1490 = -1;
}

// Ghidra 0x3b919c / 0x3b91e8
void CUnitCloakSuit::save(FILE* f) {
    CUnit::save(f);
    writef(f, "CloakTimeout %f\n", D(mCloakTimeout));
}
void CUnitCloakSuit::load(FILE* f) {
    CUnit::load(f);
    CUtils::safeReadFromFile(f, "CloakTimeout", &mCloakTimeout);
}

// Ghidra 0x3beb58 / 0x3beb88
void CUnitLandmine::save(FILE* f) {
    CUnit::save(f);
    writef(f, "IsDetected %i\n", mIsDetected ? 1 : 0);
}
void CUnitLandmine::load(FILE* f) {
    CUnit::load(f);
    CUtils::safeReadFromFile(f, "IsDetected", &mIsDetected);
}

// Ghidra 0x3bf420 / 0x3bf46c
void CUnitMineDropper::save(FILE* f) {
    CUnit::save(f);
    writef(f, "TimeToNextDrop %f\n", D(mTimeToNextDrop));
}
void CUnitMineDropper::load(FILE* f) {
    CUnit::load(f);
    CUtils::safeReadFromFile(f, "TimeToNextDrop", &mTimeToNextDrop);
}

// Ghidra 0x3c35a0
void CUnitRoboBrain::save(FILE* f) {
    CUnit::save(f);
    writef(f, "XP %f\n", D(mXP));
    writef(f, "XPForNextLevel %f\n", D(mXPForNextLevel));
    writef(f, "Level %i\n", I(mLevel));
    writef(f, "LevelUpsPending %i\n", I(mLevelUpsPending));
    writef(f, "UpgradeAutomated %i\n", mUpgradeAutomated);
    writef(f, "LastAddedRank %i\n", mLastAddedRank);
    writef(f, "SendAutoUpgradeCmdCountdown %f\n", D(mSendAutoUpgradeCmdCountdown));
}

// Ghidra 0x3c36bc
void CUnitRoboBrain::load(FILE* f) {
    CUnit::load(f);
    if (!CUtils::safeReadFromFile(f, "XP", &mXP)) return;
    if (!CUtils::safeReadFromFile(f, "XPForNextLevel", &mXPForNextLevel)) return;
    if (!CUtils::safeReadFromFile(f, "Level", &mLevel)) return;
    if (!CUtils::safeReadFromFile(f, "LevelUpsPending", &mLevelUpsPending)) return;
    if (!CUtils::safeReadFromFile(f, "UpgradeAutomated", &mUpgradeAutomated)) return;
    if (!CUtils::safeReadFromFile(f, "LastAddedRank", &mLastAddedRank)) return;
    CUtils::safeReadFromFile(f, "SendAutoUpgradeCmdCountdown", &mSendAutoUpgradeCmdCountdown);
}

// Ghidra 0x3c61f8 / 0x3c6290
void CUnitShieldTank::save(FILE* f) {
    CUnit::save(f);
    writef(f, "ShieldEnabled %i\n", mShieldEnabled ? 1 : 0);
    writef(f, "ShieldStrength %f\n", D(mShieldStrength));
    writef(f, "ShieldSetpoint %f\n", D(mShieldSetpoint));
}
void CUnitShieldTank::load(FILE* f) {
    CUnit::load(f);
    if (!CUtils::safeReadFromFile(f, "ShieldEnabled", &mShieldEnabled)) return;
    if (!CUtils::safeReadFromFile(f, "ShieldStrength", &mShieldStrength)) return;
    CUtils::safeReadFromFile(f, "ShieldSetpoint", &mShieldSetpoint);
}

// Ghidra 0x3caabc
void CUnitSpotter::save(FILE* f) {
    CUnit::save(f);
    writef(f, "UAVposition %f %f %f\n", D(mUAVposition.x), D(mUAVposition.y), D(mUAVposition.z));
    writef(f, "UAVyaw %f\n", D(mUAVyaw));
    writef(f, "UAVpitch %f\n", D(mUAVpitch));
    writef(f, "UAVroll %f\n", D(mUAVroll));
    writef(f, "UAVvelocity %f\n", D(mUAVvelocity));
    writef(f, "UAVzSetpoint %f\n", D(mUAVzSetpoint));
    writef(f, "UAVstate %i\n", mUAVstate);
    writef(f, "UAVThrusterPS %i\n", I(mUAVThrusterPS));
}

// Ghidra 0x3cac70
void CUnitSpotter::load(FILE* f) {
    CUnit::load(f);
    if (!CUtils::safeReadFromFile(f, "UAVposition", &mUAVposition)) return;
    if (!CUtils::safeReadFromFile(f, "UAVyaw", &mUAVyaw)) return;
    if (!CUtils::safeReadFromFile(f, "UAVpitch", &mUAVpitch)) return;
    if (!CUtils::safeReadFromFile(f, "UAVroll", &mUAVroll)) return;
    if (!CUtils::safeReadFromFile(f, "UAVvelocity", &mUAVvelocity)) return;
    if (!CUtils::safeReadFromFile(f, "UAVzSetpoint", &mUAVzSetpoint)) return;
    int state = 0;
    if (!CUtils::safeReadFromFile(f, "UAVstate", &state)) return;
    mUAVstate = state;
    CUtils::safeReadFromFile(f, "UAVThrusterPS", &mUAVThrusterPS);
}

// Ghidra 0x3cda10 / 0x3cda8c
void CUnitTank::save(FILE* f) {
    CUnit::save(f);
    writef(f, "TransformState %f\n", D(mTransformState));
    writef(f, "DeployDelayTimer %f\n", D(mDeployDelayTimer));
}
void CUnitTank::load(FILE* f) {
    CUnit::load(f);
    if (!CUtils::safeReadFromFile(f, "TransformState", &mTransformState)) return;
    CUtils::safeReadFromFile(f, "DeployDelayTimer", &mDeployDelayTimer);
}

// Ghidra 0x3cf63c
void CUnitTurret::save(FILE* f) {
    CUnit::save(f);
    writef(f, "SectorID %i\n", I(mSector->mId));
    writef(f, "BuildWeaponCountdown %f\n", D(mBuildWeaponCountdown));
    writef(f, "NewWeapontype %i\n", I(mNewWeapontype));
    writef(f, "ParentBuilding %i\n", I(mParentBuilding));
}

// Ghidra 0x3cf6e0
void CUnitTurret::load(FILE* f) {
    CUnit::load(f);
    unsigned sector = 0;
    if (!CUtils::safeReadFromFile(f, "SectorID", &sector)) return;
    mSector = mEngine->mGame->getLevel()->getSector(sector);
    if (!CUtils::safeReadFromFile(f, "BuildWeaponCountdown", &mBuildWeaponCountdown)) return;
    if (!CUtils::safeReadFromFile(f, "NewWeapontype", &mNewWeapontype)) return;
    CUtils::safeReadFromFile(f, "ParentBuilding", &mParentBuilding);
}

}  // namespace py
