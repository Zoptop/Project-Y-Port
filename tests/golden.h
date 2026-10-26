// Golden determinism digest (see test_core.cpp: determinism_digest).
// Recorded from the reference build; every compiler/platform must match it.
#pragma once
#include <cstdint>

constexpr uint64_t kGoldenDigest = 0x2ba6e5f63ddc0dc3ull;   // GCC 13 + Clang 18, x86-64, -O0..-O2, -march=haswell
