#include "test_main.h"

#include <cstring>

#include "../src/core/CMath.h"
#include "../src/core/CUtils.h"

int main(int argc, char** argv) {
    py::CUtils::setQuiet(true);
    const char* only = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (auto& c : pytest::registry()) {
        if (only && std::strstr(c.name, only) == nullptr) continue;
        const int before = pytest::failures();
        std::printf("[ RUN  ] %s\n", c.name);
        std::fflush(stdout);
        c.fn();
        std::printf("[ %s ] %s\n", pytest::failures() == before ? " OK " : "FAIL", c.name);
        ++run;
    }
    std::printf("\n%d tests, %d failed checks\n", run, pytest::failures());
    return pytest::failures() == 0 ? 0 : 1;
}
