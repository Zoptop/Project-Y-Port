// CPlane.h - port of ProjectY::CPlane (OGRE 1.x Plane derived).
//
// Layout matches the original: CVector3 normal (+0x0) and float d (+0xc).
// The plane is normal.dot(p) + d = 0.
#pragma once

#include "CMatrix3.h"
#include "CVector3.h"

namespace py {

// Minimal stand-in for ProjectY::CAxisAlignedBox, only what
// CPlane::getSide(const CAxisAlignedBox&) reads: minimum (+0x0), maximum
// (+0xc) and the extent enum (+0x18). Replace with the full port of the
// class once it exists.
struct CAxisAlignedBox {
    enum Extent { EXTENT_NULL = 0, EXTENT_FINITE = 1, EXTENT_INFINITE = 2 };
    CVector3 mMinimum{-0.5f, -0.5f, -0.5f};
    CVector3 mMaximum{0.5f, 0.5f, 0.5f};
    int mExtent = EXTENT_NULL;

    CAxisAlignedBox() = default;
    CAxisAlignedBox(const CVector3& mn, const CVector3& mx) : mMinimum(mn), mMaximum(mx), mExtent(EXTENT_FINITE) {}

    bool isNull() const { return mExtent == EXTENT_NULL; }
    bool isInfinite() const { return mExtent == EXTENT_INFINITE; }
    CVector3 getCenter() const {
        return {(mMaximum.x + mMinimum.x) * 0.5f, (mMaximum.y + mMinimum.y) * 0.5f,
                (mMaximum.z + mMinimum.z) * 0.5f};
    }
    // As inlined into CPlane::getSide: ZERO unless the extent is FINITE
    // (the INFINITE case is never reached there).
    CVector3 getHalfSize() const {
        if (mExtent == EXTENT_FINITE) return (mMaximum - mMinimum) * 0.5f;
        return CVector3::ZERO;
    }
};

class CPlane {
public:
    enum Side { NO_SIDE = 0, POSITIVE_SIDE = 1, NEGATIVE_SIDE = 2, BOTH_SIDE = 3 };

    CVector3 normal;
    Real d = 0.0f;

    CPlane();                                                                  // 0x371d80
    CPlane(const CPlane& rhs);                                                 // 0x371ddc
    CPlane(const CVector3& rkNormal, float fConstant);                         // 0x371e28 (d = -fConstant)
    CPlane(float a, float b, float c, float d);                                // 0x371e7c
    CPlane(const CVector3& rkNormal, const CVector3& rkPoint);                 // 0x37265c
    CPlane(const CVector3& rkPoint0, const CVector3& rkPoint1, const CVector3& rkPoint2);   // 0x372568
    CPlane& operator=(const CPlane&) = default;

    Side getSide(const CVector3& rkPoint) const;                               // 0x371f70
    Side getSide(const CAxisAlignedBox& rkBox) const;                          // 0x3720bc
    Side getSide(const CVector3& centre, const CVector3& halfSize) const;      // 0x371fb4
    float getDistance(const CVector3& rkPoint) const;                          // 0x371ebc
    void redefine(const CVector3& rkPoint0, const CVector3& rkPoint1, const CVector3& rkPoint2);   // 0x372270
    void redefine(const CVector3& rkNormal, const CVector3& rkPoint);          // 0x37257c
    CVector3 projectVector(const CVector3& v) const;                           // 0x372670
    float normalise();                                                         // 0x3727e4

    CPlane operator-() const { return CPlane(-(normal.x), -(normal.y), -(normal.z), -d); }
    bool operator==(const CPlane& rhs) const { return rhs.d == d && rhs.normal == normal; }
    bool operator!=(const CPlane& rhs) const { return rhs.d != d || rhs.normal != normal; }
};

}  // namespace py
