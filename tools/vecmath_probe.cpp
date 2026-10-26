// vecmath_probe.cpp - runs the port's CVector2/3, CQuaternion, CMatrix3,
// CMatrix4 and CPlane functions on inputs from stdin, for
// tools/emu/check_vecmath.py (which runs the same inputs through the original
// libProjectY.so under Unicorn and compares the output bits).
//
// Usage: vecmath_probe <function>   (or "list" to print all function names)
// stdin: one test per line, input words as 8-digit hex (float bit patterns,
// ints, bools). stdout: one line per test, output words as hex.
// Output objects start filled with SENTINEL so that untouched fields compare
// equal to the emulator side, which pre-fills its buffers the same way.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/CMath.h"
#include "core/CMatrix3.h"
#include "core/CMatrix4.h"
#include "core/CPlane.h"
#include "core/CQuaternion.h"
#include "core/CUtils.h"
#include "core/CVector2.h"
#include "core/CVector3.h"
#include "core/CVector4.h"
#include "core/FloatBits.h"

using namespace py;

namespace {

constexpr uint32_t SENTINEL = 0x7fc0beefu;

struct In {
    const std::vector<uint32_t>& w;
    size_t i = 0;
    uint32_t u() { return w.at(i++); }
    float f() { return bitsf(u()); }
    CVector2 v2() { const float a = f(); return CVector2(a, f()); }
    CVector3 v3() { const float a = f(), b = f(); return CVector3(a, b, f()); }
    CVector4 v4() { const float a = f(), b = f(), c = f(); return CVector4(a, b, c, f()); }
    CQuaternion q() { const float a = f(), b = f(), c = f(); return CQuaternion(a, b, c, f()); }
    CMatrix3 m3() { CMatrix3 r; for (auto& row : r.m) for (auto& e : row) e = f(); return r; }
    CMatrix4 m4() { CMatrix4 r; for (auto& row : r.m) for (auto& e : row) e = f(); return r; }
    CPlane plane() { const CVector3 n = v3(); return CPlane(n.x, n.y, n.z, f()); }
};

struct Out {
    std::vector<uint32_t> w;
    void u(uint32_t x) { w.push_back(x); }
    void f(float x) { u(fbits(x)); }
    void v3(const CVector3& v) { f(v.x); f(v.y); f(v.z); }
    void v4(const CVector4& v) { f(v.x); f(v.y); f(v.z); f(v.w); }
    void q(const CQuaternion& v) { f(v.w); f(v.x); f(v.y); f(v.z); }
    void m3(const CMatrix3& m) { for (const auto& row : m.m) for (float e : row) f(e); }
    void m4(const CMatrix4& m) { for (const auto& row : m.m) for (float e : row) f(e); }
    void plane(const CPlane& p) { v3(p.normal); f(p.d); }
};

float S() { return bitsf(SENTINEL); }
CVector3 sv3() { return CVector3(S(), S(), S()); }
CQuaternion sq() { return CQuaternion(S(), S(), S(), S()); }
CMatrix3 sm3() { CMatrix3 r; for (auto& row : r.m) for (auto& e : row) e = S(); return r; }
CMatrix4 sm4() { CMatrix4 r; for (auto& row : r.m) for (auto& e : row) e = S(); return r; }

using Fn = std::function<void(In&, Out&)>;

std::map<std::string, Fn> registry() {
    std::map<std::string, Fn> t;
    // ---- libgcc __aeabi_fadd as reproduced by py::lgcc_fadd (subnormal quirk)
    t["libgcc.fadd"] = [](In& i, Out& o) { const float a = i.f(); o.f(lgcc_fadd(a, i.f())); };

    // ---- CVector2 / CVector3
    t["v2.distance"] = [](In& i, Out& o) { const CVector2 a = i.v2(), b = i.v2(); o.f(a.distance(b)); };
    t["v3.distance"] = [](In& i, Out& o) { const CVector3 a = i.v3(), b = i.v3(); o.f(a.distance(b)); };
    t["v3.normalise"] = [](In& i, Out& o) { CVector3 a = i.v3(); const float l = a.normalise(); o.f(l); o.v3(a); };

    // ---- CQuaternion
    t["q.FromRotationMatrix"] = [](In& i, Out& o) { CQuaternion q = sq(); q.FromRotationMatrix(i.m3()); o.q(q); };
    t["q.ToRotationMatrix"] = [](In& i, Out& o) { CMatrix3 m = sm3(); i.q().ToRotationMatrix(m); o.m3(m); };
    t["q.FromAngleAxis"] = [](In& i, Out& o) {
        const float a = i.f(); const CVector3 ax = i.v3(); CQuaternion q = sq(); q.FromAngleAxis(a, ax); o.q(q);
    };
    t["q.ToAngleAxis"] = [](In& i, Out& o) {
        float a = S(); CVector3 ax = sv3(); i.q().ToAngleAxis(a, ax); o.f(a); o.v3(ax);
    };
    t["q.FromAxesPtr"] = [](In& i, Out& o) {
        CVector3 ax[3]; ax[0] = i.v3(); ax[1] = i.v3(); ax[2] = i.v3();
        CQuaternion q = sq(); q.FromAxes(ax); o.q(q);
    };
    t["q.FromAxes3"] = [](In& i, Out& o) {
        const CVector3 a = i.v3(), b = i.v3(), c = i.v3(); CQuaternion q = sq(); q.FromAxes(a, b, c); o.q(q);
    };
    t["q.ToAxesPtr"] = [](In& i, Out& o) {
        CVector3 ax[3] = {sv3(), sv3(), sv3()}; i.q().ToAxes(ax); o.v3(ax[0]); o.v3(ax[1]); o.v3(ax[2]);
    };
    t["q.ToAxes3"] = [](In& i, Out& o) {
        CVector3 a = sv3(), b = sv3(), c = sv3(); i.q().ToAxes(a, b, c); o.v3(a); o.v3(b); o.v3(c);
    };
    t["q.xAxis"] = [](In& i, Out& o) { o.v3(i.q().xAxis()); };
    t["q.yAxis"] = [](In& i, Out& o) { o.v3(i.q().yAxis()); };
    t["q.zAxis"] = [](In& i, Out& o) { o.v3(i.q().zAxis()); };
    t["q.add"] = [](In& i, Out& o) { const CQuaternion a = i.q(), b = i.q(); o.q(a + b); };
    t["q.sub"] = [](In& i, Out& o) { const CQuaternion a = i.q(), b = i.q(); o.q(a - b); };
    t["q.mulq"] = [](In& i, Out& o) { const CQuaternion a = i.q(), b = i.q(); o.q(a * b); };
    t["q.mulf"] = [](In& i, Out& o) { const CQuaternion a = i.q(); const float s = i.f(); o.q(a * s); };
    t["q.fmul"] = [](In& i, Out& o) { const float s = i.f(); const CQuaternion a = i.q(); o.q(s * a); };
    t["q.neg"] = [](In& i, Out& o) { o.q(-i.q()); };
    t["q.Dot"] = [](In& i, Out& o) { const CQuaternion a = i.q(), b = i.q(); o.f(a.Dot(b)); };
    t["q.Norm"] = [](In& i, Out& o) { o.f(i.q().Norm()); };
    t["q.Inverse"] = [](In& i, Out& o) { o.q(i.q().Inverse()); };
    t["q.UnitInverse"] = [](In& i, Out& o) { o.q(i.q().UnitInverse()); };
    t["q.Exp"] = [](In& i, Out& o) { o.q(i.q().Exp()); };
    t["q.Log"] = [](In& i, Out& o) { o.q(i.q().Log()); };
    t["q.mulv"] = [](In& i, Out& o) { const CQuaternion a = i.q(); const CVector3 v = i.v3(); o.v3(a * v); };
    t["q.equals"] = [](In& i, Out& o) {
        const CQuaternion a = i.q(), b = i.q(); const float tol = i.f(); o.u(a.equals(b, tol) ? 1u : 0u);
    };
    t["q.SlerpExtraSpins"] = [](In& i, Out& o) {
        const float tt = i.f(); const CQuaternion p = i.q(), q = i.q(); const int spins = static_cast<int32_t>(i.u());
        o.q(CQuaternion::SlerpExtraSpins(tt, p, q, spins));
    };
    t["q.Intermediate"] = [](In& i, Out& o) {
        const CQuaternion q0 = i.q(), q1 = i.q(), q2 = i.q(); CQuaternion a = sq(), b = sq();
        CQuaternion::Intermediate(q0, q1, q2, a, b); o.q(a); o.q(b);
    };
    t["q.normalise"] = [](In& i, Out& o) { CQuaternion a = i.q(); const float l = a.normalise(); o.f(l); o.q(a); };
    t["q.Slerp"] = [](In& i, Out& o) {
        const float tt = i.f(); const CQuaternion p = i.q(), q = i.q(); const bool sp = i.u() != 0;
        o.q(CQuaternion::Slerp(tt, p, q, sp));
    };
    t["q.Squad"] = [](In& i, Out& o) {
        const float tt = i.f(); const CQuaternion p = i.q(), a = i.q(), b = i.q(), q = i.q(); const bool sp = i.u() != 0;
        o.q(CQuaternion::Squad(tt, p, a, b, q, sp));
    };
    t["q.getRoll"] = [](In& i, Out& o) { const CQuaternion a = i.q(); o.f(a.getRoll(i.u() != 0)); };
    t["q.getPitch"] = [](In& i, Out& o) { const CQuaternion a = i.q(); o.f(a.getPitch(i.u() != 0)); };
    t["q.getYaw"] = [](In& i, Out& o) { const CQuaternion a = i.q(); o.f(a.getYaw(i.u() != 0)); };
    t["q.nlerp"] = [](In& i, Out& o) {
        const float tt = i.f(); const CQuaternion p = i.q(), q = i.q(); const bool sp = i.u() != 0;
        o.q(CQuaternion::nlerp(tt, p, q, sp));
    };

    // ---- CMatrix3
    t["m3.GetColumn"] = [](In& i, Out& o) { const CMatrix3 m = i.m3(); o.v3(m.GetColumn(i.u())); };
    t["m3.SetColumn"] = [](In& i, Out& o) {
        CMatrix3 m = i.m3(); const unsigned c = i.u(); m.SetColumn(c, i.v3()); o.m3(m);
    };
    t["m3.FromAxes"] = [](In& i, Out& o) {
        CMatrix3 m = sm3(); const CVector3 a = i.v3(), b = i.v3(), c = i.v3(); m.FromAxes(a, b, c); o.m3(m);
    };
    t["m3.eq"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(), b = i.m3(); o.u(a == b ? 1u : 0u); };
    t["m3.add"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(), b = i.m3(); o.m3(a + b); };
    t["m3.sub"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(), b = i.m3(); o.m3(a - b); };
    t["m3.mulm"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(), b = i.m3(); o.m3(a * b); };
    t["m3.mulv"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(); o.v3(a * i.v3()); };
    t["m3.vmul"] = [](In& i, Out& o) { const CVector3 v = i.v3(); o.v3(v * i.m3()); };
    t["m3.neg"] = [](In& i, Out& o) { o.m3(-i.m3()); };
    t["m3.mulf"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(); o.m3(a * i.f()); };
    t["m3.fmul"] = [](In& i, Out& o) { const float s = i.f(); o.m3(s * i.m3()); };
    t["m3.Transpose"] = [](In& i, Out& o) { o.m3(i.m3().Transpose()); };
    t["m3.InverseRef"] = [](In& i, Out& o) {
        const CMatrix3 a = i.m3(); const float tol = i.f(); CMatrix3 r = sm3();
        const bool ok = a.Inverse(r, tol); o.u(ok ? 1u : 0u); o.m3(r);
    };
    t["m3.InverseVal"] = [](In& i, Out& o) { const CMatrix3 a = i.m3(); o.m3(a.Inverse(i.f())); };
    t["m3.Determinant"] = [](In& i, Out& o) { o.f(i.m3().Determinant()); };
    t["m3.Bidiagonalize"] = [](In& i, Out& o) {
        CMatrix3 a = i.m3(), l = sm3(), r = sm3(); CMatrix3::Bidiagonalize(a, l, r); o.m3(a); o.m3(l); o.m3(r);
    };
    t["m3.GolubKahanStep"] = [](In& i, Out& o) {
        CMatrix3 a = i.m3(), l = i.m3(), r = i.m3(); CMatrix3::GolubKahanStep(a, l, r); o.m3(a); o.m3(l); o.m3(r);
    };
    t["m3.SVD"] = [](In& i, Out& o) {
        const CMatrix3 a = i.m3(); CMatrix3 l = sm3(), r = sm3(); CVector3 s = sv3();
        a.SingularValueDecomposition(l, s, r); o.m3(l); o.v3(s); o.m3(r);
    };
    t["m3.SVComposition"] = [](In& i, Out& o) {
        const CMatrix3 l = i.m3(); const CVector3 s = i.v3(); const CMatrix3 r = i.m3(); CMatrix3 m = sm3();
        m.SingularValueComposition(l, s, r); o.m3(m);
    };
    t["m3.Orthonormalize"] = [](In& i, Out& o) { CMatrix3 a = i.m3(); a.Orthonormalize(); o.m3(a); };
    t["m3.QDU"] = [](In& i, Out& o) {
        const CMatrix3 a = i.m3(); CMatrix3 q = sm3(); CVector3 d = sv3(), u = sv3();
        a.QDUDecomposition(q, d, u); o.m3(q); o.v3(d); o.v3(u);
    };
    t["m3.MaxCubicRoot"] = [](In& i, Out& o) { float c[3] = {i.f(), i.f(), i.f()}; o.f(CMatrix3::MaxCubicRoot(c)); };
    t["m3.SpectralNorm"] = [](In& i, Out& o) { o.f(i.m3().SpectralNorm()); };
    t["m3.ToAxisAngle"] = [](In& i, Out& o) {
        CVector3 ax = sv3(); float a = S(); i.m3().ToAxisAngle(ax, a); o.v3(ax); o.f(a);
    };
    t["m3.FromAxisAngle"] = [](In& i, Out& o) {
        const CVector3 ax = i.v3(); const float a = i.f(); CMatrix3 m = sm3(); m.FromAxisAngle(ax, a); o.m3(m);
    };
#define TOEULER(N)                                                                         \
    t["m3.ToEulerAngles" #N] = [](In& i, Out& o) {                                         \
        float y = S(), p = S(), r = S();                                                   \
        const bool ok = i.m3().ToEulerAngles##N(y, p, r);                                  \
        o.u(ok ? 1u : 0u); o.f(y); o.f(p); o.f(r);                                         \
    };
#define FROMEULER(N)                                                                       \
    t["m3.FromEulerAngles" #N] = [](In& i, Out& o) {                                       \
        const float y = i.f(), p = i.f(), r = i.f(); CMatrix3 m = sm3();                   \
        m.FromEulerAngles##N(y, p, r); o.m3(m);                                            \
    };
    TOEULER(XYZ) TOEULER(XZY) TOEULER(YXZ) TOEULER(YZX) TOEULER(ZXY) TOEULER(ZYX)
    FROMEULER(XYZ) FROMEULER(XZY) FROMEULER(YXZ) FROMEULER(YZX) FROMEULER(ZXY) FROMEULER(ZYX)
#undef TOEULER
#undef FROMEULER
    t["m3.FromEulerAnglesZY"] = [](In& i, Out& o) {
        const float a = i.f(), b = i.f(); CMatrix3 m = sm3(); m.FromEulerAnglesZY(a, b); o.m3(m);
    };
    t["m3.Tridiagonal"] = [](In& i, Out& o) {
        CMatrix3 m = i.m3(); float d[3] = {S(), S(), S()}, s[3] = {S(), S(), S()};
        m.Tridiagonal(d, s); o.m3(m); for (float x : d) o.f(x); for (float x : s) o.f(x);
    };
    t["m3.QLAlgorithm"] = [](In& i, Out& o) {
        CMatrix3 m = i.m3(); float d[3] = {i.f(), i.f(), i.f()}; float s[3] = {i.f(), i.f(), i.f()};
        const bool ok = m.QLAlgorithm(d, s); o.u(ok ? 1u : 0u); o.m3(m); for (float x : d) o.f(x); for (float x : s) o.f(x);
    };
    t["m3.EigenSolveSymmetric"] = [](In& i, Out& o) {
        const CMatrix3 m = i.m3(); float ev[3] = {S(), S(), S()}; CVector3 vec[3] = {sv3(), sv3(), sv3()};
        m.EigenSolveSymmetric(ev, vec); for (float x : ev) o.f(x); for (const auto& v : vec) o.v3(v);
    };
    t["m3.TensorProduct"] = [](In& i, Out& o) {
        const CVector3 u = i.v3(), v = i.v3(); CMatrix3 m = sm3(); CMatrix3::TensorProduct(u, v, m); o.m3(m);
    };

    // ---- CMatrix4
    t["m4.concatenate"] = [](In& i, Out& o) { const CMatrix4 a = i.m4(), b = i.m4(); o.m4(a.concatenate(b)); };
    t["m4.mulv4"] = [](In& i, Out& o) { const CMatrix4 a = i.m4(); o.v4(a * i.v4()); };
    t["m4.mulv3"] = [](In& i, Out& o) { const CMatrix4 a = i.m4(); o.v3(a * i.v3()); };
    t["m4.adjoint"] = [](In& i, Out& o) { o.m4(i.m4().adjoint()); };
    t["m4.determinant"] = [](In& i, Out& o) { o.f(i.m4().determinant()); };
    t["m4.inverse"] = [](In& i, Out& o) { o.m4(i.m4().inverse()); };
    t["m4.inverseAffine"] = [](In& i, Out& o) { o.m4(i.m4().inverseAffine()); };
    t["m4.makeTransform"] = [](In& i, Out& o) {
        const CVector3 p = i.v3(), s = i.v3(); const CQuaternion q = i.q(); CMatrix4 m = sm4();
        m.makeTransform(p, s, q); o.m4(m);
    };
    t["m4.makeTransformNoScale"] = [](In& i, Out& o) {
        const CVector3 p = i.v3(); const CQuaternion q = i.q(); CMatrix4 m = sm4(); m.makeTransformNoScale(p, q); o.m4(m);
    };
    t["m4.makeInverseTransform"] = [](In& i, Out& o) {
        const CVector3 p = i.v3(), s = i.v3(); const CQuaternion q = i.q(); CMatrix4 m = sm4();
        m.makeInverseTransform(p, s, q); o.m4(m);
    };
    t["m4.decomposition"] = [](In& i, Out& o) {
        const CMatrix4 m = i.m4(); CVector3 p = sv3(), s = sv3(); CQuaternion q = sq();
        m.decomposition(p, s, q); o.v3(p); o.v3(s); o.q(q);
    };

    // ---- CPlane
    t["p.ctor0"] = [](In&, Out& o) { o.plane(CPlane()); };
    t["p.ctorCopy"] = [](In& i, Out& o) { const CPlane a = i.plane(); o.plane(CPlane(a)); };
    t["p.ctorNormalConst"] = [](In& i, Out& o) { const CVector3 n = i.v3(); o.plane(CPlane(n, i.f())); };
    t["p.ctor4"] = [](In& i, Out& o) {
        const float a = i.f(), b = i.f(), c = i.f(); o.plane(CPlane(a, b, c, i.f()));
    };
    t["p.getDistance"] = [](In& i, Out& o) { const CPlane p = i.plane(); o.f(p.getDistance(i.v3())); };
    t["p.getSidePoint"] = [](In& i, Out& o) { const CPlane p = i.plane(); o.u(static_cast<uint32_t>(p.getSide(i.v3()))); };
    t["p.getSideBox"] = [](In& i, Out& o) {
        const CPlane p = i.plane(); CAxisAlignedBox b; b.mMinimum = i.v3(); b.mMaximum = i.v3();
        b.mExtent = static_cast<int32_t>(i.u()); o.u(static_cast<uint32_t>(p.getSide(b)));
    };
    t["p.getSideCH"] = [](In& i, Out& o) {
        const CPlane p = i.plane(); const CVector3 c = i.v3(); o.u(static_cast<uint32_t>(p.getSide(c, i.v3())));
    };
    t["p.redefine3"] = [](In& i, Out& o) {
        CPlane p = i.plane(); const CVector3 a = i.v3(), b = i.v3(), c = i.v3(); p.redefine(a, b, c); o.plane(p);
    };
    t["p.ctor3"] = [](In& i, Out& o) { const CVector3 a = i.v3(), b = i.v3(), c = i.v3(); o.plane(CPlane(a, b, c)); };
    t["p.redefine2"] = [](In& i, Out& o) {
        CPlane p = i.plane(); const CVector3 n = i.v3(), q = i.v3(); p.redefine(n, q); o.plane(p);
    };
    t["p.ctor2"] = [](In& i, Out& o) { const CVector3 n = i.v3(), q = i.v3(); o.plane(CPlane(n, q)); };
    t["p.projectVector"] = [](In& i, Out& o) { const CPlane p = i.plane(); o.v3(p.projectVector(i.v3())); };
    t["p.normalise"] = [](In& i, Out& o) { CPlane p = i.plane(); const float l = p.normalise(); o.f(l); o.plane(p); };
    return t;
}

}  // namespace

int main(int argc, char** argv) {
    CUtils::setQuiet(true);
    CMath trig(4096);
    const auto t = registry();
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <function>|list\n", argv[0]);
        return 2;
    }
    const std::string name = argv[1];
    if (name == "list") {
        for (const auto& kv : t) std::printf("%s\n", kv.first.c_str());
        return 0;
    }
    const auto it = t.find(name);
    if (it == t.end()) {
        std::fprintf(stderr, "unknown function %s\n", name.c_str());
        return 2;
    }
    std::string line;
    std::vector<uint32_t> words;
    while (std::getline(std::cin, line)) {
        words.clear();
        std::istringstream ss(line);
        std::string tok;
        while (ss >> tok) words.push_back(static_cast<uint32_t>(std::stoul(tok, nullptr, 16)));
        In in{words};
        Out out;
        it->second(in, out);
        for (size_t k = 0; k < out.w.size(); ++k) std::printf(k ? " %08x" : "%08x", out.w[k]);
        std::printf("\n");
    }
    return 0;
}
