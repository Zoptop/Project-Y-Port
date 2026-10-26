// CGameBombers.cpp - CGame::addBomb/addDropCrate/addDropMine/addBomber:
// create a bomber or a falling object, init it (which takes the next
// game-object id) and append it to the scene object list (+0x3c).
// Objects added while CGame::timeUpdate walks the list are updated in the
// same step (std::list: the walk reaches the new tail).
#include "game/CBomber.h"
#include "game/CBomberBomb.h"
#include "game/CBomberCrate.h"
#include "game/CBomberMine.h"
#include "game/CGame.h"

namespace py {

// Ghidra 0x1789f0
void CGame::addBomb(Real yaw, Real pitch, const CVector3& position, Real velocity, CPlayer* owner) {
    auto* b = new CBomberBomb();
    b->init(yaw, pitch, position, velocity, owner);
    mSceneObjects.push_back(b);
}

// Ghidra 0x178a84
void CGame::addDropCrate(Real yaw, Real pitch, const CVector3& position, Real velocity) {
    auto* c = new CBomberCrate();
    c->init(yaw, pitch, position, velocity);
    mSceneObjects.push_back(c);
}

// Ghidra 0x178b10
void CGame::addDropMine(Real yaw, Real pitch, const CVector3& position, Real velocity, CPlayer* owner) {
    auto* m = new CBomberMine();
    m->init(yaw, pitch, position, velocity, owner);
    mSceneObjects.push_back(m);
}

// Ghidra 0x178ba4
void CGame::addBomber(CVector2 target, CPlayer* owner, int dropType, unsigned dropParam1, unsigned dropParam2,
                      int callerScript, int droppedUnitId) {
    auto* b = new CBomber();
    b->init(target, owner, dropType, dropParam1, dropParam2, callerScript, droppedUnitId);
    mSceneObjects.push_back(b);
}

}  // namespace py
