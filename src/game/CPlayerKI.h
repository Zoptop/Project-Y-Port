// CPlayerKI.h - port of ProjectY::CPlayerKI (0x1d8 bytes), the computer
// player ("KI" = Kuenstliche Intelligenz). Background: docs/re/ai.md (the RE
// package's notes/ai.md); the decompile is the authority.
//
// Every 10th CPlayer::timeUpdate call (one AI tick) it runs its top-level
// state machine (mStep): 0 influence of units, 1 goal influence, 2 strategy,
// 3 brain commands (sub-machine mBrainStep 0,1,2,5,3,4), 4 squads (mSquadStep
// 6..9), 5 special operations, 6 harvesters. The first tick builds the
// influence grid (oneTimeInit). It acts only through player commands (the
// cmd* helpers of CPlayer) and CRng; a passive AI (network client that is not
// the host) builds the grid and does nothing else.
//
// Runtime budget. The original gives each AI tick 2 ms of wall-clock time:
// timeUpdate stores CUtils::getTimer() (+0x12c) and every step and sub-step
// first asks isCurrentStepRuntimeOk() (getTimer() - start < 2.0); when it
// fails, the tick ends and the next tick resumes at the same (sub-)step
// (updateSquads also remembers the squad to resume with, +0x1bc). That makes
// the AI machine-speed dependent. The port's isCurrentStepRuntimeOk() always
// returns true, i.e. every AI tick runs all steps to completion, keeping the
// step variables and resume points as in the original. This is also exactly
// what the original does under the emulator (tools/emu): its clock_gettime
// is a constant, so getTimer() - start is always 0 < 2. The resume branches
// are kept (and are dead code), so a deterministic budget could be plugged in
// later in isCurrentStepRuntimeOk.
//
// Pointer-ordered sets. The squads (+0x178) and the collected units (+0x190)
// are std::set<CKISquad*> / std::set<CUnit*> in the original, ordered by heap
// address. Under the oracle's bump allocator that is allocation order, so the
// port orders them by allocation serial (CKISquad::mAllocSerial,
// CSceneObject::mAllocSerial, both from py::allocSerial()).
//
// The influence grid has 200 x 200 world-unit cells (CInfluenceMapCell,
// index x + W*y).
#pragma once

#include <cstdio>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "core/Real.h"
#include "game/CPlayer.h"
#include "render/Math3D.h"

namespace py {

class CBuilding;
class CKISquad;
class CTexture;
class CUnit;

// Orders the original's pointer-keyed sets by allocation serial (see above).
struct CKISquadLess {
    bool operator()(const CKISquad* a, const CKISquad* b) const;
};
struct CKIUnitLess {
    bool operator()(const CUnit* a, const CUnit* b) const;
};

class CPlayerKI : public CPlayer {
public:
    void save(FILE* f) override;             // +0x10 Ghidra 0x2d99a4 (CGameSave.cpp)
    void load(FILE* f) override;             // +0x14 Ghidra 0x2d9a68
    // EInfluenceMaps (updateInfluenceMapAtIndex; 1 BLOCKED / 2 FULLYBLOCKED are
    // written directly by updateInfluenceBlocked, 11 COUNT is debug only)
    enum EInfluenceMaps {
        IM_BLOCKED = 1, IM_FULLYBLOCKED = 2, IM_OWNUNIT = 3, IM_ENEMYUNIT = 4, IM_NEUTRALUNIT = 5,
        IM_INFLUENCE = 6, IM_TENSION = 7, IM_VULNERABILITY = 8, IM_GOAL = 9, IM_SUM = 10
    };

    // CPlayerKI::CInfluenceMapCell (0x28 bytes)
    struct CInfluenceMapCell {
        Real blocked = 0.0f;           // +0x00 number of blocked occupancy cells
        bool passable = false;         // +0x04 (FULLYBLOCKED) blocked < 1/(sx*sy)
        Real own = 0.0f;               // +0x08 OWNUNIT: own and allied units
        Real enemy = 0.0f;             // +0x0c ENEMYUNIT
        Real neutral = 0.0f;           // +0x10 NEUTRALUNIT
        Real influence = 0.0f;         // +0x14 INFLUENCE = own - enemy
        Real tension = 0.0f;           // +0x18 TENSION = own + enemy
        Real vulnerability = 0.0f;     // +0x1c VULNERABILITY = tension - |influence|
        Real goal = 0.0f;              // +0x20 GOAL
        Real sum = 0.0f;               // +0x24 SUM
    };

    CPlayerKI(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team, bool passive);   // Ghidra 0x2d8074
    ~CPlayerKI() override;

    // ---- CPlayer overrides
    void timeUpdate() override;                                        // +0x00 Ghidra 0x2d9b48
    void addUnit(CUnit* unit) override;                                // +0x04 Ghidra 0x2d98c8
    void removeUnit(unsigned id) override;                             // +0x08 Ghidra 0x2d9914
    void unitProduced(CUnit* unit, CBuilding* building) override;      // +0x0c Ghidra 0x2d6590
    bool isAI() const override { return true; }                        // +0x1c Ghidra 0x2d3f18
    void initFOWTexture() override;                                    // +0x20 Ghidra 0x2d3f7c (texture fow_ki.png only, CPlayerDraw.cpp)
    void bindFOWtexture(unsigned textureUnit) override;                // +0x38 Ghidra 0x2d3fc0 (CPlayerDraw.cpp)
    void updateFOWtexture() override {}                                // +0x24 Ghidra 0x2d3f2c
    void FOWtimeupdate() override {}                                   // +0x28 Ghidra 0x2d3f38
    void drawFOWCirclePlayer(const CVector2&, Real) override {}        // +0x2c Ghidra 0x2d3f20
    Real getVisibilityValue(const CVector2&, bool*) override { return 1.0f; }   // +0x34 Ghidra 0x2d3f30
    void reInit() override {}                                          // +0x40 Ghidra 0x2d3f3c
    void directUnitControlDisabled(unsigned id) override;              // +0x50 Ghidra 0x2d3f40
    void techResearched(unsigned tech, CBuilding* lab) override;       // +0x70 Ghidra 0x2d9d3c

    // ---- script interface (CScriptAIOption) and map settings
    void setTargetSector(unsigned id) { mTargetSector = id; mTargetMode = 0; }   // Ghidra 0x2d8018
    void setTargetUnit(unsigned id) { mTargetUnit = id; mTargetMode = 1; }       // Ghidra 0x2d8028
    void setTargetPosition(const CVector2& p) { mTargetMode = 2; mTargetPosition = p; }   // Ghidra 0x2d8038
    void setMainMode(unsigned mode) { mKIMode = static_cast<int>(mode); }        // Ghidra 0x2d806c

    // ---- the AI (public for the squads and for tests)
    bool isCurrentStepRuntimeOk() const { return true; }               // Ghidra 0x2d49a8, see the file comment
    void oneTimeInit();                                                // Ghidra 0x2d97cc
    void updateInfluenceBlocked();                                     // Ghidra 0x2d49f8
    bool updateInfluenceUnits();                                       // Ghidra 0x2d7cf4
    bool updateInfluenceGoal();                                        // Ghidra 0x2d7328
    bool updateStrategy();                                             // Ghidra 0x2d8a84
    bool updateBrainCommands();                                        // Ghidra 0x2d5550
    bool updateSquads();                                               // Ghidra 0x2d9268
    bool updateSpecOp();                                               // Ghidra 0x2d82d8
    bool updateHarvester();                                            // Ghidra 0x2d5434
    void updateInfluenceMapAtIndex(Real value, int index, int map);    // Ghidra 0x2d4be0
    void updateInfluenceMapWithFalloff(Real value, Real falloff, Real radius, const CVector2& pos, int map);   // Ghidra 0x2d6e40
    int getInfluenceIndexFromMapPosition(const CVector2& pos) const;   // Ghidra 0x2d4df4
    void getInfluenceIndicesFromMapPosition(const CVector2& pos, int* x, int* y) const;   // Ghidra 0x2d6d00
    int getClosestResource(const CVector2& pos, CVector2* out);        // Ghidra 0x2d4f08
    CInfluenceMapCell& cell(int index) { return mInfluenceMap[static_cast<size_t>(index)]; }
    const std::vector<CInfluenceMapCell>& influenceMap() const { return mInfluenceMap; }
    const std::set<CKISquad*, CKISquadLess>& squads() const { return mSquads; }
    // The script's target as a position (shared by strategy and spec ops):
    // false when the target unit or sector does not exist.
    bool getTargetPosition(CVector2* out) const;

    // ---- fields (original offsets)
    int mStep = 0;                      // +0x120 top-level step 0..6
    int mBrainStep = 0;                 // +0x124 updateBrainCommands sub-step
    int mSquadStep = 6;                 // +0x128 updateSquads sub-step 6..9
    Real mRuntimeStart = 0.0f;          // +0x12c CUtils::getTimer() at the tick start (unused, see above)
    std::set<unsigned> mKIBrains;       // +0x130 ids of own RoboBrains
    bool mInitDone = false;             // +0x148 oneTimeInit ran
    int mKIMode = 0;                    // +0x14c main mode: "set kimode", "set kimainmode", aioption type 4
    unsigned mInfluenceW = 0;           // +0x150 grid width (mapW / 200)
    unsigned mInfluenceH = 0;           // +0x154
    std::vector<CInfluenceMapCell> mInfluenceMap;   // +0x158
    int mDebugInfluenceMap = IM_GOAL;   // +0x170 (debug texture, not ported)
    unsigned mLastSquadID = 0;          // +0x174 squad ids count from 1
    std::set<CKISquad*, CKISquadLess> mSquads;   // +0x178 never shrinks (squads are never deleted)
    std::set<CUnit*, CKIUnitLess> mSquadUnits;   // +0x190 units collected for squads (step 6)
    unsigned mMaxUnitsPerSquad = 0;     // +0x1a8
    unsigned mTickCounter = 0;          // +0x1b0 the AI ticks every 10th call
    unsigned mWantedSquads = 0;         // +0x1b4
    unsigned mCurrentSquads = 0;        // +0x1b8
    CKISquad* mResumeSquad = nullptr;   // +0x1bc squad to resume updating with
    CTexture* mFOWTextureKI = nullptr;  // +0x1ac fow_ki.png (drawing only)
    bool mPassive = false;              // +0x1c0 network client that is not the host
    int mTargetMode = 2;                // +0x1c4 0 sector, 1 unit, 2 position
    unsigned mTargetSector = 0;         // +0x1c8
    unsigned mTargetUnit = 0;           // +0x1cc
    CVector2 mTargetPosition;           // +0x1d0
    // +0x1fc + 4 * player id of every game object in the original: the brain
    // this AI reserved the object for (updateBrainCommands steps 1 and 4).
    // Only this AI reads or writes its slot, so it is kept here, by object id.
    std::map<unsigned, unsigned> mReservedFor;

private:
    void updateBrainGatherers();        // updateBrainCommands sub-step 3
    std::vector<std::unique_ptr<CKISquad>> mSquadStorage;   // owns the squads
};

}  // namespace py
