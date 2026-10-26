// CVector2.h - ProjectY::CVector2 (OGRE Vector2 derived), subset.
// Components are py::Real (the original's soft-float arithmetic, see Real.h);
// each operator is a single operation so expressions keep the original
// evaluation order (see docs/DETERMINISM.md).
#pragma once

#include "Real.h"

namespace py {

struct CVector2 {
    Real x = 0.0f, y = 0.0f;

    constexpr CVector2() = default;
    constexpr CVector2(float x_, float y_) : x(x_), y(y_) {}

    CVector2 operator+(const CVector2& o) const { return {x + o.x, y + o.y}; }
    CVector2 operator-(const CVector2& o) const { return {x - o.x, y - o.y}; }
    CVector2 operator*(Real s) const { return {x * s, y * s}; }
    bool operator==(const CVector2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const CVector2& o) const { return !(*this == o); }

    Real squaredLength() const { return x * x + y * y; }
    Real dotProduct(const CVector2& o) const { return x * o.x + y * o.y; }

    // Ghidra 0x12b55c: sqrtf((x-o.x)^2 + (y-o.y)^2) through the game's fdlibm.
    float distance(const CVector2& o) const;

    static const CVector2 ZERO;
};

inline const CVector2 CVector2::ZERO{0.0f, 0.0f};

}  // namespace py
