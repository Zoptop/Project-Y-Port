// CParticleSystemInfo.h - port of ProjectY::CParticleSystemInfo (type 7,
// 0x7c bytes): an on-map info marker with an optional text (+0x54).
// Subtype 1 lives 4 s and makes 1 + 5 + 10 particles (types 0, 1, 2);
// type-1 particles are re-spawned while the system lives.
//
// Ghidra: ctor 0x2cf66c, setNewParticleData 0x2d0384, genParticles 0x2d0648,
// init 0x2d07cc, timeUpdate 0x2d0894. Not ported: getQuads 0x2cf938 and
// the screen-position helpers (render).
#pragma once

#include <string>

#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemInfo : public CParticleSystem {
public:
    explicit CParticleSystemInfo(int subtype);   // Ghidra 0x2cf66c

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x2d07cc
    void timeUpdate(Real dt) override;                               // Ghidra 0x2d0894
    // Ghidra 0x2d0384: sets up `p` as a particle of `type`; true when the
    // particle is re-spawned for good (type 1 of subtype 1).
    bool setNewParticleData(CParticle* p, unsigned type);
    // Ghidra 0x2d0648. spreadAges: [!] ages (1 - i) * lifetime / count (the
    // original's formula, so only the first two get ages >= 0).
    void genParticles(unsigned type, unsigned count, bool spreadAges);

    Real mRuntime = 0.0f;     // +0x50 "Runtime"
    std::string mText;        // +0x54
    Real mLifetime = 0.0f;    // +0x58 the system's lifetime (subtype 1: 4 s)
};

}  // namespace py
