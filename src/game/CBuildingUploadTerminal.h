// CBuildingUploadTerminal.h - port of ProjectY::CBuildingUploadTerminal
// (0x348 bytes), a campaign objective building. Its dish turns (drawing
// only; "RadarYaw").
//
// Ghidra: CBuildingUploadTerminal 0x14d104, init 0x14d13c, timeUpdate 0x14d264.
#pragma once

#include <cstdio>

#include "game/CBuilding.h"

namespace py {

class CBuildingUploadTerminal : public CBuilding {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x14ddd0 (CBuildingSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x14dda4
    CBuildingUploadTerminal() = default;            // Ghidra 0x14d104
    void timeUpdate() override;                     // +0x08 Ghidra 0x14d264
    void timeUpdateSimple() override;               // +0x0c Ghidra 0x14dd34
    void holdState() override { mRadarYawHeld = mRadarYaw; }      // +0x1c Ghidra 0x14d0cc
    void restoreState() override { mRadarYaw = mRadarYawHeld; }   // +0x20 Ghidra 0x14d0e8
    void init(const CVector2& position, Real rotation, unsigned sectorId, int subtype) override;   // +0xcc 0x14d13c
    // drawing (CBuildingUploadTerminalDraw.cpp)
    void draw(bool alphaPass, float alpha) override;   // +0x10 Ghidra 0x14d2d4

    Real mRadarYaw = 0.0f;               // +0x338 "RadarYaw"
    Real mRadarYawHeld = 0.0f;           // +0x33c
};

}  // namespace py
