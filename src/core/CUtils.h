// CUtils.h - logging, timing and data-path helpers (subset of ProjectY::CUtils).
#pragma once

#include <cstdarg>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "CVector2.h"
#include "FloatBits.h"
#include "Real.h"

namespace py {

// Logs "NOT PORTED YET: <what>" once per name (stubs of unported functions).
void notPorted(const char* what);

// Next allocation serial number: the order in which the original's bump-
// allocated objects would have been placed in memory. Used to order the
// original's pointer-keyed sets reproducibly (see docs/re/sim_phase4.md).
uint64_t allocSerial();

class CUtils {
public:
    // printf-style log line (original: __android_log_print + optional file).
    static void log(const char* fmt, ...);
    static void vlog(const char* fmt, va_list ap);
    static void setLogFile(const std::string& path);
    static void setQuiet(bool quiet);

    // Milliseconds since startTimer(), as float. The original computes
    // (sec*1000.0 + nsec/1e6) in double and narrows to float.
    static void startTimer();
    static float getTimer();

    // Directory holding the original game data (assets + map files).
    static void setDataDir(const std::string& dir);
    static const std::string& getDataDir();
    static std::string dataPath(const std::string& file);

    // Ghidra 0x3d23c0: lo if v < lo, else hi if v > hi, else v (NaN -> v).
    static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Lockstep checksum (Ghidra 0x3c2700..0x3c294c): a wrapping 32-bit sum of
    // raw words; floats contribute their bit pattern, bools 1 (true) / 2 (false).
    static uint32_t mChecksum;
    static void resetChecksum() { mChecksum = 0; }
    static void updateChecksum(float f) { add('f', fbits(f)); }
    static void updateChecksum(Real f) { add('f', fbits(f.v)); }
    static void updateChecksum(bool b) { add('b', b ? 1u : 2u); }
    static void updateChecksum(int i) { add('i', static_cast<uint32_t>(i)); }
    static void updateChecksum(unsigned u) { add('u', u); }
    static void updateChecksum(const CVector2& v) {
        updateChecksum(v.x);
        updateChecksum(v.y);
    }
    static void updateChecksum(const std::vector<bool>& v);
    // Debugging: when set, every update is also recorded (kind, value), as
    // tools/emu/game_oracle.py --trace records the original's.
    static std::vector<std::pair<char, uint32_t>>* mChecksumTrace;

private:
    static void add(char kind, uint32_t v) {
        mChecksum += v;
        if (mChecksumTrace) mChecksumTrace->push_back({kind, v});
    }
};

}  // namespace py
