// CCraterManager.h - port of ProjectY::CCraterManager (0x38 bytes, CGame +0xb8).
//
// Craters are ground decals left by explosions (bombs, landmines, the HQ's
// destruction, CUnit::die, the spotter). The manager also batches the fake
// shadow quads of units/buildings (addShadowSimple/Transform/Rotate, filled
// and drawn every frame, at most 1000).
//
// [!] Nothing here affects the simulation. The crater list is read only by
// draw/checkVisibility/save, nothing is checksummed, and no game code queries
// it. CGame::timeUpdate calls timeUpdate every step, which only fades the
// craters out (alpha -= dt * 0.007) and deletes the faded ones.
//
// The original addCrater draws three unsynchronised CRng numbers for the
// decal's look (alpha 0.7..1.0, size factor 2..3, rotation 0..2pi). The port
// leaves those out on purpose. Visual-only code must not touch the game's
// RNGs (docs/re/sim_phase4.md), and the CRng stream already differs from
// the original because explosion/smoke/thruster particles (also CRng users)
// are not ported. The renderer can make the look with its own RNG. Alpha
// starts at 1 here, so a crater lasts the original's longest lifetime.
//
// Ghidra: CCraterManager 0x160574, ~ 0x160698, addCrater 0x160970,
// timeUpdate 0x160c64. Not ported: addShadow* (per-frame render batching),
// checkVisibility, draw, save/load.
#pragma once

#include <list>
#include <memory>

#include "core/CVector3.h"
#include "core/Real.h"

namespace py {

class CEngine;

// ProjectY::CCrater (0x30 bytes)
struct CCrater {
    CVector3 mPosition;        // +0x00 (z = 0)
    Real mSize = 0.0f;         // +0x0c / +0x10 in the original: size * (2 + rand)
    Real mRotation = 0.0f;     // +0x14 rand * 2pi in the original (renderer)
    Real mAlpha = 1.0f;        // +0x28 0.7 + 0.3 * rand in the original
    bool mPermanent = false;   // +0x2c never fades
};

class CCraterManager {
public:
    CCraterManager();    // Ghidra 0x160574 (shader/texture/quad buffer: render only)

    // Ghidra 0x160970. 'size' is the crater radius before the random factor.
    void addCrater(const CVector3& position, Real size, bool permanent);
    // Ghidra 0x160c64: fade out and delete (visual only, see above).
    void timeUpdate();

    const std::list<std::unique_ptr<CCrater>>& craters() const { return mCraters; }

    CEngine* mEngine = nullptr;                      // +0x00
    std::list<std::unique_ptr<CCrater>> mCraters;    // +0x18
};

}  // namespace py
