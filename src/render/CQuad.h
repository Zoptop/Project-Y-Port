// CQuad.h - the CPU quad types of ProjectY (CQuadPosUV, CQuadPosUVColor,
// CQuadPosUVColor4, CQuadPosUVColor4Width, CQuadPosUVTTL) and their vertex
// layouts (CVertexPosUV, ...), plus the global getQuadVertices helper.
//
// A quad is six vertices (two triangles, GL_TRIANGLES) in the order
//   v0 = p0, v1 = p1, v2 = p2, v3 = p0, v4 = p2, v5 = p3
// with p0 = bottom-left (-hx,-hy), p1 = bottom-right (+hx,-hy),
// p2 = top-right (+hx,+hy), p3 = top-left (-hx,+hy) around the centre, and
// UVs p0 = (tl.x, tl.y+size.y), p1 = (tl.x+size.x, tl.y+size.y),
// p2 = (tl.x+size.x, tl.y), p3 = (tl.x, tl.y). Textures are uploaded top row
// first, so tl.y is the top of the image and "up" (+y) shows the image top.
//
// Strides match the original exactly (the structs are memcpy'd into CPU
// buffers and handed to CGLSLProgram::startDrawLocal*):
//   CVertexPosUV / PosUVColor / PosUVTTL : 32 bytes  (quad 0xc0)
//   CVertexPosUVColor4 / PosUvColor4Width : 64 bytes (quad 0x180)
//
// Positions are rendering data, not simulation state: plain float maths.
#pragma once

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

struct CVertexPosUV {
    float pos[3];
    float uv[2];
    float unused[3];
};
static_assert(sizeof(CVertexPosUV) == 32, "stride 32");

struct CVertexPosUVColor {
    float pos[3];
    float uv[2];
    float color[3];   // COLR @0x14 (vec3)
};
static_assert(sizeof(CVertexPosUVColor) == 32, "stride 32");

struct CVertexPosUVTTL {
    float pos[3];
    float uv[2];
    float ttl;        // TTL @0x14
    float unused[2];
};
static_assert(sizeof(CVertexPosUVTTL) == 32, "stride 32");

struct CVertexPosUVColor4 {
    float pos[3];
    float uv[2];
    float color[4];   // COLR @0x14 (vec4: rgb + alpha @0x20)
    float unused[7];
};
static_assert(sizeof(CVertexPosUVColor4) == 64, "stride 64");

struct CVertexPosUvColor4Width {
    float pos[3];
    float uv[2];
    float color[4];   // COLR @0x14
    float width;      // WIDTH @0x24
    float unused[6];
};
static_assert(sizeof(CVertexPosUvColor4Width) == 64, "stride 64");

// Row-major 3x3 rotation (CMatrix3 layout), optional extra transform.
struct Mat3RowMajor {
    float m[9];
};

// _Z15getQuadVertices... (ELF 0x36fefc, Ghidra 0x37fefc).
// outPos[4], outUV[4]: corners p0..p3 as described above.
// half = half extents. With yaw == 0 and rot == null the corners are
// centre +- half; otherwise rotated by FromAxisAngle(UNIT_Z, yaw) (and rot).
void getQuadVertices(Vec3 outPos[4], CVector2 outUV[4], const Vec3& center, const CVector2& half,
                     const CVector2& uvTopLeft, const CVector2& uvSize, float yaw,
                     const Mat3RowMajor* rot = nullptr);

struct CQuadPosUV {
    CVertexPosUV v[6];
    CQuadPosUV() = default;
    // (centre, half extents, uv top-left, uv size, yaw) - Ghidra 0x382c38
    CQuadPosUV(const Vec3& center, const CVector2& half, const CVector2& uvTL, const CVector2& uvSize,
               float yaw);
    // (top-left corner, full size, uv top-left, uv size) at z = 0 - Ghidra 0x3829f0
    CQuadPosUV(const CVector2& topLeft, const CVector2& size, const CVector2& uvTL, const CVector2& uvSize);
    // (z, transform, half, uv top-left, uv size): corners = M * (+-hx, +-hy, z),
    // M a row-major 4x4 (CMatrix4) - Ghidra 0x3824e0
    CQuadPosUV(float z, const float rowMajor4x4[16], const CVector2& half, const CVector2& uvTL,
               const CVector2& uvSize);
    // Beam between two points with the given width (UNIT_Z x dir) - Ghidra 0x382df4
    CQuadPosUV(const Vec3& from, const Vec3& to, float width, const CVector2& uvTL, const CVector2& uvSize);
};
static_assert(sizeof(CQuadPosUV) == 0xc0, "quad size");

struct CQuadPosUVColor {
    CVertexPosUVColor v[6];
    CQuadPosUVColor() = default;
    // Ghidra 0x381e48: (top-left corner, size, uvTL, uvSize, colour), z = 0
    CQuadPosUVColor(const CVector2& topLeft, const CVector2& size, const CVector2& uvTL, const CVector2& uvSize,
                    const Vec3& color);
    // Ghidra 0x3821d8: (centre, half, uvTL, uvSize, colour, yaw)
    CQuadPosUVColor(const Vec3& center, const CVector2& half, const CVector2& uvTL, const CVector2& uvSize,
                    const Vec3& color, float yaw);
};
static_assert(sizeof(CQuadPosUVColor) == 0xc0, "quad size");

struct CQuadPosUVColor4 {
    CVertexPosUVColor4 v[6];
    CQuadPosUVColor4() = default;
    // Ghidra 0x3813e0: (centre, half, uvTL, uvSize, colour, alpha, yaw, rot)
    CQuadPosUVColor4(const Vec3& center, const CVector2& half, const CVector2& uvTL, const CVector2& uvSize,
                     const Vec3& color, float alpha, float yaw, const Mat3RowMajor* rot = nullptr);
    // Ghidra 0x3816f8 / 0x381aa0: (top-left corner, size, uvTL, uvSize, colour, alpha[, yaw]), z = 0
    CQuadPosUVColor4(const CVector2& topLeft, const CVector2& size, const CVector2& uvTL, const CVector2& uvSize,
                     const Vec3& color, float alpha, float yaw = 0.0f);
};
static_assert(sizeof(CQuadPosUVColor4) == 0x180, "quad size");

struct CQuadPosUVColor4Width {
    CVertexPosUvColor4Width v[6];
    CQuadPosUVColor4Width() = default;
    // Ghidra 0x38104c: ring quad (circle shaders). half = (r, r), uv from (-1,-1)
    // size (2,2), WIDTH = width / r.
    CQuadPosUVColor4Width(const Vec3& center, float r, float width, const Vec3& color, float alpha);
};
static_assert(sizeof(CQuadPosUVColor4Width) == 0x180, "quad size");

struct CQuadPosUVTTL {
    CVertexPosUVTTL v[6];
    CQuadPosUVTTL() = default;
    // Ghidra 0x38093c: (centre, half, uvTL, uvSize, ttl, yaw, rot)
    CQuadPosUVTTL(const Vec3& center, const CVector2& half, const CVector2& uvTL, const CVector2& uvSize, float ttl,
                  float yaw, const Mat3RowMajor* rot = nullptr);
    // Ghidra 0x380b1c: (z, row-major 4x4 transform, half, uvTL, uvSize, ttl)
    CQuadPosUVTTL(float z, const float rowMajor4x4[16], const CVector2& half, const CVector2& uvTL,
                  const CVector2& uvSize, float ttl);
};
static_assert(sizeof(CQuadPosUVTTL) == 0xc0, "quad size");

}  // namespace py
