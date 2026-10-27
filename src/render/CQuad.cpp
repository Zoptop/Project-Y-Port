// CQuad.cpp - see CQuad.h.
#include "render/CQuad.h"

#include "core/FloatBits.h"
#include "core/fdlibm.h"

namespace py {

namespace {
// q0 q1 q2 q0 q2 q3
constexpr int kOrder[6] = {0, 1, 2, 0, 2, 3};

template <class V>
void setPosUV(V& v, const CVector3& p, const CVector2& uv) {
    v.pos[0] = p.x;
    v.pos[1] = p.y;
    v.pos[2] = p.z;
    v.uv[0] = uv.x;
    v.uv[1] = uv.y;
}

void cornerUVs(CVector2* uv, const CVector2& tl, const CVector2& size) {
    uv[0] = CVector2(tl.x, tl.y + size.y);
    uv[1] = CVector2(tl.x + size.x, tl.y + size.y);
    uv[2] = CVector2(tl.x + size.x, tl.y);
    uv[3] = CVector2(tl.x, tl.y);
}
}  // namespace

// Ghidra 0x37fefc (ELF 0x36fefc, from the disassembly). The rotation axis
// for yaw != 0 is +z (CMatrix3::FromAxisAngle on a global axis constant).
void getQuadVertices(CVector3* pos, CVector2* uv, const CVector3& center, const CVector2& halfSize,
                     const CVector2& uvTopLeft, const CVector2& uvSize, float yaw, const CMatrix3* rot) {
    const Real sx = halfSize.x, sy = halfSize.y;
    pos[0] = CVector3(-sx, -sy, 0.0f);
    pos[1] = CVector3(sx, -sy, 0.0f);
    pos[2] = CVector3(sx, sy, 0.0f);
    pos[3] = CVector3(-sx, sy, 0.0f);
    if (yaw == 0.0f) {
        if (rot) {
            for (int i = 0; i < 4; ++i) {
                const CVector3 r = *rot * pos[i];
                pos[i] = CVector3(r.x + center.x, r.y + center.y, r.z + center.z);
            }
        } else {
            for (int i = 0; i < 4; ++i)
                pos[i] = CVector3(center.x + pos[i].x, center.y + pos[i].y, center.z + 0.0f);
        }
    } else {
        CMatrix3 r;
        r.FromAxisAngle(CVector3::UNIT_Z, yaw);
        const CMatrix3 m = rot ? (*rot * r) : r;
        for (int i = 0; i < 4; ++i) {
            const CVector3 p = m * pos[i];
            pos[i] = CVector3(p.x + center.x, p.y + center.y, p.z + center.z);
        }
    }
    cornerUVs(uv, uvTopLeft, uvSize);
}

// CQuadPosUV / CQuadPosUVTTL (float z, CMatrix4&, ...): Ghidra 0x3824e0 / 0x380b1c
void getQuadVerticesTransform(CVector3* pos, CVector2* uv, float z, const CMatrix4& m, const CVector2& halfSize,
                              const CVector2& uvTopLeft, const CVector2& uvSize) {
    const Real sx = halfSize.x, sy = halfSize.y;
    const Real cx[4] = {-sx, sx, sx, -sx};
    const Real cy[4] = {-sy, -sy, sy, sy};
    for (int i = 0; i < 4; ++i) {
        pos[i].x = cx[i] * m.m[0][0] + cy[i] * m.m[0][1] + Real(z) * m.m[0][2] + m.m[0][3];
        pos[i].y = cx[i] * m.m[1][0] + cy[i] * m.m[1][1] + Real(z) * m.m[1][2] + m.m[1][3];
        pos[i].z = cx[i] * m.m[2][0] + cy[i] * m.m[2][1] + Real(z) * m.m[2][2] + m.m[2][3];
    }
    uv[0] = CVector2(uvTopLeft.x + 0.0f, uvSize.y + uvTopLeft.y);
    uv[1] = CVector2(uvTopLeft.x + uvSize.x, uvSize.y + uvTopLeft.y);
    uv[2] = CVector2(uvTopLeft.x + uvSize.x, uvTopLeft.y + 0.0f);
    uv[3] = CVector2(uvTopLeft.x, uvTopLeft.y);
}

CQuadPosUV::CQuadPosUV(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                       const CVector2& uvSize, float yaw) {
    CVector3 p[4];
    CVector2 t[4];
    getQuadVertices(p, t, center, halfSize, uvTopLeft, uvSize, yaw, nullptr);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kOrder[i]], t[kOrder[i]]);
        v[i].pad[0] = v[i].pad[1] = v[i].pad[2] = 0.0f;
    }
}

CQuadPosUV::CQuadPosUV(float z, const CMatrix4& m, const CVector2& halfSize, const CVector2& uvTopLeft,
                       const CVector2& uvSize) {
    CVector3 p[4];
    CVector2 t[4];
    getQuadVerticesTransform(p, t, z, m, halfSize, uvTopLeft, uvSize);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kOrder[i]], t[kOrder[i]]);
        v[i].pad[0] = v[i].pad[1] = v[i].pad[2] = 0.0f;
    }
}

// Ghidra 0x382df4. d = normalize(end - start) (if longer than 1e-8),
// side = (Z.z dy - Z.y dz, Z.x dz - Z.z dx, Z.y dx - Z.x dy) * width / 2;
// vertices end+s, end-s, start-s, end+s, start-s, start+s.
CQuadPosUV::CQuadPosUV(const CVector3& start, const CVector3& end, float width, const CVector2& uvTopLeft,
                       const CVector2& uvSize) {
    const Real h = Real(width) * 0.5f;
    Real dx = end.x - start.x, dy = end.y - start.y, dz = end.z - start.z;
    const Real len = fdlibm::sqrtf(dx * dx + dy * dy + dz * dz);
    if (len > bitsf(0x322bcc77)) {
        const Real inv = 1.0f / len;
        dx = dx * inv;
        dy = dy * inv;
        dz = dz * inv;
    }
    const CVector3& Z = CVector3::UNIT_Z;
    const Real sx = h * (Z.z * dy - Z.y * dz);
    const Real sy = h * (Z.x * dz - Z.z * dx);
    const Real sz = h * (Z.y * dx - Z.x * dy);
    const CVector3 ep(sx + end.x, sy + end.y, sz + end.z);
    const CVector3 em(end.x - sx, end.y - sy, end.z - sz);
    const CVector3 sm(start.x - sx, start.y - sy, start.z - sz);
    const CVector3 sp(sx + start.x, sy + start.y, sz + start.z);
    const Real u0 = uvTopLeft.x, v0 = uvTopLeft.y;
    const Real u1 = u0 + uvSize.x, v1 = v0 + uvSize.y;
    const CVector3 p[6] = {ep, em, sm, ep, sm, sp};
    const CVector2 t[6] = {CVector2(u1, v0), CVector2(u1, v1), CVector2(u0, v1),
                           CVector2(u1, v0), CVector2(u0, v1), CVector2(u0, v0)};
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[i], t[i]);
        v[i].pad[0] = v[i].pad[1] = v[i].pad[2] = 0.0f;
    }
}

CQuadPosUVTTL::CQuadPosUVTTL(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                             const CVector2& uvSize, float ttl, float yaw, const CMatrix3* rot) {
    CVector3 p[4];
    CVector2 t[4];
    getQuadVertices(p, t, center, halfSize, uvTopLeft, uvSize, yaw, rot);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kOrder[i]], t[kOrder[i]]);
        v[i].ttl = ttl;
        v[i].pad[0] = v[i].pad[1] = 0.0f;
    }
}

CQuadPosUVTTL::CQuadPosUVTTL(float z, const CMatrix4& m, const CVector2& halfSize, const CVector2& uvTopLeft,
                             const CVector2& uvSize, float ttl) {
    CVector3 p[4];
    CVector2 t[4];
    getQuadVerticesTransform(p, t, z, m, halfSize, uvTopLeft, uvSize);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kOrder[i]], t[kOrder[i]]);
        v[i].ttl = ttl;
        v[i].pad[0] = v[i].pad[1] = 0.0f;
    }
}

CQuadPosUVColor::CQuadPosUVColor(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                                 const CVector2& uvSize, const CVector3& color, float yaw) {
    CVector3 p[4];
    CVector2 t[4];
    getQuadVertices(p, t, center, halfSize, uvTopLeft, uvSize, yaw, nullptr);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kOrder[i]], t[kOrder[i]]);
        v[i].color[0] = color.x;
        v[i].color[1] = color.y;
        v[i].color[2] = color.z;
    }
}

CQuadPosUVColor::CQuadPosUVColor(const CVector2& topLeft, const CVector2& size, const CVector2& uvTopLeft,
                                 const CVector2& uvSize, const CVector3& color)
    : CQuadPosUVColor(CVector3(topLeft.x + size.x * 0.5f, topLeft.y - size.y * 0.5f, 0.0f),
                      CVector2(size.x * 0.5f, size.y * 0.5f), uvTopLeft, uvSize, color, 0.0f) {}

CQuadPosUVColor4::CQuadPosUVColor4(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                                   const CVector2& uvSize, const CVector3& color, float alpha, float yaw,
                                   const CMatrix3* rot) {
    CVector3 p[4];
    CVector2 t[4];
    getQuadVertices(p, t, center, halfSize, uvTopLeft, uvSize, yaw, rot);
    for (int i = 0; i < 6; ++i) {
        setPosUV(v[i], p[kOrder[i]], t[kOrder[i]]);
        v[i].color[0] = color.x;
        v[i].color[1] = color.y;
        v[i].color[2] = color.z;
        v[i].color[3] = alpha;
        v[i].width = 0.0f;
        for (float& f : v[i].pad) f = 0.0f;
    }
}

CQuadPosUVColor4::CQuadPosUVColor4(const CVector2& topLeft, const CVector2& size, const CVector2& uvTopLeft,
                                   const CVector2& uvSize, const CVector3& color, float alpha, float yaw)
    : CQuadPosUVColor4(CVector3(topLeft.x + size.x * 0.5f, topLeft.y - size.y * 0.5f, 0.0f),
                       CVector2(size.x * 0.5f, size.y * 0.5f), uvTopLeft, uvSize, color, alpha, yaw, nullptr) {}

}  // namespace py
