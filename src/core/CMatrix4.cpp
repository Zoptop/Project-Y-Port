// CMatrix4.cpp - port of ProjectY::CMatrix4, see CMatrix4.h.
//
// Translated from the Ghidra decompilation / disassembly of libProjectY.so,
// following OGRE's Matrix4 code. Arithmetic is py::Real (libgcc soft-float,
// NaN bits included) with operands in the original's order: the original
// compiler commuted some products and shared common subexpressions such as
// the 2x2 minors in adjoint/determinant.
#include "CMatrix4.h"

namespace py {

// Ghidra 0x1430a4. Row 0 multiplies m[0][k]*m2[k][c]; the original
// compiler commuted the products of rows 1-3 to m2[k][c]*m[r][k].
CMatrix4 CMatrix4::concatenate(const CMatrix4& m2) const {
    CMatrix4 r;
    for (unsigned iCol = 0; iCol < 4; iCol++)
        r.m[0][iCol] = m[0][0] * m2.m[0][iCol] + m[0][1] * m2.m[1][iCol] + m[0][2] * m2.m[2][iCol] +
                       m[0][3] * m2.m[3][iCol];
    for (unsigned iRow = 1; iRow < 4; iRow++)
        for (unsigned iCol = 0; iCol < 4; iCol++)
            r.m[iRow][iCol] = m2.m[0][iCol] * m[iRow][0] + m2.m[1][iCol] * m[iRow][1] +
                              m2.m[2][iCol] * m[iRow][2] + m2.m[3][iCol] * m[iRow][3];
    return r;
}

// Ghidra 0x1ac514
CVector3 CMatrix4::operator*(const CVector3& v) const {
    CVector3 r;
    const Real fInvW = 1.0f / (m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3]);
    r.x = (v.x * m[0][0] + v.y * m[0][1] + v.z * m[0][2] + m[0][3]) * fInvW;
    r.y = (v.x * m[1][0] + v.y * m[1][1] + v.z * m[1][2] + m[1][3]) * fInvW;
    r.z = (v.x * m[2][0] + v.y * m[2][1] + v.z * m[2][2] + m[2][3]) * fInvW;
    return r;
}

// Ghidra 0x14f530 (row 0 as m*v, rows 1-3 commuted to v*m)
CVector4 CMatrix4::operator*(const CVector4& v) const {
    return CVector4(m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
                    v.x * m[1][0] + v.y * m[1][1] + v.z * m[1][2] + v.w * m[1][3],
                    v.x * m[2][0] + v.y * m[2][1] + v.z * m[2][2] + v.w * m[2][3],
                    v.x * m[3][0] + v.y * m[3][1] + v.z * m[3][2] + v.w * m[3][3]);
}

// Ghidra 0x2f6b24. OGRE builds each entry with MINOR(); the original
// compiler shared the 2x2 sub-determinants between entries and commuted some
// products, reproduced here.
CMatrix4 CMatrix4::adjoint() const {
    const Real m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const Real m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const Real m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const Real m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    const Real A = m22 * m33 - m32 * m23;
    const Real B = m33 * m21 - m23 * m31;
    const Real C = m32 * m21 - m22 * m31;
    const Real D = m33 * m12 - m32 * m13;
    const Real E = m11 * m33 - m31 * m13;
    const Real F = m11 * m32 - m12 * m31;
    const Real G = m23 * m12 - m22 * m13;
    const Real H = m11 * m23 - m21 * m13;
    const Real I = m11 * m22 - m12 * m21;
    const Real J = m33 * m20 - m23 * m30;
    const Real K = m32 * m20 - m22 * m30;
    const Real L = m33 * m10 - m13 * m30;
    const Real M = m32 * m10 - m12 * m30;
    const Real N = m23 * m10 - m13 * m20;
    const Real O = m22 * m10 - m12 * m20;
    const Real P = m31 * m20 - m21 * m30;
    const Real Q = m31 * m10 - m11 * m30;
    const Real R = m21 * m10 - m11 * m20;

    return CMatrix4(m11 * A - m12 * B + m13 * C,       //  MINOR(1,2,3, 1,2,3)
                    -(A * m01 - B * m02 + C * m03),     // -MINOR(0,2,3, 1,2,3)
                    m01 * D - m02 * E + m03 * F,       //  MINOR(0,1,3, 1,2,3)
                    -(m01 * G - m02 * H + m03 * I),     // -MINOR(0,1,2, 1,2,3)

                    -(A * m10 - m12 * J + m13 * K),     // -MINOR(1,2,3, 0,2,3)
                    A * m00 - m02 * J + m03 * K,       //  MINOR(0,2,3, 0,2,3)
                    -(D * m00 - m02 * L + m03 * M),     // -MINOR(0,1,3, 0,2,3)
                    G * m00 - m02 * N + m03 * O,       //  MINOR(0,1,2, 0,2,3)

                    B * m10 - m11 * J + m13 * P,       //  MINOR(1,2,3, 0,1,3)
                    -(B * m00 - m01 * J + m03 * P),     // -MINOR(0,2,3, 0,1,3)
                    E * m00 - m01 * L + m03 * Q,       //  MINOR(0,1,3, 0,1,3)
                    -(H * m00 - m01 * N + m03 * R),     // -MINOR(0,1,2, 0,1,3)

                    -(C * m10 - m11 * K + m12 * P),     // -MINOR(1,2,3, 0,1,2)
                    C * m00 - m01 * K + m02 * P,       //  MINOR(0,2,3, 0,1,2)
                    -(F * m00 - m01 * M + m02 * Q),     // -MINOR(0,1,3, 0,1,2)
                    I * m00 - m01 * O + m02 * R);      //  MINOR(0,1,2, 0,1,2)
}

// Ghidra 0x2f7500: m00*MINOR(123,123) - m01*MINOR(123,023) + m02*MINOR(123,013)
// - m03*MINOR(123,012), with the shared sub-determinants of the original.
float CMatrix4::determinant() const {
    const Real m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const Real m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const Real m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const Real m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    const Real A = m22 * m33 - m32 * m23;
    const Real B = m33 * m21 - m23 * m31;
    const Real C = m32 * m21 - m22 * m31;
    const Real J = m33 * m20 - m23 * m30;
    const Real K = m32 * m20 - m22 * m30;
    const Real P = m31 * m20 - m21 * m30;

    return m00 * (m11 * A - m12 * B + m13 * C) - m01 * (A * m10 - m12 * J + m13 * K) +
           m02 * (B * m10 - m11 * J + m13 * P) - m03 * (C * m10 - m11 * K + m12 * P);
}

// Ghidra 0x2f7868 (operand order as in OGRE's source)
CMatrix4 CMatrix4::inverse() const {
    const Real m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const Real m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const Real m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const Real m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    Real v0 = m20 * m31 - m21 * m30;
    Real v1 = m20 * m32 - m22 * m30;
    Real v2 = m20 * m33 - m23 * m30;
    Real v3 = m21 * m32 - m22 * m31;
    Real v4 = m21 * m33 - m23 * m31;
    Real v5 = m22 * m33 - m23 * m32;

    const Real t00 = +(v5 * m11 - v4 * m12 + v3 * m13);
    const Real t10 = -(v5 * m10 - v2 * m12 + v1 * m13);
    const Real t20 = +(v4 * m10 - v2 * m11 + v0 * m13);
    const Real t30 = -(v3 * m10 - v1 * m11 + v0 * m12);

    const Real invDet = 1.0f / (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);

    const Real d00 = t00 * invDet;
    const Real d10 = t10 * invDet;
    const Real d20 = t20 * invDet;
    const Real d30 = t30 * invDet;

    const Real d01 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    const Real d11 = +(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    const Real d21 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    const Real d31 = +(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

    v0 = m10 * m31 - m11 * m30;
    v1 = m10 * m32 - m12 * m30;
    v2 = m10 * m33 - m13 * m30;
    v3 = m11 * m32 - m12 * m31;
    v4 = m11 * m33 - m13 * m31;
    v5 = m12 * m33 - m13 * m32;

    const Real d02 = +(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    const Real d12 = -(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    const Real d22 = +(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    const Real d32 = -(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

    v0 = m21 * m10 - m20 * m11;
    v1 = m22 * m10 - m20 * m12;
    v2 = m23 * m10 - m20 * m13;
    v3 = m22 * m11 - m21 * m12;
    v4 = m23 * m11 - m21 * m13;
    v5 = m23 * m12 - m22 * m13;

    const Real d03 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    const Real d13 = +(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    const Real d23 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    const Real d33 = +(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

    return CMatrix4(d00, d01, d02, d03, d10, d11, d12, d13, d20, d21, d22, d23, d30, d31, d32, d33);
}

// Ghidra 0x2f83c4 (operand order as in OGRE's source)
CMatrix4 CMatrix4::inverseAffine() const {
    // assert(isAffine());
    const Real m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
    const Real m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];

    Real t00 = m22 * m11 - m21 * m12;
    Real t10 = m20 * m12 - m22 * m10;
    Real t20 = m21 * m10 - m20 * m11;

    Real m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];

    const Real invDet = 1.0f / (m00 * t00 + m01 * t10 + m02 * t20);

    t00 *= invDet;
    t10 *= invDet;
    t20 *= invDet;

    m00 *= invDet;
    m01 *= invDet;
    m02 *= invDet;

    const Real r00 = t00;
    const Real r01 = m02 * m21 - m01 * m22;
    const Real r02 = m01 * m12 - m02 * m11;

    const Real r10 = t10;
    const Real r11 = m00 * m22 - m02 * m20;
    const Real r12 = m02 * m10 - m00 * m12;

    const Real r20 = t20;
    const Real r21 = m01 * m20 - m00 * m21;
    const Real r22 = m00 * m11 - m01 * m10;

    const Real m03 = m[0][3], m13 = m[1][3], m23 = m[2][3];

    const Real r03 = -(r00 * m03 + r01 * m13 + r02 * m23);
    const Real r13 = -(r10 * m03 + r11 * m13 + r12 * m23);
    const Real r23 = -(r20 * m03 + r21 * m13 + r22 * m23);

    return CMatrix4(r00, r01, r02, r03, r10, r11, r12, r13, r20, r21, r22, r23, 0.0f, 0.0f, 0.0f, 1.0f);
}

// Ghidra 0x2f883c
void CMatrix4::makeTransform(const CVector3& position, const CVector3& scale, const CQuaternion& orientation) {
    // Ordering:
    //    1. Scale
    //    2. Rotate
    //    3. Translate
    CMatrix3 rot3x3;
    orientation.ToRotationMatrix(rot3x3);

    // Set up final matrix with scale, rotation and translation
    m[0][0] = scale.x * rot3x3.m[0][0];
    m[0][1] = scale.y * rot3x3.m[0][1];
    m[0][2] = scale.z * rot3x3.m[0][2];
    m[0][3] = position.x;
    m[1][0] = scale.x * rot3x3.m[1][0];
    m[1][1] = scale.y * rot3x3.m[1][1];
    m[1][2] = scale.z * rot3x3.m[1][2];
    m[1][3] = position.y;
    m[2][0] = scale.x * rot3x3.m[2][0];
    m[2][1] = scale.y * rot3x3.m[2][1];
    m[2][2] = scale.z * rot3x3.m[2][2];
    m[2][3] = position.z;

    // No projection term
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

// Ghidra 0x2f8a14
void CMatrix4::makeTransformNoScale(const CVector3& position, const CQuaternion& orientation) {
    CMatrix3 rot3x3;
    orientation.ToRotationMatrix(rot3x3);

    m[0][0] = rot3x3.m[0][0];
    m[0][1] = rot3x3.m[0][1];
    m[0][2] = rot3x3.m[0][2];
    m[0][3] = position.x;
    m[1][0] = rot3x3.m[1][0];
    m[1][1] = rot3x3.m[1][1];
    m[1][2] = rot3x3.m[1][2];
    m[1][3] = position.y;
    m[2][0] = rot3x3.m[2][0];
    m[2][1] = rot3x3.m[2][1];
    m[2][2] = rot3x3.m[2][2];
    m[2][3] = position.z;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

// Ghidra 0x2f8b10
void CMatrix4::makeInverseTransform(const CVector3& position, const CVector3& scale, const CQuaternion& orientation) {
    // Invert the parameters
    CVector3 invTranslate = -position;
    const CVector3 invScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
    const CQuaternion invRot = orientation.Inverse();

    // Because we're inverting, order is translation, rotation, scale
    // So make translation relative to scale & rotation
    invTranslate = invRot * invTranslate;   // rotate
    invTranslate.x = invScale.x * invTranslate.x;   // invTranslate *= invScale (commuted)
    invTranslate.y = invScale.y * invTranslate.y;
    invTranslate.z = invScale.z * invTranslate.z;

    // Next, make a 3x3 rotation matrix
    CMatrix3 rot3x3;
    invRot.ToRotationMatrix(rot3x3);

    // Set up final matrix with scale, rotation and translation
    m[0][0] = invScale.x * rot3x3.m[0][0];
    m[0][1] = invScale.x * rot3x3.m[0][1];
    m[0][2] = invScale.x * rot3x3.m[0][2];
    m[0][3] = invTranslate.x;
    m[1][0] = invScale.y * rot3x3.m[1][0];
    m[1][1] = invScale.y * rot3x3.m[1][1];
    m[1][2] = invScale.y * rot3x3.m[1][2];
    m[1][3] = invTranslate.y;
    m[2][0] = invScale.z * rot3x3.m[2][0];
    m[2][1] = invScale.z * rot3x3.m[2][1];
    m[2][2] = invScale.z * rot3x3.m[2][2];
    m[2][3] = invTranslate.z;

    // No projection term
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

// Ghidra 0x2f8dc0
void CMatrix4::decomposition(CVector3& position, CVector3& scale, CQuaternion& orientation) const {
    // assert(isAffine());  (not in the original build)
    CMatrix3 m3x3;
    extract3x3Matrix(m3x3);

    CMatrix3 matQ;
    CVector3 vecU;
    m3x3.QDUDecomposition(matQ, scale, vecU);

    orientation = CQuaternion(matQ);
    position = CVector3(m[0][3], m[1][3], m[2][3]);
}

}  // namespace py
