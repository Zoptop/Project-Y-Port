// CVector2.cpp - out-of-line ProjectY::CVector2 functions (see CVector2.h).
#include "CVector2.h"

#include "FloatBits.h"
#include "fdlibm.h"

namespace py {

// Ghidra 0x12b55c ProjectY::CVector2::distance(CVector2 const&) const
// (sf_* so that inf/NaN inputs give the original's NaN bit patterns).
float CVector2::distance(const CVector2& o) const {
    const float dx = sf_sub(x, o.x);
    const float dy = sf_sub(y, o.y);
    return fdlibm::sqrtf(sf_add(sf_mul(dx, dx), sf_mul(dy, dy)));
}

}  // namespace py
