// CPlayer.h - port of ProjectY::CPlayer (0x120 bytes): a player's units,
// techs, resources, score, fog of war and radar. The computer player
// (CPlayerKI) is in CPlayerKI.h.
//
// Player 0 is the neutral player; 1..n play. Ids of the player's units and
// brains are kept in ordered sets (by id). Techs are a 33-bit vector<bool>
// (ETechs, docs/re/sim_phase4.md 7).
//
// Fog of war (only with the UseFOW option): every player has a byte grid of
// the level's FOW size (CLevel +0x168 x +0x16c, "fowsize" * 32 / 1000), row y
// at y * W. Units and buildings paint visibility into their team members'
// grids each step (drawFOWCircleTeam -> drawFOWCirclePlayer, additive,
// saturating at 255), and FOWtimeupdate fades every explored cell back
// towards 64. getVisibilityValue reads it as value / 256; the radar grid
// (same size, 1 = covered) is reported through its bool*. The original keeps
// the grids in CRasterizer frame buffers and uploads them as textures; the
// textures are not ported, the grids are. The computer player has no FOW grid
// (it sees everything: getVisibilityValue = 1) but does have a radar grid.
//
// Ghidra: CPlayer::CPlayer 0x375124, getBonus 0x374454, calcChecksum 0x373430.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
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

// ProjectY::CRasterizer (0xc bytes): integer drawing into a byte frame
// buffer it does not own. Only the parts the FOW and radar grids use.
class CRasterizer {
public:
    void setFrameBuffer(uint8_t* buffer, unsigned width, unsigned height);   // Ghidra 0x385c5c
    void clear(uint8_t value);                                                // Ghidra 0x385d0c
    void setPixel(int x, int y, uint8_t value);                               // Ghidra 0x385c90
    // Ghidra 0x3862ac: adds `add` (saturating at `max`) inside the circle.
    void upfill_circle(int cx, int cy, int r, uint8_t add, uint8_t max, int w, int h);
    // Ghidra 0x386488: sets `value` inside the circle. [!] see .cpp
    void fill_circle(int cx, int cy, int r, uint8_t value, int w, int h);

    uint8_t* mBuffer = nullptr;   // +0x00
    unsigned mWidth = 0;          // +0x04
    unsigned mHeight = 0;         // +0x08
};

// ProjectY::CRadarEntry (0x10 bytes)
struct CRadarEntry {
    CVector2 position;   // +0x00
    Real radius;         // +0x08
    unsigned id;         // +0x0c the radar's object id
};

class CPlayer {
public:
    enum EType { TYPE_HUMAN = 0, TYPE_NEUTRAL = 1, TYPE_AI = 2 };
    static constexpr unsigned kNumTechs = 33;

    CPlayer(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team);   // Ghidra 0x375124
    virtual ~CPlayer();

    // ---- vtable (slot)
    virtual void timeUpdate();                                 // +0x00 Ghidra 0x373a1c
    virtual void addUnit(CUnit* unit);                         // +0x04 Ghidra 0x374fe4
    virtual void removeUnit(unsigned id);                      // +0x08 Ghidra 0x375034
    virtual void unitProduced(CUnit*, CBuilding*) {}           // +0x0c Ghidra 0x3729f4
    // +0x10 save / +0x14 load: savegames are not ported yet; +0x18 draw: empty
    virtual bool isAI() const { return false; }                // +0x1c
    virtual void initFOWTexture();                             // +0x20 Ghidra 0x374168 (grid only)
    virtual void updateFOWtexture() {}                         // +0x24 Ghidra 0x373230 (texture upload only)
    virtual void FOWtimeupdate();                              // +0x28 Ghidra 0x372918
    virtual void drawFOWCirclePlayer(const CVector2& pos, Real radius);   // +0x2c Ghidra 0x372ffc
    virtual void drawFOWCircleTeam(const CVector2& pos, Real radius);     // +0x30 Ghidra 0x373934
    virtual Real getVisibilityValue(const CVector2& pos, bool* radar);    // +0x34 Ghidra 0x3729fc
    // +0x38 bindFOWtexture, +0x3c stopThreads: rendering / empty
    virtual void reInit();                                     // +0x40 Ghidra 0x3741e0
    virtual bool isFriend(const CPlayer* other) const { return mTeam == other->mTeam; }   // +0x44
    virtual int getBrainCap();                                 // +0x48 Ghidra 0x3732bc
    virtual int getCurrentBrains() { return static_cast<int>(mBrains.size()); }   // +0x4c
    virtual void directUnitControlDisabled(unsigned) {}        // +0x50
    virtual void setTeam(unsigned team) { mTeam = team; }      // +0x54
    virtual bool hasTech(unsigned tech);                       // +0x58 Ghidra 0x3738d4
    virtual bool isTechResearchable(unsigned tech);            // +0x5c Ghidra 0x373330
    virtual std::vector<int> getResearchableTechs();           // +0x60 Ghidra 0x3754d4
    virtual std::vector<int> getKnownTechs();                  // +0x64 Ghidra 0x375420
    virtual void addTech(unsigned tech);                       // +0x68 Ghidra 0x373ed8
    virtual Real getTechResearchTime(unsigned tech);           // +0x6c Ghidra 0x37495c
    virtual void techResearched(unsigned tech, CBuilding* lab);   // +0x70 Ghidra 0x373858
    // +0x74 updateChecksum / +0x78 printChecksum: desync diagnostics, not ported
    virtual void calcChecksum();                               // +0x7c Ghidra 0x373430
    virtual void updateScore();                                // +0x80 Ghidra 0x37345c
    virtual Real getCurrentResources() { return mCurrentResources; }   // +0x84
    virtual void consumeResources(Real amount);                // +0x88 Ghidra 0x372bc8
    virtual void setCaptureAbility(bool can);                  // +0x8c Ghidra 0x372fd0
    virtual Real getTechResearchCost(unsigned tech);           // +0x90 Ghidra 0x3737ec
    virtual void addRadar(const CVector2& pos, Real radius, unsigned id);   // +0x94 Ghidra 0x37592c
    virtual void removeRadar(unsigned id);                     // +0x98 Ghidra 0x374d04
    virtual void refreshRadarMap();                            // +0x9c Ghidra 0x373fec
    virtual void initRadarMap();                               // +0xa0 Ghidra 0x3731b0
    virtual void drawRadarCircleTeam(const CVector2& pos, Real radius);     // +0xa4 Ghidra 0x3739a8
    virtual void drawRadarCirclePlayer(const CVector2& pos, Real radius);   // +0xa8 Ghidra 0x373670

    // ---- non-virtual
    bool isNeutral() const { return mType == TYPE_NEUTRAL; }  // Ghidra 0x373fd8
    void initTeamMembers();                                    // Ghidra 0x375068
    // Ghidra 0x374454: multiplier from researched techs (bonus 0..14),
    // for a unit of weight class / damage type / building flag.
    Real getBonus(int bonus, int weightClass, int damageType, bool building, bool flag);
    Real getBonus(int bonus) { return getBonus(bonus, 0, 0, false, false); }   // Ghidra 0x374930
    void wipeUnitsAndSectors();                                // Ghidra 0x374210
    void lost(bool surrender);                                 // Ghidra 0x374318
    void won();                                                // Ghidra 0x37434c
    // Ghidra 0x374368: own units (category 1) or buildings (2) of a type.
    int getGameobjectCount(unsigned category, unsigned type);
    const std::string& getName() const { return mName; }
    const std::vector<uint8_t>& fowMap() const { return mFOWMap; }
    const std::vector<uint8_t>& radarMap() const { return mRadarMap; }

    // ---- player commands. In the original the AI and the UI issue these
    // through the game's CCommandManager; single player (CCommandManagerLocal)
    // executes them at once, which is what these do (bodies of
    // CCommandManagerLocal, Ghidra 0x158740..0x158de4). The player id
    // argument of the original is `this`.
    // TODO(network): CCommandManagerNetworked queues them for step + 10.
    void cmdUnitOrderEnterUnit(unsigned unitId, unsigned targetId);          // vtbl +0x00
    void cmdUnitOrderMoveTo(unsigned unitId, const CVector2& pos);           // vtbl +0x08
    void cmdUnitOrderAttackUnit(unsigned unitId, unsigned targetId);         // vtbl +0x10
    void cmdUnitOrderEnterBuilding(unsigned unitId, unsigned buildingId);    // vtbl +0x14
    void cmdBuildingOrderDoSpecOp(unsigned buildingId, const CVector2& pos); // vtbl +0x1c
    void cmdProcessDialButton(unsigned objectId, int button);                // vtbl +0x30
    void cmdUnitOrderHoldPosition(unsigned unitId);                          // vtbl +0x34
    void cmdUnitOrderOnWatch(unsigned unitId);                               // vtbl +0x40
    void cmdUnitOrderSetMaxVelocityLimiter(unsigned unitId, Real limiter);   // vtbl +0x44
    void cmdUnitOrderGatherResourcesAtLocation(unsigned unitId, const CVector2& pos);   // vtbl +0x4c

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
    std::map<unsigned, std::unique_ptr<CRadarEntry>> mRadars;   // +0x60 by object id
    unsigned mTeam = 0;                 // +0x78
    Real mFOWFadeRemainder = 0.0f;      // +0x7c fractional part of dt*10 not yet faded
    std::vector<uint8_t> mFOWMap;       // +0x80 FOW grid (empty until initFOWTexture)
    std::unique_ptr<CRasterizer> mFOWRasterizer;   // +0x88
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
    Real mScore = 0.0f;                 // +0x114 updateScore (not initialised by the original)
    std::unique_ptr<CRasterizer> mRadarRasterizer;   // +0x118
    std::vector<uint8_t> mRadarMap;     // +0x11c radar grid (empty until initRadarMap)
};

}  // namespace py

// The computer player used to be declared here; most users include only
// CPlayer.h.
#include "game/CPlayerKI.h"
