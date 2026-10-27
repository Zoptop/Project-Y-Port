// CBuildingScienceLab.h - port of ProjectY::CBuildingScienceLab (0x3e0
// bytes): researches techs (ETechs, CTechManager). A brain inside runs the
// research of mCurrentTech; when done the player gets the tech and the brain
// is used up. -1 = no tech.
//
// Ghidra: CBuildingScienceLab 0x1442dc, init 0x144834, timeUpdate 0x144920,
// processDialButton 0x1445b8, getRandomTech 0x144454, calcChecksum 0x146268.
#pragma once

#include "game/CBuilding.h"

namespace py {

class CBuildingScienceLab : public CBuilding {
public:
    void drawInfoscreen(CVector2& pos) override;   // +0x94 Ghidra 0x145908 (CInfoscreenDraw.cpp)
    CBuildingScienceLab() = default;                // Ghidra 0x1442dc

    void timeUpdate() override;                     // +0x08 Ghidra 0x144920
    void calcChecksum() override;                   // +0x24 Ghidra 0x146268
    void ownerChanged(CPlayer* newOwner) override;  // +0x44 Ghidra 0x146dcc
    void processDialButton(int button) override;    // +0x84 Ghidra 0x1445b8 (simulation part)
    bool isCommanded() override { return !mContainedBrains.empty(); }   // +0x88 Ghidra 0x143ea4
    void removeCommander() override;                // +0x8c Ghidra 0x144188
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x144834
    // drawing (CBuildingScienceLabDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x144c58
    void drawLabels();                              // (port) the label part of draw
    void enter(unsigned brainId) override { mContainedBrains.push_back(brainId); }   // +0xd8 Ghidra 0x1442a8

    // Ghidra 0x144454: a random researchable tech other than the current and
    // the next one (unsynced CRng), -1 if none.
    int getRandomTech();

    int mCurrentTech = 0;                // +0x350 "CurrentTech"
    int& mNextTech = mProductionType;    // +0x36c "NextTech" (CBuilding::mProductionType; -1 after init)
    bool mResearchStopped = false;       // +0x388 "ResearchStopped"
    bool mAutoCallClosestBrain = false;  // +0x3a4 "AutoCallClosestBrain"
    Real mCallClosestBrainCountdown = 0.0f;   // +0x3c0 "CallClosestBrainCountdown"
    bool mRandomResearchEnabled = false; // +0x3dc "RandomResearchEnabled"
};

}  // namespace py
