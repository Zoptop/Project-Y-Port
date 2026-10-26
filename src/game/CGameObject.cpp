#include "CGameObject.h"

namespace py {

// Ghidra 0x180ff8 (grid cells and the frame stamp follow with phase 4)
void CGameObject::init(const CVector2& position, float rotation, CPlayer* owner) {
    mOwner = owner;
    mRotation = rotation;
    mPosX = position.x;
    mPosY = position.y;
}

}  // namespace py
