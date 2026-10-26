// CVector3.cpp - out-of-line ProjectY::CVector3 functions (see CVector3.h).
#include "CVector3.h"

namespace py {

// Ghidra 0x150ad0 ProjectY::CVector3::normalise()
float CVector3::normalise() {
    const Real fLength = fdlibm::sqrtf(x * x + y * y + z * z);
    if (fLength > bitsf(0x322bcc77u)) {   // 1e-08
        const Real fInvLength = 1.0f / fLength;
        x = x * fInvLength;
        y = y * fInvLength;
        z = z * fInvLength;
    }
    return fLength;
}

// Ghidra 0x3e0ad4 ProjectY::CVector3::distance(CVector3 const&) const
float CVector3::distance(const CVector3& v) const {
    const Real dx = x - v.x;
    const Real dy = y - v.y;
    const Real dz = z - v.z;
    return fdlibm::sqrtf(dx * dx + dy * dy + dz * dz);
}

}  // namespace py
