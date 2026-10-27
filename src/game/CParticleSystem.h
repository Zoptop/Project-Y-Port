// CParticleSystem.h - port of ProjectY::CParticleSystem, the base class of
// the seven particle-system types (CParticleManager::createParticleSystem).
//
// Logic and state only. getQuads (vtable +0x08) and checkVisibility build
// the quads for the renderer and are not ported yet; the state they read
// (positions, ages, UV tiles, orientations) is kept.
//
// [!] The systems draw from the unsynchronised CRng when they create or
// re-spawn particles (Smoke/Thruster/Info setNewParticleData, Explosion
// init), and CRng also decides what the computer AI does. The draws are part
// of the behaviour and are ported in the original's order and count; see
// docs/re/sim_phase4.md.
//
// Ghidra: CParticleSystem(int) 0x36df90, init 0x36dd00, timeUpdate 0x36dcf4
// (empty), interpolateValue 0x36e1c8 / 0x36e2a8 / 0x36e414. Not ported:
// checkVisibility 0x36e038, interpolateColors 0x36e604.. (render),
// save/load.
#pragma once

#include <memory>
#include <vector>

#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CParticle.h"

namespace py {

class CEngine;
class CPlayer;

// CParticleManager::createParticleSystem types (+0x48)
enum EParticleSystemType {
    PS_SMOKE = 1,
    PS_SELECT = 2,
    PS_EXPLOSION = 3,
    PS_THRUSTER = 4,
    PS_SHOTDIRECTED = 5,
    PS_SHOTCONSTANT = 6,
    PS_INFO = 7,
};

class CParticleSystem {
public:
    explicit CParticleSystem(int subtype);   // Ghidra 0x36df90
    virtual ~CParticleSystem() = default;     // Ghidra 0x36dd58 (deletes the particles)

    // vtable +0x0c. The base does nothing (Ghidra 0x36dcf4).
    virtual void timeUpdate(Real dt);
    // vtable +0x10. Ghidra 0x36dd00: position, player, bounding radius; the
    // subclasses create their particles first and then call this.
    virtual void init(const CVector3& position, CPlayer* player);

    // Linear interpolation of a value over 2, 3 or 4 keys (t ascending).
    Real interpolateValue(Real t, Real v0, Real t0, Real v1, Real t1) const;   // Ghidra 0x36e1c8
    Real interpolateValue(Real t, Real v0, Real t0, Real v1, Real t1, Real v2, Real t2) const;   // Ghidra 0x36e2a8
    Real interpolateValue(Real t, Real v0, Real t0, Real v1, Real t1, Real v2, Real t2, Real v3,
                          Real t3) const;   // Ghidra 0x36e414

    unsigned numParticles() const { return static_cast<unsigned>(mParticles.size()); }

    std::vector<std::unique_ptr<CParticle>> mParticles;   // +0x04
    bool mDeleteRequested = false;   // +0x10 "DeleteRequested": the manager deletes it once it has no particles
    CEngine* mEngine = nullptr;      // +0x14
    int mSubtype = 0;                // +0x18 "Subtype"
    CVector3 mPosition;              // +0x1c "Position"
    unsigned mID = 0;                // +0x28
    CPlayer* mDisplayOnlyForPlayer = nullptr;   // +0x2c "DisplayOnlyForPlayer" (null: everybody)
    Real mBoundingRadius = 1.0f;     // +0x30 visibility test radius (= +0x44 after init)
    CVector3 mVisibilityCenter;      // +0x34 (checkVisibility)
    bool mVisible = false;           // +0x40 (checkVisibility)
    Real mInitBoundingRadius = 0.0f; // +0x44 set by the subclass's init
    int mType = 0;                   // +0x48 "Type" (EParticleSystemType)
    bool mNoDepthCheck = false;      // +0x4c "NoDepthCheck"
    bool mSaveable = true;           // +0x4d saved with the game
    bool mIgnoreFOW = false;         // +0x4e drawn even in the fog of war
    bool mShowInMenu = false;        // +0x4f "ShowInMenu"

protected:
    // the std::vector::erase(it) the systems use to remove one particle
    void eraseParticle(size_t i) { mParticles.erase(mParticles.begin() + static_cast<std::ptrdiff_t>(i)); }
};

}  // namespace py
