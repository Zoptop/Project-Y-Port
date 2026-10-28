// CSoundMoodAnalyzer.h - port of ProjectY::CSoundMoodAnalyzer (0x50 bytes,
// embedded in CSoundManager at +0x18).
//
// Picks the music mood from what happens to the local player: damage dealt
// and received, units destroyed and lost, sectors captured and lost, flags
// being lost, and the influence (CUnit::getInfluenceAndFalloff) of the own
// and the enemy units. Every second of frame time (CEngine +0x1a8) it decays
// the counters, adds the elapsed time to "tranquility" and chooses one of
// six moods; the music (CSoundManager::getRandomSoundForMood) follows it
// when the next track is due.
//
// None of this is simulation state: the counters are fed from the
// simulation (CUnit::hit / die, CSector::ownerChanged,
// CBuildingFlagpost::timeUpdate) but nothing reads them back.
#pragma once

#include "core/Real.h"

namespace py {

class CEngine;

// ESoundMood: the music track sets ta_ tn_ td_ aa_ an_ ad_ (CSoundManager ctor)
enum ESoundMood {
    SOUNDMOOD_TRANQUIL_AGGRESSIVE = 0,   // "ta_<n>.ogg"
    SOUNDMOOD_TRANQUIL_NEUTRAL = 1,      // "tn_<n>.ogg"
    SOUNDMOOD_TRANQUIL_DEFENSIVE = 2,    // "td_<n>.ogg"
    SOUNDMOOD_ACTION_AGGRESSIVE = 3,     // "aa_<n>.ogg"
    SOUNDMOOD_ACTION_NEUTRAL = 4,        // "an_<n>.ogg"
    SOUNDMOOD_ACTION_DEFENSIVE = 5,      // "ad_<n>.ogg"
};

class CSoundMoodAnalyzer {
public:
    void reset();                         // Ghidra 0x39aaf8
    void update();                        // Ghidra 0x399d38 (CSoundManager::update, every frame)
    void damageReceived(Real amount);     // Ghidra 0x399b00
    void damageDealt(Real amount);        // Ghidra 0x399b54
    void unitDestroyed();                 // Ghidra 0x399ba8
    void unitLost();                      // Ghidra 0x399be8
    void sectorLost();                    // Ghidra 0x399c28
    void sectorCaptured();                // Ghidra 0x399c68
    void sectorFlagCapturing(Real v);     // Ghidra 0x399ca0 (no caller in the original)
    void sectorFlagLoosing(Real v);       // Ghidra 0x399cec
    void drawDebug();                     // Ghidra 0x39a2bc (the values as text, top left)

    CEngine* mEngine = nullptr;           // +0x00
    Real mDamageReceived = 0.0f;          // +0x04
    Real mDamageDealt = 0.0f;             // +0x08
    Real mUnitsDestroyed = 0.0f;          // +0x0c
    Real mUnitsLost = 0.0f;               // +0x10
    Real mSectorsLost = 0.0f;             // +0x14
    Real mSectorsCaptured = 0.0f;         // +0x18
    Real mTranquility = 0.0f;             // +0x1c
    Real mSectorFlagCapturing = 0.0f;     // +0x20
    Real mSectorFlagLoosing = 0.0f;       // +0x24
    Real mOwnInfluence = 0.0f;            // +0x28
    Real mEnemyInfluence = 0.0f;          // +0x2c
    Real mOwnBattleValue = 0.0f;          // +0x30
    Real mEnemyBattleValue = 0.0f;        // +0x34
    Real mMoodTranquil = 1.0f;            // +0x38
    Real mMoodAggressive = 0.0f;          // +0x3c
    Real mSubMoodAggressor = 0.5f;        // +0x40
    Real mSubMoodDefender = 0.5f;         // +0x44
    Real mTimer = 1.0f;                   // +0x48 seconds since the last analysis
    int mMood = SOUNDMOOD_TRANQUIL_NEUTRAL;   // +0x4c ESoundMood (CSoundManager +0x64)
};

}  // namespace py
