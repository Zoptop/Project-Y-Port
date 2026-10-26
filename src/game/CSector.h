// CSector.h - port of ProjectY::CSector (0x70 bytes), phase 3 subset.
//
// A sector is a capturable map region: its owner, the buildings inside it
// (CSector::createBuilding adds them to the sector and to CGame), whether it
// holds an HQ, and two submeshes of the map mesh: "border<id>" (normal view)
// and "schematic<id>" (strategy view).
//
// Ghidra: CSector::CSector 0x392078, createBuilding 0x39229c / 0x392330,
// ownerChanged 0x391a30, draw 0x3918e0.
#pragma once

#include <cstdint>
#include <map>

#include "core/CVector2.h"
#include "core/Real.h"

namespace py {

class CBuilding;
class CPlayer;

class CSector {
public:
    CSector(unsigned id, unsigned ownerId);

    // "create building <type> sector <id> ..." without / with an explicit id.
    // The z coordinate of the script line is not passed on.
    CBuilding* createBuilding(int type, Real x, Real y, Real rotation, int subtype);
    CBuilding* createBuilding(unsigned id, int type, Real x, Real y, Real rotation, int subtype);

    // Ghidra 0x391a30: pushes mOwner to every building of the sector (vtable
    // +0x44), and the drivers of the sector's turrets leave them (leaveVehicle(true)).
    // (The sound mood update for the local player is not simulation.)
    void ownerChanged();
    void calcChecksum();   // Ghidra 0x391c48
    bool isHQSector() const { return mHasHQ; }   // Ghidra 0x392070

    std::map<unsigned, CBuilding*> mBuildings;   // +0x04  by building id
    bool mHasHQ = false;                         // +0x34
    CPlayer* mOwner = nullptr;                   // +0x50
    CBuilding* mFlagpost = nullptr;              // +0x5c set by CBuildingFlagpost::init
    unsigned mId = 0;                            // +0x54
    uint32_t mSchematicFirst = 0, mSchematicCount = 0;   // +0x60/+0x64 "schematic<id>"
    uint32_t mBorderFirst = 0, mBorderCount = 0;         // +0x68/+0x6c "border<id>"
};

}  // namespace py
