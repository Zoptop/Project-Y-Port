// CUIUtils.h - small helpers of the original the HUD uses: CUtils::isInRectangle,
// the interface atlas UV helpers and CMath::checkSegmentRectangleHit.
// Render/UI maths: plain float.
#pragma once

#include "core/CVector2.h"

namespace py::uiutil {

// CUtils::isInRectangle (Ghidra 0x3d25ac): strictly inside (x0, y0)..(x1, y1)
inline bool isInRectangle(const CVector2& p, float x0, float y0, float x1, float y1) {
    return p.x > x0 && p.x < x1 && p.y > y0 && p.y < y1;
}

// CUtils::getInterfaceBtnUVpos (Ghidra 0x3d2668): cell (col, row) of `cell`
// pixels in the 1024 px interface.png atlas
inline CVector2 btnUVpos(int col, int row, float cell = 64.0f) {
    return CVector2(static_cast<float>(col) * cell * (1.0f / 1024.0f), static_cast<float>(row) * cell * (1.0f / 1024.0f));
}
// CUtils::getInterfaceBtnUVdefsize (Ghidra 0x3d26f0): one 64 px cell
inline CVector2 btnUVdefsize() { return CVector2(0.0625f, 0.0625f); }
// CTexture::getUVCoordsFromPixel (Ghidra 0x3a2980) for interface.png (1024 x 1024)
inline CVector2 uvFromPixel(float px, float py) { return CVector2(px / 1024.0f, py / 1024.0f); }

// get_line_intersection (segments p0-p1 and p2-p3): 1 and the point if they cross
inline int getLineIntersection(float p0x, float p0y, float p1x, float p1y, float p2x, float p2y, float p3x,
                               float p3y, float* ix, float* iy) {
    const float s1x = p1x - p0x, s1y = p1y - p0y;
    const float s2x = p3x - p2x, s2y = p3y - p2y;
    const float den = -s2x * s1y + s1x * s2y;
    if (den == 0.0f) return 0;
    const float s = (-s1y * (p0x - p2x) + s1x * (p0y - p2y)) / den;
    const float t = (s2x * (p0y - p2y) - s2y * (p0x - p2x)) / den;
    if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
        *ix = p0x + t * s1x;
        *iy = p0y + t * s1y;
        return 1;
    }
    return 0;
}

// CMath::checkSegmentRectangleHit (Ghidra 0x2e4128): where the segment a-b
// crosses the rectangle (min, max): left, top, right, bottom edge in that order
inline bool checkSegmentRectangleHit(const CVector2& a, const CVector2& b, const CVector2& mn, const CVector2& mx,
                                     CVector2& out) {
    float ix = 0, iy = 0;
    int r = getLineIntersection(mn.x, mn.y, mn.x, mx.y, a.x, a.y, b.x, b.y, &ix, &iy);
    if (!r) r = getLineIntersection(mn.x, mn.y, mx.x, mn.y, a.x, a.y, b.x, b.y, &ix, &iy);
    if (!r) r = getLineIntersection(mx.x, mn.y, mx.x, mx.y, a.x, a.y, b.x, b.y, &ix, &iy);
    if (!r) r = getLineIntersection(mn.x, mx.y, mx.x, mx.y, a.x, a.y, b.x, b.y, &ix, &iy);
    out = CVector2(ix, iy);
    return r == 1;
}

}  // namespace py::uiutil
