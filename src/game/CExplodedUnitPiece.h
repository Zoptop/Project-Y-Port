// CExplodedUnitPiece.h - port of ProjectY::CExplodedUnitPiece (0x58 bytes):
// one flying piece of a destroyed unit's mesh (CGame::createEUPs). Logic and
// state only; CGame::drawExplodedUnitPieces draws them (CGameDraw.cpp).
//
// The constructor draws 11 CRng uniforms (speed, direction, orientation,
// spin, spin speed). Pieces fly with gravity, bounce on z = 1 and are
// deleted after 10 s (style 0) or 15 s (style 1).
//
// Ghidra: CExplodedUnitPiece 0x17583c, timeUpdate 0x176060, compQDot
// 0x175e50 (a free function).
#pragma once

#include "core/CQuaternion.h"
#include "core/CVector3.h"
#include "core/Real.h"

namespace py {

// EEXPPIECESTYLE
enum EExpPieceStyle { EXPPIECESTYLE_UNIT = 0, EXPPIECESTYLE_BIG = 1 };

// Ghidra 0x175e50: the quaternion derivative used to spin a piece. [!] Not
// the textbook 0.5 * (0, w) * q: the components come out permuted and with
// odd signs, reproduced as compiled.
CQuaternion compQDot(const CQuaternion& q, const CVector3& w);

class CExplodedUnitPiece {
public:
    // Ghidra 0x17583c. firstTri/triCount: the mesh triangles of this piece;
    // color: the owner's colour.
    CExplodedUnitPiece(unsigned firstTri, unsigned triCount, const CVector3& position, const CVector3& color,
                       int style);

    void timeUpdate(Real dt);   // Ghidra 0x176060

    unsigned mFirstTri;          // +0x00
    unsigned mTriCount;          // +0x04
    CVector3 mSpeed;             // +0x08
    CVector3 mPosition;          // +0x14
    CVector3 mColor;             // +0x20
    CQuaternion mOrientation;    // +0x2c
    CVector3 mSpin;              // +0x3c angular velocity
    Real mSize = 0.0f;           // +0x48 float(triCount): heavier pieces fall faster
    bool mDelete = false;        // +0x4c
    Real mAge = 0.0f;            // +0x50
    int mStyle;                  // +0x54 EExpPieceStyle
};

}  // namespace py
