// CParticleSystemThruster.h - port of ProjectY::CParticleSystemThruster
// (type 4, 0x68 bytes): the bombers' engine flames.
//
// Subtypes 1 and 2: three "glow" particles (type 0, never expire) and
// fifteen flame particles (type 1) that are re-spawned when they expire;
// each spawn draws 4 normals and 2 uniforms from CRng. The owner sets
// mPosition and mOrientation (CBomber::draw does it every frame).
//
// [!] getQuads (CParticleSystemDraw.cpp) also draws one CRng::GetUniform
// for every type-1 particle it draws (a flicker), and only when the system
// is visible and the camera is not in strategy view. The renderer must
// reproduce that draw to keep the CRng stream (and so the AI) faithful.
//
// Ghidra: ctor 0x2d372c, setNewParticleData 0x2d37c8, init 0x2d3a84,
// timeUpdate 0x2d3c3c, getQuads 0x2d2b98 (CParticleSystemDraw.cpp).
#pragma once

#include <cstdio>

#include "core/CQuaternion.h"
#include "game/CParticleSystem.h"

namespace py {

class CParticleSystemThruster : public CParticleSystem {
public:
    void save(FILE* f) override;   // +0x14 Ghidra 0x2d3d4c (CParticleSave.cpp)
    void load(FILE* f) override;   // +0x18 Ghidra 0x2d3e2c
    explicit CParticleSystemThruster(int subtype);   // Ghidra 0x2d372c

    void init(const CVector3& position, CPlayer* player) override;   // Ghidra 0x2d3a84
    void getQuads(CQuadPosUVColor4* quads, int& count) override;      // Ghidra 0x2d2b98 (CParticleSystemDraw.cpp)
    void timeUpdate(Real dt) override;                               // Ghidra 0x2d3c3c
    void setNewParticleData(CParticle* p);                           // Ghidra 0x2d37c8

    Real mRuntime = 0.0f;                // +0x50 "Runtime"
    unsigned mAttachedToGameObject = 0;  // +0x54 "AttachedToGameObject" (never set)
    CQuaternion mOrientation;            // +0x58 "Orientation"
};

}  // namespace py
