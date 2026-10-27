// CParticleSystemExplosion.h - port of ProjectY::CParticleSystemExplosion
// (type 3, 0x54 bytes). All particles are created by init (with many CRng
// draws); the system asks for its deletion as soon as the first one expires.
//
// Subtypes 1-4 (units/buildings: size), 5/6 (small hits), 7 (EMP),
// 8 (landmine / bomb); others create nothing.
//
// Ghidra: ctor 0x2ce0d4, init 0x2ce11c, timeUpdate 0x2cf398, getQuads
// 0x2cbe38 (not ported).
#pragma once

#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemExplosion : public CParticleSystem {
public:
    explicit CParticleSystemExplosion(int subtype);   // Ghidra 0x2ce0d4

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x2ce11c
    void timeUpdate(Real dt) override;                               // Ghidra 0x2cf398

    Real mRuntime = 0.0f;   // +0x50 "Runtime"
};

}  // namespace py
