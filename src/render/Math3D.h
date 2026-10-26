// Math3D.h - small vector/matrix helpers for rendering.
//
// Rendering is not part of the lockstep simulation, so these use plain
// float math. (Gameplay code must use the CMath/fdlibm path instead.)
// Matrices are column-major, as OpenGL expects.
#pragma once

#include <cmath>
#include <utility>

namespace py {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    float length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const {
        const float l = length();
        return l > 0 ? *this * (1.0f / l) : *this;
    }
};

// Quaternion (w, x, y, z) like OGRE / CQuaternion.
struct Quat {
    float w = 1, x = 0, y = 0, z = 0;
    Quat() = default;
    Quat(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}
    Quat inverse() const {
        const float n = w * w + x * x + y * y + z * z;
        if (n <= 0) return {};
        const float inv = 1.0f / n;
        return {w * inv, -x * inv, -y * inv, -z * inv};
    }
    Vec3 rotate(const Vec3& v) const {
        // v' = q v q*
        const Vec3 u(x, y, z);
        const Vec3 uv = u.cross(v), uuv = u.cross(uv);
        return v + (uv * (2.0f * w)) + (uuv * 2.0f);
    }
};

struct Mat4 {
    float m[16];   // column-major: m[col*4 + row]
    static Mat4 identity() {
        Mat4 r{};
        for (int i = 0; i < 16; ++i) r.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        return r;
    }
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += at(rr, k) * o.at(k, c);
                r.at(rr, c) = s;
            }
        return r;
    }
    // translate * rotate(q) * scale (OGRE makeTransform order)
    static Mat4 transform(const Vec3& t, const Quat& q, const Vec3& s) {
        const float w = q.w, x = q.x, y = q.y, z = q.z;
        const float r00 = 1 - 2 * (y * y + z * z), r01 = 2 * (x * y - z * w), r02 = 2 * (x * z + y * w);
        const float r10 = 2 * (x * y + z * w), r11 = 1 - 2 * (x * x + z * z), r12 = 2 * (y * z - x * w);
        const float r20 = 2 * (x * z - y * w), r21 = 2 * (y * z + x * w), r22 = 1 - 2 * (x * x + y * y);
        Mat4 r = identity();
        r.at(0, 0) = r00 * s.x; r.at(0, 1) = r01 * s.y; r.at(0, 2) = r02 * s.z; r.at(0, 3) = t.x;
        r.at(1, 0) = r10 * s.x; r.at(1, 1) = r11 * s.y; r.at(1, 2) = r12 * s.z; r.at(1, 3) = t.y;
        r.at(2, 0) = r20 * s.x; r.at(2, 1) = r21 * s.y; r.at(2, 2) = r22 * s.z; r.at(2, 3) = t.z;
        return r;
    }
    static Mat4 perspective(float fovyRad, float aspect, float zNear, float zFar) {
        Mat4 r{};
        const float f = 1.0f / std::tan(fovyRad * 0.5f);
        r.at(0, 0) = f / aspect;
        r.at(1, 1) = f;
        r.at(2, 2) = (zFar + zNear) / (zNear - zFar);
        r.at(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
        r.at(3, 2) = -1.0f;
        return r;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        const Vec3 f = (target - eye).normalized();
        const Vec3 s = f.cross(up).normalized();
        const Vec3 u = s.cross(f);
        Mat4 r = identity();
        r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z; r.at(0, 3) = -s.dot(eye);
        r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z; r.at(1, 3) = -u.dot(eye);
        r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = f.dot(eye);
        return r;
    }
    bool inverse(Mat4& out) const;
};

inline bool Mat4::inverse(Mat4& out) const {
    // Gauss-Jordan on a 4x8 augmented matrix.
    float a[4][8];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            a[r][c] = at(r, c);
            a[r][c + 4] = (r == c) ? 1.0f : 0.0f;
        }
    for (int c = 0; c < 4; ++c) {
        int piv = c;
        for (int r = c + 1; r < 4; ++r)
            if (std::fabs(a[r][c]) > std::fabs(a[piv][c])) piv = r;
        if (std::fabs(a[piv][c]) < 1e-12f) return false;
        if (piv != c)
            for (int k = 0; k < 8; ++k) std::swap(a[c][k], a[piv][k]);
        const float inv = 1.0f / a[c][c];
        for (int k = 0; k < 8; ++k) a[c][k] *= inv;
        for (int r = 0; r < 4; ++r)
            if (r != c) {
                const float f = a[r][c];
                for (int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
            }
    }
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out.at(r, c) = a[r][c + 4];
    return true;
}

}  // namespace py
