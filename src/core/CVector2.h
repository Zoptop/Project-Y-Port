// CVector2.h - ProjectY::CVector2 (OGRE Vector2 derived), subset.
// Plain binary32 components; each operator is a single IEEE operation so
// expressions keep the original evaluation order (see docs/DETERMINISM.md).
#pragma once

namespace py {

struct CVector2 {
    float x = 0.0f, y = 0.0f;

    constexpr CVector2() = default;
    constexpr CVector2(float x_, float y_) : x(x_), y(y_) {}

    CVector2 operator+(const CVector2& o) const { return {x + o.x, y + o.y}; }
    CVector2 operator-(const CVector2& o) const { return {x - o.x, y - o.y}; }
    CVector2 operator*(float s) const { return {x * s, y * s}; }
    bool operator==(const CVector2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const CVector2& o) const { return !(*this == o); }

    static const CVector2 ZERO;
};

inline const CVector2 CVector2::ZERO{0.0f, 0.0f};

}  // namespace py
