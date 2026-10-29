// CParticleSystemSmoke.h - port of ProjectY::CParticleSystemSmoke (type 1,
// 0x70 bytes): smoke, dust and the "status" effects that follow a unit.
//
// It spawns a particle every mRuntimeFactor seconds until it has
// mMaxParticles, then re-spawns expired ones; every spawn draws from CRng
// (setNewParticleData). Subtypes as created by the game:
//    1 smoke (damaged unit, < 30 % health)   2/10 smoke trail
//    3 xp gain (CUnitRoboBrain::gainXP)       4 heal (CUnit::heal)
//    5, 6 dust                                7 bomber drop marker
//    8 invulnerability, 9 stun (follow the unit; +0x6c = its radius)
//   11 resource gathering, 12 harvester gathering
//
// Ghidra: ctor 0x370e64, setNewParticleData 0x370eac, init 0x37170c,
// timeUpdate 0x371878, getQuads 0x36fd10 (CParticleSystemDraw.cpp).
#pragma once

#include <cstdio>

#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemSmoke : public CParticleSystem {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x371b70 (CParticleSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x371ca4
    explicit CParticleSystemSmoke(int subtype);   // Ghidra 0x370e64

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x37170c
    void getQuads(CQuadPosUVColor4* quads, int& count) override;      // Ghidra 0x36fd10 (CParticleSystemDraw.cpp)
    void timeUpdate(Real dt) override;                               // Ghidra 0x371878
    void setNewParticleData(CParticle* p);                           // Ghidra 0x370eac

    Real mRuntime = 0.0f;           // +0x50 "Runtime"
    unsigned mMaxParticles = 0;     // +0x54 "MaxParticles"
    Real mRuntimeFactor = 0.0f;     // +0x58 "RuntimeFactor": seconds per new particle
    bool mPositionFollows = false;  // +0x5c "PositionFollows": particles move with the system
    CVector3 mOldPosition;          // +0x60 "OldPosition"
    Real mScale = 0.0f;             // +0x6c "Scale" (set by the creator; getQuads)
};

}  // namespace py
