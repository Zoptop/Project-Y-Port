// CParticleSystemSelect.h - port of ProjectY::CParticleSystemSelect (type 2,
// 0x58 bytes): a short selection/target marker; optionally follows a game
// object (+0x54). No CRng draws.
//
// Ghidra: ctor 0x36f5f0, init 0x36f638, timeUpdate 0x36f8f0, getQuads
// 0x36efe8 (CParticleSystemDraw.cpp).
#pragma once

#include <cstdio>

#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemSelect : public CParticleSystem {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x36fa8c (CParticleSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x36faec
    explicit CParticleSystemSelect(int subtype);   // Ghidra 0x36f5f0

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x36f638
    void getQuads(CQuadPosUVColor4* quads, int& count) override;      // Ghidra 0x36efe8 (CParticleSystemDraw.cpp)
    void timeUpdate(Real dt) override;                               // Ghidra 0x36f8f0

    Real mRuntime = 0.0f;                // +0x50 "Runtime"
    unsigned mAttachedToGameObject = 0;  // +0x54 "AttachedToGameObject"
};

}  // namespace py
