// CSaveIO.h - the text I/O of the savegames (CEngine::saveGame / loadGame,
// Ghidra 0x16c448 / 0x168bec, and every class's save/load).
//
// The original writes "Key value" lines with fprintf and reads them back
// with CUtils::safeReadFromFile (one fgets + sscanf per key). Floats go
// through "%f" (6 decimals) and sscanf "%f" (strtof), both in bionic. The
// port must produce the same text on every platform, so it does not leave
// the float conversions to the host's C library:
//   - formatFixed is the exact decimal expansion of the value, rounded to
//     the requested decimals with ties to even (bionic's gdtoa, glibc and
//     Python agree; e.g. 1/128 -> "0.007812"), "inf"/"-inf", and "nan"
//     without a sign (bionic's vfprintf drops the sign of a NaN);
//   - parseFloat is strtof: the correctly rounded binary32 value of the
//     decimal string (no detour through double, which could round twice).
// Integers and strings use the host's printf/sscanf (identical for the
// formats used here).
#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

namespace py {
namespace saveio {

// C's "%.<decimals>f" of v.
std::string formatFixed(double v, int decimals);

// strtof on s (leading white space, sign, digits with an optional '.' and
// exponent, "inf"/"infinity"/"nan", case-insensitive). Returns the number of
// characters used, 0 if there is no number (out unchanged).
size_t parseFloat(const char* s, float& out);

// The original's fprintf/sprintf with the conversions the savegame code
// uses: %i %d %u %li %ld %c %s %% with flags/width (e.g. %02i), %hhu, and %f
// / %.<n>f through formatFixed. (A float argument arrives as double, as in
// C varargs.)
std::string vformat(const char* fmt, va_list ap);
std::string format(const char* fmt, ...);
void writef(FILE* f, const char* fmt, ...);

// Writing "%i" with a double argument, as CInterfaceIngamePerPlayer::save
// does for "TimerRuntime %i" (Ghidra 0x1a5650): on the original's soft-float
// ARM ABI the double goes into r2:r3 and %i prints r2, the low word of the
// double's bit pattern. [!]
int lowWordOfDouble(double d);

// a float for a "%f" argument (C varargs promote float to double)
inline double D(float v) { return static_cast<double>(v); }
// an unsigned for "%i" (the original passes the raw word: 0xffffffff prints -1)
inline int I(unsigned v) { return static_cast<int>(v); }

}  // namespace saveio
}  // namespace py
