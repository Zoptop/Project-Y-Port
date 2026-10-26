// CPlane.cpp - port of ProjectY::CPlane, see CPlane.h.
// Translated from the Ghidra decompilation of libProjectY.so (OGRE Plane);
// py::Real arithmetic with the original's operand order.
#include "CPlane.h"

namespace py {

// Ghidra 0x371d80
CPlane::CPlane() : normal(CVector3::ZERO), d(0.0f) {}

// Ghidra 0x371ddc
CPlane::CPlane(const CPlane& rhs) : normal(rhs.normal), d(rhs.d) {}

// Ghidra 0x371e28: d = -fConstant (sign-bit flip)
CPlane::CPlane(const CVector3& rkNormal, float fConstant) : normal(rkNormal), d(-fConstant) {}

// Ghidra 0x371e7c
CPlane::CPlane(float a, float b, float c, float _d) : normal(a, b, c), d(_d) {}

// Ghidra 0x37265c
CPlane::CPlane(const CVector3& rkNormal, const CVector3& rkPoint) { redefine(rkNormal, rkPoint); }

// Ghidra 0x372568
CPlane::CPlane(const CVector3& rkPoint0, const CVector3& rkPoint1, const CVector3& rkPoint2) {
    redefine(rkPoint0, rkPoint1, rkPoint2);
}

// Ghidra 0x371ebc
float CPlane::getDistance(const CVector3& rkPoint) const { return d + normal.dotProduct(rkPoint); }

// Ghidra 0x371f70
CPlane::Side CPlane::getSide(const CVector3& rkPoint) const {
    const float fDistance = getDistance(rkPoint);
    if (fDistance < 0.0f) return NEGATIVE_SIDE;
    if (fDistance > 0.0f) return POSITIVE_SIDE;
    return NO_SIDE;
}

// Ghidra 0x3720bc
CPlane::Side CPlane::getSide(const CAxisAlignedBox& box) const {
    if (box.isNull()) return NO_SIDE;
    if (box.isInfinite()) return BOTH_SIDE;
    return getSide(box.getCenter(), box.getHalfSize());
}

// Ghidra 0x371fb4
CPlane::Side CPlane::getSide(const CVector3& centre, const CVector3& halfSize) const {
    // Calculate the distance between box centre and the plane
    const float dist = getDistance(centre);

    // Calculate the maximise allows absolute distance for
    // the distance between box centre and plane
    const float maxAbsDist = normal.absDotProduct(halfSize);

    if (dist < -maxAbsDist) return NEGATIVE_SIDE;
    if (dist > +maxAbsDist) return POSITIVE_SIDE;
    return BOTH_SIDE;
}

// Ghidra 0x372270
void CPlane::redefine(const CVector3& rkPoint0, const CVector3& rkPoint1, const CVector3& rkPoint2) {
    const CVector3 kEdge1 = rkPoint1 - rkPoint0;
    const CVector3 kEdge2 = rkPoint2 - rkPoint0;
    normal = kEdge1.crossProduct(kEdge2);
    // normal.normalise(), inlined with the products commuted
    const Real fLength = fdlibm::sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (fLength > bitsf(0x322bcc77u)) {   // 1e-08
        const Real fInvLength = 1.0f / fLength;
        normal.x = fInvLength * normal.x;
        normal.y = fInvLength * normal.y;
        normal.z = fInvLength * normal.z;
    }
    // d = -normal.dotProduct(rkPoint0), commuted
    d = -(rkPoint0.x * normal.x + rkPoint0.y * normal.y + rkPoint0.z * normal.z);
}

// Ghidra 0x37257c
void CPlane::redefine(const CVector3& rkNormal, const CVector3& rkPoint) {
    normal = rkNormal;
    d = -rkNormal.dotProduct(rkPoint);
}

// Ghidra 0x372670
CVector3 CPlane::projectVector(const CVector3& p) const {
    // We know plane normal is unit length, so use simple method
    CMatrix3 xform;
    // (OGRE writes -normal.y * normal.x etc.; the original commuted some.)
    xform.m[0][0] = 1.0f - normal.x * normal.x;
    xform.m[0][1] = -normal.x * normal.y;
    xform.m[0][2] = -normal.x * normal.z;
    xform.m[1][0] = normal.x * -normal.y;
    xform.m[1][1] = 1.0f - normal.y * normal.y;
    xform.m[1][2] = normal.z * -normal.y;
    xform.m[2][0] = normal.x * -normal.z;
    xform.m[2][1] = normal.y * -normal.z;
    xform.m[2][2] = 1.0f - normal.z * normal.z;
    return xform * p;
}

// Ghidra 0x3727e4
float CPlane::normalise() {
    const float fLength = normal.length();

    // Will also work for zero-sized vectors, but will change nothing
    if (fLength > bitsf(0x322bcc77u)) {   // 1e-08
        const Real fInvLength = 1.0f / Real(fLength);
        normal.x = fInvLength * normal.x;
        normal.y = fInvLength * normal.y;
        normal.z = fInvLength * normal.z;
        d = d * fInvLength;
    }
    return fLength;
}

}  // namespace py
