#include "CQuad.h"

#include <cmath>

namespace py {

namespace {

// Six vertices from the four corners: p0 p1 p2 p0 p2 p3.
constexpr int kCorner[6] = {0, 1, 2, 0, 2, 3};

template <class V>
void setPosUV(V& v, const Vec3& p, const CVector2& uv) {
    v.pos[0] = p.x;
    v.pos[1] = p.y;
    v.pos[2] = p.z;
    v.uv[0] = uv.x;
    v.uv[1] = uv.y;
}

// x' = ((x*m0 + y*m1) + z*m2) + m3 per row of a row-major CMatrix4
// (the order the original's unrolled code adds in).
Vec3 transformRowMajor(const float m[16], float x, float y, float z) {
    Vec3 r;
    r.x = ((x * m[0] + y * m[1]) + z * m[2]) + m[3];
    r.y = ((x * m[4] + y * m[5]) + z * m[6]) + m[7];
    r.z = ((x * m[8] + y * m[9]) + z * m[10]) + m[11];
    return r;
}

// Corners and UVs of the matrix constructors (Ghidra 0x3824e0 / 0x380b1c).
// Note the original adds 0 to tl.x / tl.y for p0..p2 but not for p3.
void matrixCorners(float z, const float m[16], const CVector2& half, const CVector2& uvTL, const CVector2& uvSize,
                   Vec3 p[4], CVector2 uv[4]) {
    const float hx = half.x, hy = half.y;
    p[0] = transformRowMajor(m, -hx, -hy, z);
    p[1] = transformRowMajor(m, hx, -hy, z);
    p[2] = transformRowMajor(m, hx, hy, z);
    p[3] = transformRowMajor(m, -hx, hy, z);
    const float u0 = uvTL.x + 0.0f;
    const float vBottom = uvSize.y + uvTL.y;
    const float u1 = uvTL.x + uvSize.x;
    const float vTop = uvTL.y + 0.0f;
    uv[0] = CVector2(u0, vBottom);
    uv[1] = CVector2(u1, vBottom);
    uv[2] = CVector2(u1, vTop);
    uv[3] = CVector2(uvTL.x, uvTL.y);
}

// The 2D constructors: top-left corner + full size -> centre (z = 0) + half.
void cornerToCenter(const CVector2& topLeft, const CVector2& size, Vec3& center, CVector2& half) {
    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    center = Vec3(topLeft.x + hx, topLeft.y - hy, 0.0f);
    half = CVector2(hx, hy);
}

}  // namespace

// ELF 0x36fefc (Ghidra 0x37fefc)
void getQuadVertices(Vec3 outPos[4], CVector2 outUV[4], const Vec3& c, const CVector2& half,
                     const CVector2& uvTL, const CVector2& uvSize, float yaw, const Mat3RowMajor* rot) {
    const float hx = half.x, hy = half.y;
    Vec3 p[4] = {Vec3(-hx, -hy, 0.0f), Vec3(hx, -hy, 0.0f), Vec3(hx, hy, 0.0f), Vec3(-hx, hy, 0.0f)};
    if (yaw == 0.0f) {
        if (rot == nullptr) {
            for (int i = 0; i < 4; ++i) outPos[i] = Vec3(c.x + p[i].x, c.y + p[i].y, c.z + 0.0f);
        } else {
            const float* m = rot->m;
            for (int i = 0; i < 4; ++i) {
                const Vec3 q(m[0] * p[i].x + m[1] * p[i].y + m[2] * p[i].z,
                             m[3] * p[i].x + m[4] * p[i].y + m[5] * p[i].z,
                             m[6] * p[i].x + m[7] * p[i].y + m[8] * p[i].z);
                outPos[i] = Vec3(q.x + c.x, q.y + c.y, q.z + c.z);
            }
        }
    } else {
        // CMatrix3::FromAxisAngle(UNIT_Z, yaw), times rot when given [?]: the
        // product order with rot is not verified (no caller passes both).
        const float cs = std::cos(yaw), sn = std::sin(yaw);
        float m[9] = {cs, -sn, 0.0f, sn, cs, 0.0f, 0.0f, 0.0f, 1.0f};
        if (rot != nullptr) {
            float r[9];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    r[i * 3 + j] = rot->m[i * 3 + 0] * m[0 * 3 + j] + rot->m[i * 3 + 1] * m[1 * 3 + j] +
                                   rot->m[i * 3 + 2] * m[2 * 3 + j];
            for (int i = 0; i < 9; ++i) m[i] = r[i];
        }
        for (int i = 0; i < 4; ++i) {
            const Vec3 q(m[0] * p[i].x + m[1] * p[i].y + m[2] * p[i].z, m[3] * p[i].x + m[4] * p[i].y + m[5] * p[i].z,
                         m[6] * p[i].x + m[7] * p[i].y + m[8] * p[i].z);
            outPos[i] = Vec3(q.x + c.x, q.y + c.y, q.z + c.z);
        }
    }
    outUV[0] = CVector2(uvTL.x, uvTL.y + uvSize.y);
    outUV[1] = CVector2(uvTL.x + uvSize.x, uvTL.y + uvSize.y);
    outUV[2] = CVector2(uvTL.x + uvSize.x, uvTL.y);
    outUV[3] = CVector2(uvTL.x, uvTL.y);
}

// ---------------------------------------------------------------- CQuadPosUV

// Ghidra 0x382c38
CQuadPosUV::CQuadPosUV(const Vec3& center, const CVector2& half, const CVector2& uvTL, const CVector2& uvSize,
                       float yaw) {
    Vec3 p[4];
    CVector2 uv[4];
    getQuadVertices(p, uv, center, half, uvTL, uvSize, yaw);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].unused[0] = v[i].unused[1] = v[i].unused[2] = 0.0f;
    }
}

// Ghidra 0x3829f0
CQuadPosUV::CQuadPosUV(const CVector2& topLeft, const CVector2& size, const CVector2& uvTL,
                       const CVector2& uvSize) {
    Vec3 c;
    CVector2 half;
    cornerToCenter(topLeft, size, c, half);
    *this = CQuadPosUV(c, half, uvTL, uvSize, 0.0f);
}

// Ghidra 0x3824e0
CQuadPosUV::CQuadPosUV(float z, const float m[16], const CVector2& half, const CVector2& uvTL,
                       const CVector2& uvSize) {
    Vec3 p[4];
    CVector2 uv[4];
    matrixCorners(z, m, half, uvTL, uvSize, p, uv);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].unused[0] = v[i].unused[1] = v[i].unused[2] = 0.0f;
    }
}

// Ghidra 0x382df4: a band of the given width from `from` to `to`.
CQuadPosUV::CQuadPosUV(const Vec3& from, const Vec3& to, float width, const CVector2& uvTL,
                       const CVector2& uvSize) {
    const float h = width * 0.5f;
    float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 1e-8f) {
        const float inv = 1.0f / len;
        dx = dx * inv;
        dy = dy * inv;
        dz = dz * inv;
    }
    // d x UNIT_Z
    const Vec3 off(h * (1.0f * dy - 0.0f * dz), h * (0.0f * dz - 1.0f * dx), h * (0.0f * dx - 0.0f * dy));
    const Vec3 p0(off.x + to.x, off.y + to.y, off.z + to.z);
    const Vec3 p1(to.x - off.x, to.y - off.y, to.z - off.z);
    const Vec3 p2(from.x - off.x, from.y - off.y, from.z - off.z);
    const Vec3 p3(off.x + from.x, off.y + from.y, off.z + from.z);
    const float uR = uvTL.x + uvSize.x, vB = uvTL.y + uvSize.y;
    const Vec3 pos[6] = {p0, p1, p2, p0, p2, p3};
    const CVector2 uv[6] = {CVector2(uR, uvTL.y), CVector2(uR, vB), CVector2(uvTL.x, vB),
                            CVector2(uR, uvTL.y), CVector2(uvTL.x, vB), CVector2(uvTL.x, uvTL.y)};
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], pos[i], uv[i]);
        v[i].unused[0] = v[i].unused[1] = v[i].unused[2] = 0.0f;
    }
}

// ----------------------------------------------------------- CQuadPosUVColor

// Ghidra 0x3821d8
CQuadPosUVColor::CQuadPosUVColor(const Vec3& center, const CVector2& half, const CVector2& uvTL,
                                 const CVector2& uvSize, const Vec3& color, float yaw) {
    Vec3 p[4];
    CVector2 uv[4];
    getQuadVertices(p, uv, center, half, uvTL, uvSize, yaw);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].color[0] = color.x;
        v[i].color[1] = color.y;
        v[i].color[2] = color.z;
    }
}

// Ghidra 0x381e48
CQuadPosUVColor::CQuadPosUVColor(const CVector2& topLeft, const CVector2& size, const CVector2& uvTL,
                                 const CVector2& uvSize, const Vec3& color) {
    Vec3 c;
    CVector2 half;
    cornerToCenter(topLeft, size, c, half);
    *this = CQuadPosUVColor(c, half, uvTL, uvSize, color, 0.0f);
}

// ---------------------------------------------------------- CQuadPosUVColor4

// Ghidra 0x3813e0
CQuadPosUVColor4::CQuadPosUVColor4(const Vec3& center, const CVector2& half, const CVector2& uvTL,
                                   const CVector2& uvSize, const Vec3& color, float alpha, float yaw,
                                   const Mat3RowMajor* rot) {
    Vec3 p[4];
    CVector2 uv[4];
    getQuadVertices(p, uv, center, half, uvTL, uvSize, yaw, rot);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].color[0] = color.x;
        v[i].color[1] = color.y;
        v[i].color[2] = color.z;
        v[i].color[3] = alpha;
        for (float& u : v[i].unused) u = 0.0f;
    }
}

// Ghidra 0x3816f8 / 0x381aa0
CQuadPosUVColor4::CQuadPosUVColor4(const CVector2& topLeft, const CVector2& size, const CVector2& uvTL,
                                   const CVector2& uvSize, const Vec3& color, float alpha, float yaw) {
    Vec3 c;
    CVector2 half;
    cornerToCenter(topLeft, size, c, half);
    *this = CQuadPosUVColor4(c, half, uvTL, uvSize, color, alpha, yaw);
}

// ----------------------------------------------------- CQuadPosUVColor4Width

// Ghidra 0x38104c
CQuadPosUVColor4Width::CQuadPosUVColor4Width(const Vec3& center, float r, float width, const Vec3& color,
                                             float alpha) {
    const float w = width / r;
    Vec3 p[4];
    CVector2 uv[4];
    getQuadVertices(p, uv, center, CVector2(r, r), CVector2(-1.0f, -1.0f), CVector2(2.0f, 2.0f), 0.0f);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].color[0] = color.x;
        v[i].color[1] = color.y;
        v[i].color[2] = color.z;
        v[i].color[3] = alpha;
        v[i].width = w;
        for (float& u : v[i].unused) u = 0.0f;
    }
}

// ------------------------------------------------------------- CQuadPosUVTTL

// Ghidra 0x38093c
CQuadPosUVTTL::CQuadPosUVTTL(const Vec3& center, const CVector2& half, const CVector2& uvTL,
                             const CVector2& uvSize, float ttl, float yaw, const Mat3RowMajor* rot) {
    Vec3 p[4];
    CVector2 uv[4];
    getQuadVertices(p, uv, center, half, uvTL, uvSize, yaw, rot);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].ttl = ttl;
        v[i].unused[0] = v[i].unused[1] = 0.0f;
    }
}

// Ghidra 0x380b1c
CQuadPosUVTTL::CQuadPosUVTTL(float z, const float m[16], const CVector2& half, const CVector2& uvTL,
                             const CVector2& uvSize, float ttl) {
    Vec3 p[4];
    CVector2 uv[4];
    matrixCorners(z, m, half, uvTL, uvSize, p, uv);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kCorner[i]], uv[kCorner[i]]);
        v[i].ttl = ttl;
        v[i].unused[0] = v[i].unused[1] = 0.0f;
    }
}

}  // namespace py
