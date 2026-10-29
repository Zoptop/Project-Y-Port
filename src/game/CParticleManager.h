// CParticleManager.h - port of ProjectY::CParticleManager (0x2c bytes,
// engine +0x14c): owns every particle system, keyed by id.
//
// The original keeps the systems in a std::unordered_map<unsigned,
// CParticleSystem*> (libstdc++ of the Android NDK, GCC 4.8) and updates
// them in the map's iteration order. That order decides the order of the
// systems' CRng draws, so the port reproduces the container exactly
// (CParticleSystemTable: same bucket counts, rehash policy, insertion and
// erase rules) instead of relying on the host's std::unordered_map, whose
// order differs between standard libraries.
//
// Ids come from a static counter that starts at 1 and is never reset
// (mNextParticleSystemID, 0x4e1914 in .data); 0 means "no system".
//
// Ghidra: CParticleManager 0x36c890, timeUpdate 0x36c524, getParticleSystem
// 0x36c684, disableParticleSystem 0x36c6f8, cleanUp 0x36c7f8,
// createParticleSystem 0x36ca44 / 0x36cbc8 (CVector2 + z), getUVTileCoords
// 0x36c4d4. Drawing: draw 0x36cea8 (CParticleManagerDraw.cpp). Not ported: save/load.
#pragma once

#include <cstdio>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/Real.h"
#include "game/CParticleSystem.h"
#include "render/CQuad.h"

namespace py {

class CCameraLevel;
class CGLSLProgram;
class CPlayer;
class CTexture;

// std::unordered_map<unsigned, CParticleSystem*> as the original's libstdc++
// implements it (_Hashtable with _Prime_rehash_policy, max load factor 1,
// std::hash<unsigned> = identity). Owns the systems.
class CParticleSystemTable {
public:
    struct Node {
        Node* next = nullptr;
        unsigned key = 0;
        CParticleSystem* value = nullptr;
    };

    CParticleSystemTable();   // 11 buckets (_M_next_bkt(10))
    ~CParticleSystemTable();
    CParticleSystemTable(const CParticleSystemTable&) = delete;
    CParticleSystemTable& operator=(const CParticleSystemTable&) = delete;

    // _M_emplace (unique keys): no-op if the key exists. Takes ownership.
    void insert(unsigned key, CParticleSystem* value);
    CParticleSystem* find(unsigned key) const;
    // Unlinks and frees `n` (not the system); returns the node after it.
    Node* erase(Node* n);
    // Deletes every system and node; the bucket count stays (clear()).
    void clear();

    Node* begin() const { return mBeforeBegin.next; }   // iteration order
    size_t size() const { return mElementCount; }
    size_t bucketCount() const { return mBuckets.size(); }

    // _Prime_rehash_policy (GCC 4.8: double arithmetic, ceil/floor)
    static size_t nextBucketCount(size_t n, size_t* nextResize);

private:
    size_t bucketOf(unsigned key) const { return key % mBuckets.size(); }
    void rehash(size_t n);

    std::vector<Node*> mBuckets;   // each points at the node *before* the bucket's first node
    Node mBeforeBegin;
    size_t mElementCount = 0;
    size_t mNextResize = 0;
};

class CParticleManager {
public:
    void save(FILE* f);                      // Ghidra 0x36c728 (CParticleSave.cpp)
    void load(FILE* f);                      // Ghidra 0x36cc28
    CParticleManager();    // Ghidra 0x36c890 (shader, texture, quad buffers: render only)
    ~CParticleManager();   // Ghidra 0x36c7fc

    // Ghidra 0x36ca44. type: EParticleSystemType. Creates and initialises a
    // system (its init may draw CRng), gives it the next id and stores it.
    // ignoreFOW (+0x4e), player (null: shown to everybody), showInMenu (+0x4f).
    // Returns null for an unknown type ([!] the original crashes).
    CParticleSystem* createParticleSystem(int type, const CVector3& position, int subtype, bool ignoreFOW,
                                          CPlayer* player, bool showInMenu);
    // Ghidra 0x36cbc8: position (pos.x, pos.y, z)
    CParticleSystem* createParticleSystem(int type, const CVector2& position, Real z, int subtype, bool ignoreFOW,
                                          CPlayer* player, bool showInMenu);
    // The id of a new system, as most callers store it (`create(...)->mID`).
    unsigned createParticleSystemId(int type, const CVector3& position, int subtype) {
        CParticleSystem* ps = createParticleSystem(type, position, subtype, false, nullptr, false);
        return ps ? ps->mID : 0;
    }

    CParticleSystem* getParticleSystem(unsigned id) const;   // Ghidra 0x36c684 (null if gone)
    // Ghidra 0x36c6f8: asks the system to delete itself and clears `id`.
    void disableParticleSystem(unsigned& id);
    // Not an original function: the pattern every owner uses to make a
    // system follow it - `getParticleSystem(id)`, then 0 into `id` if the
    // system is gone, else the system's position = `position`.
    void attachParticleSystem(unsigned& id, const CVector3& position);
    // Ghidra 0x36c524. Every system in the map's order: timeUpdate, or, when
    // it asked for deletion and has no particles left, delete it.
    void timeUpdate(Real dt);
    // Ghidra 0x36c7f8 (CEngine::resetGame)
    void cleanUp();

    static CVector2 getUVTileCoords(Real u, Real v) { return CVector2(u * 0.125f, v * 0.125f); }   // Ghidra 0x36c4d4

    const CParticleSystemTable& systems() const { return mSystems; }

    static unsigned mNextParticleSystemID;   // 0x4e1914, starts at 1

    // Ghidra 0x36cea8 (CParticleManagerDraw.cpp): the systems' quads in two
    // batches of at most 10000, sorted by z: the depth-tested systems, then
    // the NoDepthCheck ones (+0x4c) without depth test; `menu` draws only the
    // ShowInMenu systems (+0x4f), else only the others. A system shown only to
    // one player (+0x2c) is drawn for that interface player only.
    void draw(const CCameraLevel& cam, bool menu);

private:
    void initGraphics();
    CParticleSystemTable mSystems;   // +0x00
    // drawing (the constructor's render part, made on the first draw)
    std::vector<CQuadPosUVColor4> mQuadsDepth;     // +0x18 (10000)
    std::vector<CQuadPosUVColor4> mQuadsNoDepth;   // +0x1c (10000)
    CGLSLProgram* mShader = nullptr;               // +0x24 particlesgray.gfx
    CTexture* mTexture = nullptr;                  // +0x28 particlesgray.png
    bool mGraphicsTried = false;
};

}  // namespace py
