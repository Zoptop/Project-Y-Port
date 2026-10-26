// Tests for the OGRE-derived math classes (CVector2/3, CQuaternion, CMatrix3,
// CMatrix4, CPlane) and py::Real. The bit-exactness against the original
// binary is checked by tools/emu/check_vecmath.py; these tests pin values that
// can be derived by hand (exact small-integer arithmetic) and the original's
// quirks, so a regression shows up without the emulator.
#include <cstdint>

#include "../src/core/CMath.h"
#include "../src/core/CMatrix3.h"
#include "../src/core/CMatrix4.h"
#include "../src/core/CPlane.h"
#include "../src/core/CQuaternion.h"
#include "../src/core/CVector2.h"
#include "../src/core/CVector3.h"
#include "../src/core/FloatBits.h"
#include "test_main.h"

using py::bitsf;
using py::fbits;

namespace {
void ensureTables() {
    static py::CMath* m = nullptr;
    if (!m) m = new py::CMath(4096);
}
bool eq3(const py::CVector3& v, float x, float y, float z) { return v.x == x && v.y == y && v.z == z; }
}  // namespace

// Same memory layout as the original classes (they are copied around as raw
// floats by game code and by the emulator checks).
static_assert(sizeof(py::Real) == 4, "Real must be a plain float");
static_assert(sizeof(py::CVector3) == 12, "CVector3 layout");
static_assert(sizeof(py::CQuaternion) == 16, "CQuaternion layout");
static_assert(sizeof(py::CMatrix3) == 36, "CMatrix3 layout");
static_assert(sizeof(py::CMatrix4) == 64, "CMatrix4 layout");
static_assert(sizeof(py::CPlane) == 16, "CPlane layout");

TEST(vecmath_vector_basics) {
    // 3-4-5 triangles: every intermediate is an exact small integer.
    CHECK(py::CVector2(0.0f, 0.0f).distance(py::CVector2(3.0f, -4.0f)) == 5.0f);
    CHECK(py::CVector3(1.0f, 2.0f, 2.0f).distance(py::CVector3(1.0f, -1.0f, -2.0f)) == 5.0f);

    // normalise returns the old length; 1/5 = 0.2f (0x3e4ccccd), 3*0.2f and
    // 4*0.2f round to 0.6f (0x3f19999a) and 0.8f (0x3f4ccccd).
    py::CVector3 v(3.0f, 4.0f, 0.0f);
    CHECK(v.normalise() == 5.0f);
    CHECK(fbits(v.x) == 0x3f19999au && fbits(v.y) == 0x3f4ccccdu && fbits(v.z) == 0u);

    // Below 1e-08 nothing is divided: zero and tiny vectors stay as they are.
    py::CVector3 z(0.0f, 0.0f, 0.0f);
    CHECK(z.normalise() == 0.0f && eq3(z, 0.0f, 0.0f, 0.0f));
    py::CVector3 t(1e-9f, 0.0f, 0.0f);
    CHECK(t.normalise() == 1e-9f && t.x == 1e-9f);

    CHECK(eq3(py::CVector3(1.0f, 0.0f, 0.0f).crossProduct(py::CVector3(0.0f, 1.0f, 0.0f)), 0.0f, 0.0f, 1.0f));
}

TEST(vecmath_real_follows_libgcc) {
    // NaN rules of the original's soft-float: 0*inf -> 0x7fc00000 with the
    // sign of the infinity (x86 hardware gives 0xffc00000).
    const py::Real zero = 0.0f, inf = bitsf(0x7f800000u);
    CHECK(fbits(zero * inf) == 0x7fc00000u);
    CHECK(fbits(inf - inf) == 0x7fc00000u);
    CHECK(fbits(-inf + inf) == 0xffc00000u);   // inf + -inf takes the first operand's sign

    // The original __aeabi_fadd mis-rounds sums that land in the subnormal
    // range (values taken from the original binary under emulation):
    CHECK(fbits(py::lgcc_fadd(bitsf(0x80da6de2u), bitsf(0x0086959bu))) == 0x8027b08eu);   // IEEE: 0x8053d847
    CHECK(fbits(py::lgcc_fadd(bitsf(0x8026de01u), bitsf(0x8058e95bu))) == 0x807f8eb8u);   // IEEE: 0x807fc75c
    // ... and is exact otherwise.
    CHECK(fbits(py::lgcc_fadd(1.5f, 2.25f)) == fbits(3.75f));
    CHECK(fbits(py::lgcc_fadd(bitsf(0x00400000u), bitsf(0x00400000u))) == 0x00800000u);   // denormal doubling
    CHECK(fbits(py::lgcc_fadd(5.0f, -5.0f)) == 0u);
}

TEST(vecmath_quaternion_products) {
    using py::CQuaternion;
    const CQuaternion i(0.0f, 1.0f, 0.0f, 0.0f), j(0.0f, 0.0f, 1.0f, 0.0f), k(0.0f, 0.0f, 0.0f, 1.0f);
    const CQuaternion ij = i * j, ji = j * i;
    CHECK(ij.w == 0.0f && ij.x == 0.0f && ij.y == 0.0f && ij.z == 1.0f);
    CHECK(ji.w == 0.0f && ji.x == 0.0f && ji.y == 0.0f && ji.z == -1.0f);

    // 180 degrees about z: (1,2,3) -> (-1,-2,3); uv = (-2,1,0), uuv = (-1,-2,0).
    CHECK(eq3(k * py::CVector3(1.0f, 2.0f, 3.0f), -1.0f, -2.0f, 3.0f));

    py::CMatrix3 r;
    k.ToRotationMatrix(r);
    CHECK(r == py::CMatrix3(-1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f));
    // Back again through the trace <= 0 branch: i = 2, root = sqrt(4) = 2.
    CQuaternion q;
    q.FromRotationMatrix(r);
    CHECK(q.w == 0.0f && q.x == 0.0f && q.y == 0.0f && q.z == 1.0f);

    // q and -q are the same rotation for equals() (ACos(-1) = PI).
    ensureTables();
    CHECK(k.equals(k, 1e-3f));
    CHECK(k.equals(-k, 1e-3f));
    CHECK(!k.equals(i, 1e-3f));
}

TEST(vecmath_quaternion_normalise_and_nlerp) {
    using py::CQuaternion;
    // nlerp(0.5, 1, i) = (0.5, 0.5, 0, 0); Norm = 0.5, factor = 1/sqrtf(0.5)
    // = 1/0x3f3504f3 = 0x3fb504f3, and 0.5f * 0x3fb504f3 = 0x3f3504f3.
    const CQuaternion a = CQuaternion::nlerp(0.5f, CQuaternion(1.0f, 0.0f, 0.0f, 0.0f), CQuaternion(0.0f, 1.0f, 0.0f, 0.0f));
    CHECK(fbits(a.w) == 0x3f3504f3u && fbits(a.x) == 0x3f3504f3u && a.y == 0.0f && a.z == 0.0f);
    const CQuaternion b = CQuaternion::nlerp(1.0f, CQuaternion(1.0f, 0.0f, 0.0f, 0.0f), CQuaternion(0.0f, 1.0f, 0.0f, 0.0f));
    CHECK(b.w == 0.0f && b.x == 1.0f);

    // Quirk kept: normalise() of a zero quaternion divides 1/sqrt(0) = inf and
    // 0*inf gives the original's NaN 0x7fc00000 in every component.
    CQuaternion z(0.0f, -0.0f, 0.0f, 0.0f);
    CHECK(z.normalise() == 0.0f);
    CHECK(fbits(z.w) == 0x7fc00000u && fbits(z.x) == 0x7fc00000u && fbits(z.y) == 0x7fc00000u &&
          fbits(z.z) == 0x7fc00000u);

    // Inverse of a zero quaternion is ZERO (no division).
    const CQuaternion inv = CQuaternion(0.0f, 0.0f, 0.0f, 0.0f).Inverse();
    CHECK(inv.w == 0.0f && inv.x == 0.0f && inv.y == 0.0f && inv.z == 0.0f);
}

TEST(vecmath_matrix3_inverse_determinant) {
    using py::CMatrix3;
    // det = 1*(1*0-4*6) - 2*(0*0-4*5) + 3*(0*6-1*5) = -24 + 40 - 15 = 1
    const CMatrix3 m(1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 4.0f, 5.0f, 6.0f, 0.0f);
    CHECK(m.Determinant() == 1.0f);
    CMatrix3 inv;
    CHECK(m.Inverse(inv, 1e-6f));
    CHECK(inv == CMatrix3(-24.0f, 18.0f, 5.0f, 20.0f, -15.0f, -4.0f, -5.0f, 4.0f, 1.0f));
    CHECK(m * inv == CMatrix3::IDENTITY);

    // Singular (row 2 = row 0 + row 1): Inverse fails. Quirk kept (OGRE and
    // the original): the cofactors are written before the determinant test,
    // so the by-value Inverse() returns the adjugate, not ZERO.
    const CMatrix3 s(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 5.0f, 7.0f, 9.0f);
    CHECK(s.Determinant() == 0.0f);
    CHECK(!s.Inverse(inv, 1e-6f));
    const CMatrix3 adj(3.0f, 3.0f, -3.0f, -6.0f, -6.0f, 6.0f, 3.0f, 3.0f, -3.0f);
    CHECK(inv == adj);
    CHECK(s.Inverse(1e-6f) == adj);
}

TEST(vecmath_matrix3_qdu_svd) {
    using py::CMatrix3;
    using py::CVector3;
    // Upper triangular M = Q*D*U with Q = I, D = (2,3,5), U = (4/2, 6/2, 9/3);
    // InvSqrt(9) = 0x3eaaaaab and 3*0x3eaaaaab rounds to 1.0f, same for 0.2f*5.
    const CMatrix3 m(2.0f, 4.0f, 6.0f, 0.0f, 3.0f, 9.0f, 0.0f, 0.0f, 5.0f);
    CMatrix3 q;
    CVector3 d, u;
    m.QDUDecomposition(q, d, u);
    CHECK(q == CMatrix3::IDENTITY);
    CHECK(eq3(d, 2.0f, 3.0f, 5.0f));
    CHECK(eq3(u, 2.0f, 3.0f, 3.0f));

    // SVD of a diagonal matrix: singular values are |diagonal|, and the
    // composition L*S*R gives the matrix back exactly.
    const CMatrix3 diag(3.0f, 0.0f, 0.0f, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    CMatrix3 l, r;
    CVector3 sv;
    diag.SingularValueDecomposition(l, sv, r);
    CHECK(eq3(sv, 3.0f, 2.0f, 1.0f));
    CMatrix3 back;
    back.SingularValueComposition(l, sv, r);
    CHECK(back == diag);

    // Orthonormalize leaves a rotation (here a permutation) unchanged.
    CMatrix3 p(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    p.Orthonormalize();
    CHECK(p == CMatrix3(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f));
}

TEST(vecmath_matrix3_euler_quirks) {
    using py::CMatrix3;
    ensureTables();
    float y = 9.0f, p = 9.0f, r = 9.0f;
    // Identity: atan2(-m12, m22) = atan2(-0, 1) = -0, so yaw and roll are -0.
    CHECK(CMatrix3::IDENTITY.ToEulerAnglesXYZ(y, p, r));
    CHECK(fbits(p) == 0u && fbits(y) == 0x80000000u && fbits(r) == 0x80000000u);

    // Gimbal lock, pitch = +90: ASin(1) = HALF_PI (clamped), no unique solution.
    const CMatrix3 up(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
    CHECK(!up.ToEulerAnglesXYZ(y, p, r));
    CHECK(p == py::CMath::HALF_PI && r == 0.0f && y == 0.0f);

    // Pitch = -90: yaw = rfRAngle - fRmY is computed as 0.0f - atan2(+0, 1) =
    // +0, where a plain negation would give -0.
    const CMatrix3 down(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    CHECK(!down.ToEulerAnglesXYZ(y, p, r));
    CHECK(p == -py::CMath::HALF_PI && fbits(y) == 0u && r == 0.0f);

    // Angle 0 about any axis is the identity (Cos from the table is exactly 1).
    CMatrix3 m;
    m.FromAxisAngle(py::CVector3(0.0f, 0.0f, 1.0f), 0.0f);
    CHECK(m == CMatrix3::IDENTITY);
    py::CVector3 axis;
    float angle = 9.0f;
    CMatrix3::IDENTITY.ToAxisAngle(axis, angle);
    CHECK(angle == 0.0f && eq3(axis, 1.0f, 0.0f, 0.0f));
}

TEST(vecmath_matrix4_inverse_transform) {
    using py::CMatrix4;
    const CMatrix4 m(2.0f, 0.0f, 0.0f, 4.0f, 0.0f, 4.0f, 0.0f, 8.0f, 0.0f, 0.0f, 8.0f, 16.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    const CMatrix4 want(0.5f, 0.0f, 0.0f, -2.0f, 0.0f, 0.25f, 0.0f, -2.0f, 0.0f, 0.0f, 0.125f, -2.0f, 0.0f, 0.0f, 0.0f,
                        1.0f);
    CHECK(m.determinant() == 64.0f);
    CHECK(m.inverse() == want);
    CHECK(m.inverseAffine() == want);
    CHECK(m.concatenate(want) == CMatrix4::IDENTITY);
    CHECK(m.adjoint() == CMatrix4(32.0f, 0.0f, 0.0f, -128.0f, 0.0f, 16.0f, 0.0f, -128.0f, 0.0f, 0.0f, 8.0f, -128.0f,
                                  0.0f, 0.0f, 0.0f, 64.0f));
    CHECK(eq3(m * py::CVector3(1.0f, 1.0f, 1.0f), 6.0f, 12.0f, 24.0f));

    CMatrix4 t;
    t.makeTransform(py::CVector3(4.0f, 8.0f, 16.0f), py::CVector3(2.0f, 4.0f, 8.0f), py::CQuaternion::IDENTITY);
    CHECK(t == m);
    t.makeInverseTransform(py::CVector3(4.0f, 8.0f, 16.0f), py::CVector3(2.0f, 4.0f, 8.0f), py::CQuaternion::IDENTITY);
    CHECK(t == want);

    py::CVector3 pos, scale;
    py::CQuaternion rot;
    m.decomposition(pos, scale, rot);
    CHECK(eq3(pos, 4.0f, 8.0f, 16.0f) && eq3(scale, 2.0f, 4.0f, 8.0f));
    CHECK(rot.w == 1.0f && rot.x == 0.0f && rot.y == 0.0f && rot.z == 0.0f);
}

TEST(vecmath_plane) {
    using py::CPlane;
    using py::CVector3;
    // Through (0,0,0), (1,0,0), (0,1,0): normal +z and d = -(0) = -0.
    const CPlane p(CVector3(0.0f, 0.0f, 0.0f), CVector3(1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f));
    CHECK(eq3(p.normal, 0.0f, 0.0f, 1.0f) && fbits(p.d) == 0x80000000u);
    CHECK(p.getDistance(CVector3(7.0f, 7.0f, 5.0f)) == 5.0f);
    CHECK(p.getSide(CVector3(0.0f, 0.0f, 5.0f)) == CPlane::POSITIVE_SIDE);
    CHECK(p.getSide(CVector3(0.0f, 0.0f, -1.0f)) == CPlane::NEGATIVE_SIDE);
    CHECK(p.getSide(CVector3(3.0f, 3.0f, 0.0f)) == CPlane::NO_SIDE);

    py::CAxisAlignedBox box(CVector3(-1.0f, -1.0f, 1.0f), CVector3(1.0f, 1.0f, 2.0f));
    CHECK(p.getSide(box) == CPlane::POSITIVE_SIDE);
    box.mMinimum.z = -1.0f;
    CHECK(p.getSide(box) == CPlane::BOTH_SIDE);
    box.mExtent = py::CAxisAlignedBox::EXTENT_NULL;
    CHECK(p.getSide(box) == CPlane::NO_SIDE);
    box.mExtent = py::CAxisAlignedBox::EXTENT_INFINITE;
    CHECK(p.getSide(box) == CPlane::BOTH_SIDE);

    CHECK(eq3(p.projectVector(CVector3(1.0f, 2.0f, 3.0f)), 1.0f, 2.0f, 0.0f));

    // normalise scales d with the normal; CPlane(n, c) stores d = -c.
    CPlane q(CVector3(0.0f, 3.0f, 4.0f), 10.0f);
    CHECK(q.d == -10.0f);
    CHECK(q.normalise() == 5.0f);
    CHECK(fbits(q.normal.y) == 0x3f19999au && fbits(q.normal.z) == 0x3f4ccccdu && q.d == -2.0f);
}
