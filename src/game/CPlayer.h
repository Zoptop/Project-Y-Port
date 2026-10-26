// CPlayer.h - port of ProjectY::CPlayer (0x120 bytes) and the CPlayerKI
// (computer player) shell.
//
// Player 0 is the neutral player; 1..n play. Ids of the player's units and
// brains are kept in ordered sets (by id). Techs are a 33-bit vector<bool>
// (ETechs, docs/re/sim_phase4.md 7).
//
// Ghidra: CPlayer::CPlayer 0x375124, getBonus 0x374454, calcChecksum 0x373430.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "core/Real.h"
#include "render/Math3D.h"

namespace py {

class CBuilding;
class CEngine;
class CGame;
class CUnit;

class CPlayer {
public:
    enum EType { TYPE_HUMAN = 0, TYPE_NEUTRAL = 1, TYPE_AI = 2 };
    static constexpr unsigned kNumTechs = 33;

    CPlayer(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team);   // Ghidra 0x375124
    virtual ~CPlayer() = default;

    // ---- vtable (slot)
    virtual void timeUpdate();                                 // +0x00 Ghidra 0x373a1c
    virtual void addUnit(CUnit* unit);                         // +0x04 Ghidra 0x374fe4
    virtual void removeUnit(unsigned id);                      // +0x08 Ghidra 0x375034
    virtual void unitProduced(CUnit*, CBuilding*) {}           // +0x0c
    virtual bool isAI() const { return false; }                // +0x1c
    virtual void FOWtimeupdate();                              // +0x28 TODO(4.x FOW)
    virtual void updateFOWtexture() {}                         // +0x24 (render)
    // +0x30: reveal a circle in the team's fog of war (CUnit::timeUpdate);
    // stub in stubs_cunit.cpp until the FOW is ported
    virtual void drawFOWCircleTeam(const CVector2& pos, Real radius);
    virtual void reInit();                                     // +0x40
    virtual bool isFriend(const CPlayer* other) const { return mTeam == other->mTeam; }   // +0x44
    virtual int getBrainCap();                                 // +0x48 Ghidra 0x3732bc
    virtual int getCurrentBrains() { return static_cast<int>(mBrains.size()); }   // +0x4c
    virtual bool directUnitControlDisabled(unsigned) { return false; }           // +0x50
    virtual void setTeam(unsigned team) { mTeam = team; }      // +0x54
    virtual bool hasTech(unsigned tech);                       // +0x58 Ghidra 0x3738d4
    virtual bool isTechResearchable(unsigned tech);            // +0x5c
    virtual std::vector<int> getResearchableTechs();           // +0x60 (ETechs)
    virtual void addTech(unsigned tech);                       // +0x68
    virtual Real getTechResearchTime(unsigned tech);           // +0x6c
    virtual void techResearched(unsigned tech, CBuilding* lab);   // +0x70
    virtual void calcChecksum();                               // +0x7c Ghidra 0x373430
    virtual void updateScore();                                // +0x80
    virtual Real getCurrentResources() { return mCurrentResources; }   // +0x84
    virtual void consumeResources(Real amount);                // +0x88 Ghidra 0x372bc8
    virtual void setCaptureAbility(bool can);                  // +0x8c Ghidra 0x372fd0
    virtual Real getTechResearchCost(unsigned tech);           // +0x90
    virtual void addRadar(const CVector2& pos, Real radius, unsigned id);   // +0x94
    virtual void removeRadar(unsigned id);                     // +0x98

    // ---- non-virtual
    bool isNeutral() const { return mType == TYPE_NEUTRAL; }  // Ghidra 0x373fd8
    void initTeamMembers();                                    // Ghidra 0x375068
    // Ghidra 0x374454: multiplier from researched techs (bonus 0..14),
    // for a unit of weight class / damage type / building flag.
    Real getBonus(int bonus, int weightClass, int damageType, bool building, bool flag);
    void lost(bool surrender);
    // Ghidra 0x374368: own units (objType 1, from mUnits) or buildings
    // (objType 2, from the game's object list) of type subType; else 0.
    int getGameobjectCount(unsigned objType, unsigned subType);
    void won();
    const std::string& getName() const { return mName; }

    std::string mName;                  // +0x04
    CGame* mGame = nullptr;             // +0x08
    int mType = 0;                      // +0x0c EType
    Vec3 mColor;                        // +0x10
    unsigned mId = 0;                   // +0x1c
    CEngine* mEngine = nullptr;         // +0x20
    CVector2 mStartPosition;            // +0x24  "set start <p> position x y"
    int mOwnedSectors = 0;              // +0x2c counted each step (timeUpdate)
    std::set<unsigned> mUnits;          // +0x30 ids of all units
    std::set<unsigned> mBrains;         // +0x48 ids of brains (type 1); size = +0x5c
    unsigned mTeam = 0;                 // +0x78
    bool mLost = false;                 // +0x8c "PlayerLost"
    bool mWon = false;                  // +0x8d "PlayerWon"
    std::vector<CPlayer*> mTeamMembers; // +0x90
    bool mHasTeamMates = false;         // +0x9c more than one member in the team
    std::vector<bool> mHasTech = std::vector<bool>(kNumTechs, false);   // +0xb8
    Real mCurrentResources = 1000.0f;   // +0xe4
    bool mCanCapture = true;            // +0xe8
    unsigned mUnitsDestroyed = 0;       // +0xec  (score)
    unsigned mUnitsBuilt = 0;           // +0xf0
    unsigned mHighestBrainLevel = 1;    // +0xf4
    Real mXPReceived = 0.0f;            // +0xf8
    Real mDamageReceived = 0.0f;        // +0xfc
    Real mDamageDone = 0.0f;            // +0x100
    unsigned mSpecOpCount = 0;          // +0x104
    unsigned mCratesPickedUp = 0;       // +0x108
    Real mSectorHoldTime = 0.0f;        // +0x10c
    unsigned mNumTouches = 0;           // +0x110
};

// The computer player (0x1d8 bytes). TODO(4.x AI): CPlayerKI/CKISquad.
class CPlayerKI : public CPlayer {
public:
    CPlayerKI(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team, int networkFlag)
        : CPlayer(name, type, color, id, team), mNetworkFlag(networkFlag) {}
    bool isAI() const override { return true; }
    int mKIMode = 0;                    // +0x14c "set kimode", "set kimainmode", aioption type 4
    // Script targets (aioption types 1..3), Ghidra 0x2d8018..0x2d806c
    void setTargetSector(unsigned id) { mTargetSector = id; mTargetType = 0; }
    void setTargetUnit(unsigned id) { mTargetUnit = id; mTargetType = 1; }
    void setTargetPosition(const CVector2& p) { mTargetType = 2; mTargetPosition = p; }
    void setMainMode(unsigned mode) { mKIMode = static_cast<int>(mode); }
    int mTargetType = 0;                // +0x1c4 0 sector, 1 unit, 2 position
    unsigned mTargetSector = 0;         // +0x1c8
    unsigned mTargetUnit = 0;           // +0x1cc
    CVector2 mTargetPosition;           // +0x1d0
    int mNetworkFlag = 0;
};

}  // namespace py
