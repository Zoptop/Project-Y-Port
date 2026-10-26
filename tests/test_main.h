// Minimal self-contained test harness (no external dependencies).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace pytest {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

}  // namespace pytest

#define PY_CONCAT2(a, b) a##b
#define PY_CONCAT(a, b) PY_CONCAT2(a, b)
#define TEST(name)                                                              \
    static void name();                                                         \
    static pytest::Registrar PY_CONCAT(reg_, name)(#name, name);                \
    static void name()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++pytest::failures();                                               \
        }                                                                       \
    } while (0)

#define CHECK_MSG(cond, ...)                                                    \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("  FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);     \
            std::printf(__VA_ARGS__);                                           \
            std::printf("\n");                                                  \
            ++pytest::failures();                                               \
        }                                                                       \
    } while (0)
