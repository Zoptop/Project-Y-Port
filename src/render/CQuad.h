// CQuad.h - ports of the CPU quad types the original batches and draws
// from client memory: CQuadPosUV, CQuadPosUVColor, CQuadPosUVColor4,
// CQuadPosUVTTL, and the free function getQuadVertices they all use.
//
// A quad is two triangles, six vertices (q0 q1 q2) (q0 q2 q3), written in
// that order into the vertex buffer, so a buffer of n quads is drawn with
// glDrawArrays(GL_TRIANGLES, 0, 6n). Corner layout (getQuadVertices):
//   q0 = centre + (-sx, -sy)   uv (u0,      v0 + dv)
//   q1 = centre + ( sx, -sy)   uv (u0 + du, v0 + dv)
//   q2 = centre + ( sx,  sy)   uv (u0 + du, v0)
//   q3 = centre + (-sx,  sy)   uv (u0,      v0)
// where (sx, sy) is the half size, optionally turned by `yaw` about +z and
// by a 3x3 matrix (the camera's billboard matrix for particles).
//
// Vertex layouts (attribute offsets as CGLSLProgram::startDrawLocal* set them):
//   CVertexPosUV       32 bytes: POSITION @0, TEXCOORD0 @12
//   CVertexPosUVTTL    32 bytes: POSITION @0, TEXCOORD0 @12, TTL @20
//   CVertexPosUVColor  32 bytes: POSITION @0, TEXCOORD0 @12, COLR (rgb) @20
//   CVertexPosUVColor4 64 bytes: POSITION @0, TEXCOORD0 @12, COLR (rgba) @20, WIDTH @36
//
// Ghidra: getQuadVertices 0x37fefc (absorbed into the CParticleSystemSmoke
// listing by Ghidra; ELF 0x36fefc), CQuadPosUVTTL 0x38093c / 0x380b1c,
// CQuadPosUVColor4 0x3813e0 / 0x3816f8 / 0x381aa0, CQuadPosUVColor 0x381e48 /
// 0x3821d8, CQuadPosUV 0x3824e0 / 0x3829f0 / 0x382c38 / 0x382df4.
// The sort order of CQuadPosUVColor4 (std::sort in CParticleManager::draw) is
// the z of the first vertex, ascending (std::__unguarded_linear_insert, ELF
// 0x35d378).
#pragma once

#include <cstdint>

#include "core/CMatrix3.h"
#include "core/CMatrix4.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "render/Math3D.h"

namespace py {

struct CVertexPosUV {
    float pos[3];
    float uv[2];
    float pad[3];
};
struct CVertexPosUVTTL {
    float pos[3];
    float uv[2];
    float ttl;
    float pad[2];
};
struct CVertexPosUVColor {
    float pos[3];
    float uv[2];
    float color[3];
};
struct CVertexPosUVColor4 {
    float pos[3];
    float uv[2];
    float color[4];
    float width;
    float pad[6];
};
static_assert(sizeof(CVertexPosUV) == 32 && sizeof(CVertexPosUVTTL) == 32 && sizeof(CVertexPosUVColor) == 32,
              "32-byte vertices");
// the circle shaders' vertex (CQuadPosUVColor4Width): WIDTH @0x24
struct CVertexPosUvColor4Width {
    float pos[3];
    float uv[2];
    float color[4];
    float width;
    float pad[6];
};
static_assert(sizeof(CVertexPosUVColor4) == 64 && sizeof(CVertexPosUvColor4Width) == 64, "64-byte vertices");

// Ghidra 0x37fefc. pos[4] / uv[4] in corner order q0..q3 (see above).
void getQuadVertices(CVector3* pos, CVector2* uv, const CVector3& center, const CVector2& halfSize,
                     const CVector2& uvTopLeft, const CVector2& uvSize, float yaw, const CMatrix3* rot);

// The two "transform" constructors (CQuadPosUV / CQuadPosUVTTL(float z,
// CMatrix4&, ...)): corners (+-sx, +-sy, z) through the affine part of m.
void getQuadVerticesTransform(CVector3* pos, CVector2* uv, float z, const CMatrix4& m, const CVector2& halfSize,
                              const CVector2& uvTopLeft, const CVector2& uvSize);

struct CQuadPosUV {
    CVertexPosUV v[6];
    CQuadPosUV() = default;
    CQuadPosUV(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft, const CVector2& uvSize,
               float yaw);
    CQuadPosUV(float z, const CMatrix4& m, const CVector2& halfSize, const CVector2& uvTopLeft,
               const CVector2& uvSize);
    // the same with a row-major float[16] (UI code)
    CQuadPosUV(float z, const float rowMajor4x4[16], const CVector2& halfSize, const CVector2& uvTopLeft,
               const CVector2& uvSize);
    // Vec3 centre (UI code)
    CQuadPosUV(const Vec3& center, const CVector2& halfSize, const CVector2& uvTopLeft, const CVector2& uvSize,
               float yaw);
    // Ghidra 0x3829f0 (screen space: top-left corner + full size, z = 0)
    CQuadPosUV(const CVector2& topLeft, const CVector2& size, const CVector2& uvTopLeft, const CVector2& uvSize);
    // Ghidra 0x382df4: a strip of `width` from `start` to `end`, lying flat
    // (its side is (end - start) x +z); u runs across, v along the strip.
    CQuadPosUV(const CVector3& start, const CVector3& end, float width, const CVector2& uvTopLeft,
               const CVector2& uvSize);
};

struct CQuadPosUVTTL {
    CVertexPosUVTTL v[6];
    CQuadPosUVTTL() = default;
    // Ghidra 0x38093c
    CQuadPosUVTTL(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                  const CVector2& uvSize, float ttl, float yaw, const CMatrix3* rot);
    // Ghidra 0x380b1c
    CQuadPosUVTTL(float z, const CMatrix4& m, const CVector2& halfSize, const CVector2& uvTopLeft,
                  const CVector2& uvSize, float ttl);
    CQuadPosUVTTL(float z, const float rowMajor4x4[16], const CVector2& halfSize, const CVector2& uvTopLeft,
                  const CVector2& uvSize, float ttl);
};

struct CQuadPosUVColor {
    CVertexPosUVColor v[6];
    CQuadPosUVColor() = default;
    // Ghidra 0x3821d8 (world space: centre + half size)
    CQuadPosUVColor(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                    const CVector2& uvSize, const CVector3& color, float yaw);
    // Ghidra 0x381e48 (screen space: top-left corner + full size, z = 0)
    CQuadPosUVColor(const CVector2& topLeft, const CVector2& size, const CVector2& uvTopLeft,
                    const CVector2& uvSize, const CVector3& color);
};

struct CQuadPosUVColor4 {
    CVertexPosUVColor4 v[6];
    CQuadPosUVColor4() = default;
    // Ghidra 0x3813e0
    CQuadPosUVColor4(const CVector3& center, const CVector2& halfSize, const CVector2& uvTopLeft,
                     const CVector2& uvSize, const CVector3& color, float alpha, float yaw, const CMatrix3* rot);
    // Ghidra 0x3816f8 / 0x381aa0 (screen space: top-left corner + full size)
    CQuadPosUVColor4(const CVector2& topLeft, const CVector2& size, const CVector2& uvTopLeft,
                     const CVector2& uvSize, const CVector3& color, float alpha, float yaw = 0.0f);
    // Vec3 centre and colour (UI / text code)
    CQuadPosUVColor4(const Vec3& center, const CVector2& halfSize, const CVector2& uvTopLeft, const CVector2& uvSize,
                     const Vec3& color, float alpha, float yaw);
    // std::sort order (ELF 0x35d378): z of the first vertex
    bool operator<(const CQuadPosUVColor4& o) const { return v[0].pos[2] < o.v[0].pos[2]; }
};

struct CQuadPosUVColor4Width {
    CVertexPosUvColor4Width v[6];
    CQuadPosUVColor4Width() = default;
    // Ghidra 0x38104c: ring quad (circle shaders). half = (r, r), uv from
    // (-1, -1), size (2, 2), WIDTH = width / r.
    CQuadPosUVColor4Width(const Vec3& center, float r, float width, const Vec3& color, float alpha);
};
static_assert(sizeof(CQuadPosUVColor4Width) == 0x180, "quad size as in the original");

static_assert(sizeof(CQuadPosUV) == 0xc0 && sizeof(CQuadPosUVTTL) == 0xc0 && sizeof(CQuadPosUVColor) == 0xc0,
              "quad sizes as in the original");
static_assert(sizeof(CQuadPosUVColor4) == 0x180, "quad size as in the original");

}  // namespace py
