// Dumps the CMath trig tables and RNG streams (for tools/emu/check_core.py).
#include <cstdio>
#include "core/CMath.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
using namespace py;
int main() {
    CUtils::setQuiet(true);
    CMath m(4096);
    for (int i = 0; i < 4096; ++i) {
        const float x = (static_cast<float>(i) + 0.5f) * CMath::TWO_PI / 4096.0f;
        std::printf("sin %08x\ntan %08x\n", fbits(CMath::SinTable(x)), fbits(CMath::TanTable(x)));
    }
    CRngSafe::SetSeed(0x12345u, 0x6789u);
    CRng::SetSeed(0x2468u, 0x1357u);
    for (int i = 0; i < 2000; ++i) {
        std::printf("su %08x\n", fbits(CRngSafe::GetUniform()));
        std::printf("sn %08x\n", fbits(CRngSafe::GetNormal(2.0f, 3.0f)));
        std::printf("se %08x\n", fbits(CRngSafe::GetExponential(5.0f)));
        std::printf("ru %08x\n", fbits(CRng::GetUniform()));
        std::printf("rn %08x\n", fbits(CRng::GetNormal()));
    }
}
