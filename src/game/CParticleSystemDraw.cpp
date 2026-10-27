// CParticleSystemDraw.cpp - the particle systems' drawing: checkVisibility,
// interpolateColors and every type's getQuads (vtable +0x08). The quads go
// into CParticleManager's batches (CQuadPosUVColor4: centre, half size,
// atlas cell of particlesgray.png (8 x 8 tiles), colour, alpha, yaw and an
// optional rotation: the camera's billboard matrix (camera +0x268) for
// particles that face the camera, the beam's rotation for shots, none for
// flat rings on the ground).
//
// t below is a particle's age / lifetime; v(t: a@x, b@y, ...) is
// interpolateValue with value a at time x, b at y, ...
//
// [!] Two quirks of the original are kept (both are drawing reaching into
// state the logic owns):
//   - Thruster particles of type 1 draw one CRng::GetUniform each per frame
//     for their colour. CRng is also what the computer players decide with,
//     so in a game that is drawn the AI's choices depend on the frame rate
//     and on what is on screen, as in the original. The lockstep checksum
//     and the replay tests never draw.
//   - Smoke subtypes 8 / 9 (invulnerability / stun rings) set their
//     particles' height (+0xc) every frame.
#include <utility>

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CRng.h"
#include "core/fdlibm.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CParticleSystemExplosion.h"
#include "game/CParticleSystemInfo.h"
#include "game/CParticleSystemSelect.h"
#include "game/CParticleSystemShotConstant.h"
#include "game/CParticleSystemShotDirected.h"
#include "game/CParticleSystemSmoke.h"
#include "game/CParticleSystemThruster.h"
#include "game/CParticleManager.h"
#include "render/CCamera.h"
#include "render/CQuad.h"

namespace py {

namespace {
const CCameraLevel* cam() { return CDrawContext::get().camera; }
bool strategy() { return cam() && cam()->isStrategyView(); }
const CMatrix3* billboard() { return cam() ? &cam()->billboardMatrix() : nullptr; }
CVector2 tile(Real u, Real v) { return CParticleManager::getUVTileCoords(u, v); }
// s x CVector2::UNIT_SCALE
CVector2 square(Real s) { return CVector2(s * 1.0f, s * 1.0f); }
CVector3 twice(const CVector3& c) { return CVector3(c.x + c.x, c.y + c.y, c.z + c.z); }
CVector3 scaled(const CVector3& c, Real k) { return CVector3(c.x * k, c.y * k, c.z * k); }
// the quad of particle `p` with its own atlas cell
CQuadPosUVColor4 particleQuad(const CVector3& center, const CVector2& half, const CParticle& p, const CVector3& color,
                              Real alpha, Real yaw, const CMatrix3* rot) {
    return CQuadPosUVColor4(center, half, p.mUVTopLeft, p.mUVSize, color, alpha, yaw, rot);
}
}  // namespace

// Ghidra 0x36e038
bool CParticleSystem::checkVisibility() {
    // (outside the ingame interface, interface +0x34 != 1: always visible)
    const CCameraLevel* c = cam();
    if (!c) return true;
    CEngine* e = mEngine ? mEngine : CEngine::getSingletonPtr();
    if (!mIgnoreFOW && e->mUseFOW && e->mGame) {
        const Real v = e->mGame->getFOWvisibility(CDrawContext::get().player, nullptr,
                                                  CVector2(mPosition.x, mPosition.y), 2, nullptr);
        if (v == 0.0f) return false;
    }
    mVisibilityCenter = mPosition;
    mVisible = c->sphereInFrustum(mVisibilityCenter, mBoundingRadius);
    return mVisible;
}

CVector3 CParticleSystem::interpolateColors(Real t, const CVector3& c0, Real t0, const CVector3& c1, Real t1) const {
    return CVector3(interpolateValue(t, c0.x, t0, c1.x, t1), interpolateValue(t, c0.y, t0, c1.y, t1),
                    interpolateValue(t, c0.z, t0, c1.z, t1));
}
CVector3 CParticleSystem::interpolateColors(Real t, const CVector3& c0, Real t0, const CVector3& c1, Real t1,
                                            const CVector3& c2, Real t2) const {
    return CVector3(interpolateValue(t, c0.x, t0, c1.x, t1, c2.x, t2), interpolateValue(t, c0.y, t0, c1.y, t1, c2.y, t2),
                    interpolateValue(t, c0.z, t0, c1.z, t1, c2.z, t2));
}
CVector3 CParticleSystem::interpolateColors(Real t, const CVector3& c0, Real t0, const CVector3& c1, Real t1,
                                            const CVector3& c2, Real t2, const CVector3& c3, Real t3) const {
    return CVector3(interpolateValue(t, c0.x, t0, c1.x, t1, c2.x, t2, c3.x, t3),
                    interpolateValue(t, c0.y, t0, c1.y, t1, c2.y, t2, c3.y, t3),
                    interpolateValue(t, c0.z, t0, c1.z, t1, c2.z, t2, c3.z, t3));
}

// ---------------------------------------------------------------- Smoke

// Ghidra 0x36fd10: camera-facing quads turned by the particle's yaw.
void CParticleSystemSmoke::getQuads(CQuadPosUVColor4* quads, int& count) {
    if (strategy()) return;
    const CVector2 uvSize = tile(1.0f, 1.0f);
    const CVector3 fire0 = CVector3::COL_YELLOW, fire1 = CVector3::COL_RED, fire2 = CVector3::COL_BLACK;
    for (auto& up : mParticles) {
        CParticle& p = *up;
        const Real yaw = p.mYaw;
        const Real t = p.mCurLifetime / p.mTotalLifetime;
        CVector2 half, uv;
        CVector3 col;
        Real alpha = 0.0f;
        switch (mSubtype) {
            case 1:   // fire smoke of a burning unit
                half = square(interpolateValue(t, 4.0f, 0.0f, 16.0f, 1.0f));
                uv = tile(3.0f, 3.0f);
                alpha = interpolateValue(t, 0.0f, 0.0f, 0.5f, 0.05f, 0.4f, 0.2f, 0.0f, 1.0f);
                col = interpolateColors(t, fire0, 0.0f, fire1, bitsf(0x3d4ccccd), fire2, bitsf(0x3e99999a));
                break;
            case 2:
                half = square(interpolateValue(t, 1.0f, 0.0f, 10.0f, 1.0f));
                uv = tile(2.0f, 3.0f);
                alpha = interpolateValue(t, 0.0f, 0.0f, 0.5f, 0.1f, 0.0f, 1.0f);
                col = interpolateColors(t, CVector3::COL_CYAN, 0.0f, CVector3::COL_BLACK, bitsf(0x3dcccccd));
                break;
            case 3:
            case 4: {
                half = CVector2(5.0f, 5.0f);
                Real a1, b1, a2, b2;
                if (mSubtype == 3) {
                    uv = tile(0.0f, 1.0f);
                    col = CVector3(1.0f, 1.0f, 0.0f);
                    a1 = 1.0f, b1 = 0.3f, a2 = 1.0f, b2 = 0.7f;
                } else {
                    uv = tile(1.0f, 1.0f);
                    col = CVector3(0.0f, 1.0f, 0.0f);
                    a1 = 0.6f, b1 = 0.2f, a2 = 0.6f, b2 = 0.8f;
                }
                alpha = interpolateValue(t, 0.0f, 0.0f, a1, b1, a2, b2, 0.0f, 1.0f);
                break;
            }
            case 5:
            case 6:
                half = square(mSubtype == 5 ? interpolateValue(t, 2.0f, 0.0f, 8.0f, 1.0f)
                                            : interpolateValue(t, 8.0f, 0.0f, 32.0f, 1.0f));
                uv = tile(3.0f, 3.0f);
                alpha = interpolateValue(t, 0.0f, 0.0f, 0.5f, 0.05f, 0.4f, 0.2f, 0.0f, 1.0f);
                col = interpolateColors(t, fire0, 0.0f, fire1, bitsf(0x3d4ccccd), fire2, bitsf(0x3e99999a));
                break;
            case 7:
                half = square(interpolateValue(t, 0.0f, 0.0f, 7.0f, 1.0f));
                uv = tile(3.0f, 3.0f);
                alpha = interpolateValue(t, 0.0f, 0.0f, 0.5f, 0.05f, 0.7f, 0.3f, 0.0f, 1.0f);
                col = twice(CVector3::COL_YELLOW);
                break;
            case 8:   // invulnerability ring, rising and falling around the unit
                half = square(mScale);
                uv = tile(3.0f, 0.0f);
                col = interpolateColors(t, CVector3(bitsf(0x3f4ccccd), bitsf(0x3e4ccccd), bitsf(0x3dcccccd)), 0.0f,
                                        CVector3(bitsf(0x3fb33333), 0.5f, bitsf(0x3ecccccd)), 0.5f,
                                        CVector3(bitsf(0x3fe66666), bitsf(0x3f19999a), bitsf(0x3f4ccccd)), 1.0f);
                p.mPosition.z = interpolateValue(t, 0.0f, 0.0f, 8.0f, 0.5f, 0.0f, 1.0f);   // [!] see the top
                alpha = 1.0f;
                break;
            case 9:   // stun ring
                half = square(mScale);
                uv = tile(3.0f, 0.0f);
                col = interpolateColors(t, CVector3(bitsf(0x3e4ccccd), 0.5f, 1.0f), 0.0f,
                                        CVector3(bitsf(0x3e99999a), bitsf(0x3e99999a), 1.0f), 0.5f,
                                        CVector3(0.5f, 0.5f, bitsf(0x3f99999a)), 1.0f);
                p.mPosition.z = mPosition.z + interpolateValue(t, 0.0f, 0.0f, 8.0f, 0.5f, 0.0f, 1.0f);   // [!]
                alpha = 1.0f;
                break;
            case 10:
                half = square(interpolateValue(t, 5.0f, 0.0f, 20.0f, 1.0f));
                uv = tile(3.0f, 0.0f);
                alpha = interpolateValue(t, 0.0f, 0.0f, 0.9f, 0.1f, 0.0f, 1.0f);
                col = interpolateColors(t, CVector3(bitsf(0x3e4ccccd), 0.5f, 1.0f), 0.0f,
                                        CVector3(bitsf(0x3e99999a), bitsf(0x3e99999a), 1.0f), 0.5f,
                                        CVector3(0.5f, 0.5f, bitsf(0x3f99999a)), 1.0f);
                break;
            case 11:
            case 12: {
                Real a1, a2;
                if (mSubtype == 11) {
                    half = square(interpolateValue(t, 5.0f, 0.0f, 13.0f, 1.0f));
                    a1 = 0.3f, a2 = 0.2f;
                } else {
                    half = square(interpolateValue(t, 10.0f, 0.0f, 25.0f, 1.0f));
                    a1 = 0.5f, a2 = 0.4f;
                }
                uv = tile(2.0f, 3.0f);
                alpha = interpolateValue(t, 0.0f, 0.0f, a1, 0.05f, a2, 0.2f, 0.0f, 1.0f);
                col = CVector3(bitsf(0x3f547ae1), bitsf(0x3e78d4fe), bitsf(0x3e3c6a7f));   // = COL_RESOURCE
                break;
            }
            default: break;   // [!] the original draws whatever is left in its locals
        }
        quads[count++] = CQuadPosUVColor4(p.mPosition, half, uv, uvSize, col, alpha, yaw, billboard());
    }
}

// ---------------------------------------------------------------- Select

// Ghidra 0x36efe8: flat markers on the ground (no billboard), pulsing for
// subtypes 2, 3 and 6; three times larger in the strategy view.
void CParticleSystemSelect::getQuads(CQuadPosUVColor4* quads, int& count) {
    const CCameraLevel* c = cam();
    const CVector2 uvSize = tile(1.0f, 1.0f);
    CVector2 uv;
    CVector3 col;
    Real yaw = 0.0f;
    switch (mSubtype) {
        case 1: uv = tile(0.0f, 0.0f); col = CVector3(0.0f, 0.0f, 1.0f); break;
        case 2: uv = tile(1.0f, 0.0f); col = CVector3(1.0f, 0.0f, 0.0f); break;
        case 3: uv = tile(1.0f, 0.0f); col = CVector3(0.0f, 1.0f, 0.0f); break;
        case 4: uv = tile(0.0f, 0.0f); col = CVector3(1.0f, 1.0f, 0.0f); break;
        case 5: uv = tile(2.0f, 0.0f); col = CVector3(bitsf(0x3f333333), 0.0f, 0.0f); break;
        case 6:
            uv = tile(4.0f, 2.0f);
            col = CVector3(1.0f, 0.0f, 1.0f);
            yaw = c ? c->yaw() : 0.0f;
            break;
        case 7: uv = tile(0.0f, 0.0f); col = CVector3(1.0f, 0.0f, 1.0f); break;
        default: break;
    }
    for (auto& up : mParticles) {
        const CParticle& p = *up;
        const Real t = p.mCurLifetime / p.mTotalLifetime;
        Real s = 0.0f;
        switch (mSubtype) {
            case 2:
            case 3: s = (CMath::SinTable(CMath::PI * 6.0f * t) + 3.0f) * 3.0f; break;
            case 4: s = t * 15.0f; break;
            case 5: s = (1.0f - t * bitsf(0x3f333333)) * 10.0f; break;
            case 6: s = (CMath::SinTable(CMath::PI * 6.0f * t) + 3.0f) * 1.5f; break;
            case 1:
            case 7: s = t * 12.0f; break;
            default: break;
        }
        if (c && c->isStrategyView()) s = s * 3.0f;
        const Real tt = t * t;
        const Real alpha = 1.0f - tt;
        const Real h = s * 3.0f;
        quads[count++] = CQuadPosUVColor4(p.mPosition, CVector2(h, h), uv, uvSize, col, alpha, yaw, nullptr);
    }
}

// ---------------------------------------------------------------- ShotConstant

// Ghidra 0x2d18e4: a beam between the system's position and its end point,
// as long as the range (x 0.5 + 1) and lying in the beam's rotation (+0x64).
// Particle type 0 is the flickering core (its colour runs through a key
// cycle 6 times per second), type 1 the glow; both are offset from the
// beam's middle by their speed vector (in the beam's frame).
void CParticleSystemShotConstant::getQuads(CQuadPosUVColor4* quads, int& count) {
    if (strategy()) return;
    const CVector3 mid((mPosition.x + mEndpoint.x) * 0.5f, (mPosition.y + mEndpoint.y) * 0.5f,
                       (mPosition.z + mEndpoint.z) * 0.5f);
    const Real len = mRange * 0.5f + 1.0f;
    for (auto& up : mParticles) {
        const CParticle& p = *up;
        const Real f = fdlibm::fmodf(p.mCurLifetime, 1.0f);
        auto offset = [&]() {
            const CVector3 o = mRotation * p.mSpeed;
            return CVector3(mid.x + o.x, mid.y + o.y, mid.z + o.z);
        };
        auto core = [&](const CVector3& a, const CVector3& b) {
            const Real tt = fdlibm::fmodf(f + f, 1.0f);
            const Real alpha = interpolateValue(tt, 0.1f, 0.0f, 0.4f, 0.5f, 0.1f, 1.0f);
            const Real k = fdlibm::fmodf(f * 6.0f, 1.0f);
            const CVector3 col = interpolateColors(k, a, 0.0f, b, 0.5f, a, 1.0f);
            return std::make_pair(alpha, col);
        };
        const unsigned type = p.mType;
        switch (mSubtype) {
            case 1:   // gatling laser (blue / green)
            case 3: {  // heavy gatling (red / yellow)
                if (type == 0) {
                    const auto ac = mSubtype == 1 ? core(CVector3::COL_BLUE, CVector3::COL_GREEN)
                                                  : core(CVector3::COL_RED, CVector3::COL_YELLOW);
                    quads[count++] = particleQuad(offset(), CVector2(len, mSubtype == 1 ? 4.0f : 6.0f), p, ac.second,
                                                  ac.first, 0.0f, &mRotation);
                } else if (type == 1) {
                    const Real tt = fdlibm::fmodf(f + f, 1.0f);
                    const Real alpha = interpolateValue(tt, 0.9f, 0.0f, 0.7f, 0.5f, 0.9f, 1.0f);
                    const CVector3 col = mSubtype == 1
                                             ? CVector3(bitsf(0x3f4ccccd), bitsf(0x3fd9999a), bitsf(0x3fb33333))
                                             : CVector3(bitsf(0x3fb33333), bitsf(0x3fd9999a), bitsf(0x3f4ccccd));
                    quads[count++] = particleQuad(offset(), CVector2(len, mSubtype == 1 ? 2.5f : 3.5f), p, col, alpha,
                                                  0.0f, &mRotation);
                }
                break;
            }
            case 2:   // hacker beam (blue / red), not offset
                if (type == 0) {
                    const auto ac = core(CVector3::COL_BLUE, CVector3::COL_RED);
                    quads[count++] = particleQuad(mid, CVector2(len, 4.0f), p, ac.second, ac.first, 0.0f, &mRotation);
                } else if (type == 1) {
                    const Real tt = fdlibm::fmodf(f + f, 1.0f);
                    const Real alpha = interpolateValue(tt, 0.9f, 0.0f, 0.7f, 0.5f, 0.9f, 1.0f);
                    quads[count++] = particleQuad(mid, CVector2(len, 2.5f), p,
                                                  CVector3(bitsf(0x3fd9999a), bitsf(0x3f4ccccd), bitsf(0x3fb33333)),
                                                  alpha, 0.0f, &mRotation);
                }
                break;
            case 4:   // sniper aiming laser
                if (type == 0)
                    quads[count++] = particleQuad(mid, CVector2(len, 1.0f), p, twice(CVector3::COL_RED), 0.2f, 0.0f,
                                                  &mRotation);
                break;
            case 5:
                if (type == 0)
                    quads[count++] = particleQuad(mid, CVector2(len, 7.0f), p, twice(CVector3::COL_GREEN), 0.5f, 0.0f,
                                                  &mRotation);
                break;
            default: break;
        }
    }
}

// ---------------------------------------------------------------- ShotDirected

// Ghidra 0x2d25dc: the bolt of the laser blasts (core type 0, glow type 1),
// all particles at the system's position in the shot's rotation.
void CParticleSystemShotDirected::getQuads(CQuadPosUVColor4* quads, int& count) {
    if (strategy()) return;
    for (auto& up : mParticles) {
        const CParticle& p = *up;
        const unsigned type = p.mType;
        CVector2 half;
        CVector3 col;
        Real alpha;
        if (mSubtype == 2) {
            if (type == 0) {
                half = CVector2(12.0f, 9.0f);
                col = CVector3::COL_GREEN;
                alpha = 0.2f;
            } else if (type == 1) {
                half = CVector2(6.0f, 4.5f);
                col = CVector3(1.0f, 3.0f, 1.0f);
                alpha = 0.9f;
            } else {
                continue;
            }
        } else if (mSubtype == 4) {
            if (type != 0) continue;
            half = CVector2(12.0f, 1.0f);
            col = twice(CVector3::COL_ORANGE);
            alpha = 1.0f;
        } else if (mSubtype == 1) {
            if (type == 0) {
                half = CVector2(8.0f, 6.0f);
                col = CVector3::COL_MAGENTA;
                alpha = 0.2f;
            } else if (type == 1) {
                half = CVector2(4.0f, 3.0f);
                col = CVector3(2.0f, 2.0f, 1.0f);
                alpha = 0.9f;
            } else {
                continue;
            }
        } else {
            continue;
        }
        quads[count++] = particleQuad(mPosition, half, p, col, alpha, 0.0f, &mRotation);
    }
}

// ---------------------------------------------------------------- Thruster

// Ghidra 0x2d2b98: the jet of the bombers / drop ships. Type 0 is the
// nozzle flame, turned about its axis with the clock and placed 10 (3)
// behind the system in its orientation (+0x58); type 1 the exhaust puffs,
// camera-facing, with a random brightness ([!] CRng, see the top).
void CParticleSystemThruster::getQuads(CQuadPosUVColor4* quads, int& count) {
    if (strategy()) return;
    CEngine* e = mEngine ? mEngine : CEngine::getSingletonPtr();
    CMatrix3 r;
    mOrientation.ToRotationMatrix(r);
    for (auto& up : mParticles) {
        const CParticle& p = *up;
        const Real age = p.mCurLifetime;
        const Real yaw = p.mYaw;
        const Real t = age / p.mTotalLifetime;
        if (mSubtype != 1 && mSubtype != 2) continue;
        if (p.mType == 0) {
            const Real k = fdlibm::fmodf(age + e->mGameTime / 5000.0f, 1.0f);
            CVector3 col;
            if (mSubtype == 1)
                col = interpolateColors(k, CVector3::COL_CYAN, 0.0f, CVector3::COL_BLUE, bitsf(0x3ea8f5c3),
                                        CVector3::COL_WHITE, bitsf(0x3f28f5c3), CVector3::COL_CYAN, 1.0f);
            else
                col = interpolateColors(k, CVector3::COL_GREEN, 0.0f, CVector3::COL_BLUE, bitsf(0x3ea8f5c3),
                                        CVector3::COL_WHITE, bitsf(0x3f28f5c3), scaled(CVector3::COL_BLUE, 1.5f), 1.0f);
            CMatrix3 spin;
            spin.FromEulerAnglesXYZ(k * 180.0f * CMath::fDeg2Rad, 0.0f, CMath::fDeg2Rad * 0.0f);
            const CMatrix3 rot = spin * r;
            const CVector3 o = r * CVector3(mSubtype == 1 ? -10.0f : -3.0f, 0.0f, 0.0f);
            const CVector3 c(mPosition.x + o.x, mPosition.y + o.y, mPosition.z + o.z);
            const CVector2 half = mSubtype == 1 ? CVector2(10.0f, 6.0f) : CVector2(5.0f, 3.0f);
            quads[count++] = particleQuad(c, half, p, col, 0.3f, 0.0f, &rot);
        } else if (p.mType == 1) {
            const CVector2 half = square(mSubtype == 1 ? interpolateValue(t, 4.0f, 0.0f, 6.0f, 1.0f)
                                                       : interpolateValue(t, 2.0f, 0.0f, 4.0f, 1.0f));
            const Real alpha = interpolateValue(t, 0.0f, 0.0f, 0.6f, 0.2f, 0.2f, 0.5f, 0.0f, 1.0f);
            CVector3 col;
            if (mSubtype == 1) {
                const Real b = CRng::GetUniform() / 3.0f + bitsf(0x3f28f5c3);
                const CVector3 c0 = scaled(CVector3::COL_CYAN, b);
                col = interpolateColors(t, c0, 0.0f, CVector3::COL_WHITE, 1.0f);
            } else {
                const CVector3 m = scaled(CVector3::COL_MAGENTA, 0.5f);
                const Real b = CRng::GetUniform() / 3.0f + bitsf(0x3f28f5c3);
                const CVector3 c0(b * m.x, b * m.y, b * m.z);
                col = interpolateColors(t, c0, 0.0f, scaled(CVector3::COL_WHITE, 0.5f), 1.0f);
            }
            quads[count++] = particleQuad(p.mPosition, half, p, col, alpha, yaw, billboard());
        }
    }
}

// ---------------------------------------------------------------- Explosion

namespace {
// one explosion particle type: size v(t: s0@0, s1@1) or, with s2 >= 0,
// v(t: s0@0, s1@0.3, s2@1)
struct ExplosionSizes {
    Real s[5][3];
};
// sizes of particle types 0..4 for subtypes 1, 3, 4 (2 = 4)
const ExplosionSizes kSizes1 = {{{0.0f, 80.0f, -1.0f}, {0.0f, 50.0f, 0.0f}, {0.0f, 16.0f, -1.0f}, {4.0f, 20.0f, -1.0f}, {4.0f, 25.0f, -1.0f}}};
const ExplosionSizes kSizes3 = {{{0.0f, 60.0f, -1.0f}, {0.0f, 30.0f, 0.0f}, {0.0f, 12.0f, -1.0f}, {4.0f, 15.0f, -1.0f}, {4.0f, 20.0f, -1.0f}}};
const ExplosionSizes kSizes4 = {{{0.0f, 30.0f, -1.0f}, {0.0f, 15.0f, 0.0f}, {0.0f, 6.0f, -1.0f}, {2.0f, 7.5f, -1.0f}, {2.0f, 10.0f, -1.0f}}};
}  // namespace

// Ghidra 0x2cbe38. Type 0: the flat shock ring (gray, fading from 0.7);
// 1: the flash; 2: fire balls; 3: fire turning to smoke; 4: smoke. Rings
// lie on the ground, the rest faces the camera, turned by the particle yaw.
void CParticleSystemExplosion::getQuads(CQuadPosUVColor4* quads, int& count) {
    if (strategy()) return;
    const CVector3 W = CVector3::COL_WHITE, K = CVector3::COL_BLACK;
    const CVector3 warm(1.0f, bitsf(0x3f4ccccd), 0.5f), hot(1.0f, 0.5f, 0.5f);
    for (auto& up : mParticles) {
        const CParticle& p = *up;
        const Real yaw = p.mYaw;
        const Real t = p.mCurLifetime / p.mTotalLifetime;
        const unsigned type = p.mType;
        auto ring = [&](Real size, const CVector3& col) {
            const Real a = interpolateValue(t, 0.7f, 0.0f, 0.0f, 1.0f);
            quads[count++] = particleQuad(p.mPosition, square(interpolateValue(t, 0.0f, 0.0f, size, 1.0f)), p, col, a,
                                          0.0f, nullptr);
        };
        auto bill = [&](const CVector2& half, const CVector3& col, Real a, const CMatrix3* rot) {
            quads[count++] = particleQuad(p.mPosition, half, p, col, a, yaw, rot);
        };
        switch (mSubtype) {
            case 1:
            case 2:
            case 3:
            case 4: {
                const ExplosionSizes& S = mSubtype == 1 ? kSizes1 : mSubtype == 3 ? kSizes3 : kSizes4;
                if (type > 4) break;
                const Real* s = S.s[type];
                if (type == 0) {
                    ring(s[1], CVector3::COL_GRAY50);
                    break;
                }
                CVector2 half;
                Real a;
                CVector3 col;
                switch (type) {
                    case 1:
                        half = square(interpolateValue(t, s[0], 0.0f, s[1], 0.3f, s[2], 1.0f));
                        a = interpolateValue(t, 0.0f, 0.0f, 1.0f, 0.05f, 1.0f, 0.5f, 0.0f, 1.0f);
                        col = interpolateColors(t, W, 0.0f, W, 0.5f, warm, 1.0f);
                        break;
                    case 2:
                        half = square(interpolateValue(t, s[0], 0.0f, s[1], 1.0f));
                        a = interpolateValue(t, 0.0f, 0.0f, 1.0f, 0.1f, 0.8f, 0.75f, 0.0f, 1.0f);
                        col = interpolateColors(t, W, bitsf(0x3ecccccd), W, bitsf(0x3f333333), hot, 1.0f);
                        break;
                    case 3:
                        half = square(interpolateValue(t, s[0], 0.0f, s[1], 1.0f));
                        a = interpolateValue(t, 0.0f, 0.0f, 0.9f, 0.02f, 0.9f, 0.2f, 0.0f, 1.0f);
                        col = interpolateColors(t, W, 0.0f, hot, bitsf(0x3e19999a), K, bitsf(0x3ecccccd));
                        break;
                    default:
                        half = square(interpolateValue(t, s[0], 0.0f, s[1], 1.0f));
                        a = interpolateValue(t, 0.0f, 0.0f, 0.9f, 0.05f, 0.9f, 0.2f, 0.0f, 1.0f);
                        col = interpolateColors(t, K, 0.0f, W, 1.0f);
                        break;
                }
                bill(half, col, a, billboard());
                break;
            }
            case 5:
            case 6:   // small hits
                if (type == 1) {
                    bill(square(interpolateValue(t, 0.0f, 0.0f, 4.0f, 0.3f, 0.0f, 1.0f)),
                         interpolateColors(t, W, 0.0f, W, 0.5f, warm, 1.0f),
                         interpolateValue(t, 0.0f, 0.0f, 0.7f, 0.05f, 0.7f, 0.5f, 0.0f, 1.0f), billboard());
                } else if (type == 4) {
                    bill(square(interpolateValue(t, 1.0f, 0.0f, 5.0f, 1.0f)), interpolateColors(t, K, 0.0f, W, 1.0f),
                         interpolateValue(t, 0.0f, 0.0f, 0.5f, 0.05f, 0.5f, 0.2f, 0.0f, 1.0f), billboard());
                }
                break;
            case 7:   // EMP blast (blue), lying flat
                if (type == 1) {
                    bill(square(interpolateValue(t, 0.0f, 0.0f, 50.0f, 0.3f, 0.0f, 1.0f)),
                         interpolateColors(t, W, 0.0f, CVector3(bitsf(0x3e4ccccd), bitsf(0x3f4ccccd), 1.0f), 0.5f,
                                           CVector3::COL_CYAN, 1.0f),
                         interpolateValue(t, 0.0f, 0.0f, 1.0f, 0.05f, 1.0f, 0.5f, 0.0f, 1.0f), nullptr);
                } else if (type == 2) {
                    bill(square(interpolateValue(t, 20.0f, 0.0f, 80.0f, 1.0f)),
                         interpolateColors(t, W, bitsf(0x3ecccccd), CVector3(bitsf(0x3e4ccccd), bitsf(0x3f666666), 1.0f),
                                           bitsf(0x3f333333), scaled(CVector3::COL_BLUE, 0.5f), 1.0f),
                         interpolateValue(t, 0.0f, 0.0f, 1.0f, 0.1f, 0.8f, 0.5f, 0.0f, 1.0f), nullptr);
                } else if (type == 0) {
                    ring(110.0f, CVector3(bitsf(0x3ecccccd), bitsf(0x3f4ccccd), 1.0f));
                }
                break;
            case 8:   // resource explosion
                if (type == 1) {
                    bill(square(interpolateValue(t, 0.0f, 0.0f, 30.0f, 0.3f, 0.0f, 1.0f)),
                         interpolateColors(t, W, 0.0f, W, 0.5f, CVector3::COL_RESOURCE, 1.0f),
                         interpolateValue(t, 0.0f, 0.0f, 1.0f, 0.05f, 1.0f, 0.5f, 0.0f, 1.0f), billboard());
                } else if (type == 0) {
                    ring(60.0f, CVector3::COL_GRAY50);
                } else if (type == 3) {
                    bill(square(interpolateValue(t, 6.0f, 0.0f, 18.0f, 1.0f)),
                         interpolateColors(t, W, 0.0f, CVector3::COL_RESOURCE, bitsf(0x3e19999a), CVector3::COL_RESOURCE,
                                           bitsf(0x3ecccccd)),
                         interpolateValue(t, 0.0f, 0.0f, 0.9f, 0.02f, 0.9f, 0.2f, 0.0f, 1.0f), billboard());
                }
                break;
            default: break;
        }
    }
}

// ---------------------------------------------------------------- Info

// Ghidra 0x2cf938: subtype 1 only (the "unit ready" marker), particles that
// have started (age >= 0): the ground ring (type 0), the glow (type 1) and
// the sparkles (type 2, sized by their yaw phase); then the text label.
void CParticleSystemInfo::getQuads(CQuadPosUVColor4* quads, int& count) {
    if (strategy()) return;
    for (auto& up : mParticles) {
        const CParticle& p = *up;
        if (!(p.mCurLifetime >= 0.0f) || mSubtype != 1) continue;
        const Real yaw = p.mYaw;
        const Real t = p.mCurLifetime / p.mTotalLifetime;
        const unsigned type = p.mType;
        if (type == 1) {
            const CVector2 half = square(interpolateValue(t, 0.0f, 0.0f, 5.0f, 0.2f));
            const Real a = interpolateValue(t, 0.0f, 0.0f, 1.2f, 0.2f, 0.4f, 0.5f, 0.0f, 1.0f);
            const Real k = fdlibm::fmodf(yaw / CMath::TWO_PI, 1.0f);
            const CVector3 col = interpolateColors(k, CVector3(1.0f, 1.0f, 0.5f), 0.0f, CVector3(1.0f, 1.5f, 1.0f), 0.5f,
                                                   CVector3(1.0f, 1.0f, 0.5f), 1.0f);
            quads[count++] = particleQuad(p.mPosition, half, p, col, a, yaw, billboard());
        } else if (type == 2) {
            const Real ph = yaw / CMath::TWO_PI;
            const Real k = fdlibm::fmodf(ph, 1.0f);
            const CVector2 half = square(interpolateValue(k, 0.0f, 0.0f, 18.0f, 0.2f, 0.0f, 1.0f));
            const Real a = interpolateValue(t, 0.0f, 0.0f, 1.8f, 0.1f, 0.0f, 1.0f);
            const CVector3 col = interpolateColors(fdlibm::fmodf(ph, 1.0f), CVector3(1.5f, 1.0f, 0.5f), 0.0f,
                                                   CVector3(1.0f, 1.5f, 1.0f), 0.5f, CVector3(1.5f, 1.0f, 0.5f), 1.0f);
            quads[count++] = particleQuad(p.mPosition, half, p, col, a, yaw, billboard());
        } else if (type == 0) {
            const CVector2 half = square(interpolateValue(t, 0.0f, 0.0f, 40.0f, 1.0f));
            const Real a = interpolateValue(t, 0.7f, 0.0f, 0.0f, 1.0f);
            quads[count++] = particleQuad(p.mPosition, half, p, CVector3(1.0f, 1.5f, 1.0f), a, 0.0f, nullptr);
        }
    }
    // TODO(CTextRender): the label mText (+0x54) above the system: screen
    // position of mPosition (camera +0x100, screenPosToPixelPos), raised by
    // v(r: 10@0, 100@1) px with r = runtime / lifetime (+0x50 / +0x58), size
    // v(r: 1@0, 48@0.05, 1@1) x GUI scale, centred (calcTextPixelLength / 2),
    // colour (1, 1, 0.5), alpha v(r: 0@0, 0.75@0.05, 0@1): drawStringPixel.
}

}  // namespace py
