#include "CUtils.h"

#include <set>

#include <chrono>
#include <cstdio>
#include <mutex>

namespace py {

namespace {
std::mutex gLogMutex;
FILE* gLogFile = nullptr;
bool gQuiet = false;
std::string gDataDir = "data";
std::chrono::steady_clock::time_point gTimerStart = std::chrono::steady_clock::now();
}  // namespace

void CUtils::vlog(const char* fmt, va_list ap) {
    char buf[2048];
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    std::lock_guard<std::mutex> lock(gLogMutex);
    if (!gQuiet) std::fprintf(stderr, "[ProjectY] %s\n", buf);
    if (gLogFile) {
        std::fprintf(gLogFile, "%s\n", buf);
        std::fflush(gLogFile);
    }
}

void CUtils::log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(fmt, ap);
    va_end(ap);
}

void CUtils::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(gLogMutex);
    if (gLogFile) std::fclose(gLogFile);
    gLogFile = std::fopen(path.c_str(), "w");
}

void CUtils::setQuiet(bool quiet) { gQuiet = quiet; }

void CUtils::startTimer() { gTimerStart = std::chrono::steady_clock::now(); }

float CUtils::getTimer() {
    using namespace std::chrono;
    const auto d = steady_clock::now() - gTimerStart;
    const auto secs = duration_cast<seconds>(d);
    const auto nsec = duration_cast<nanoseconds>(d - secs);
    const double ms = static_cast<double>(secs.count()) * 1000.0 + static_cast<double>(nsec.count()) / 1000000.0;
    return static_cast<float>(ms);
}

void CUtils::setDataDir(const std::string& dir) { gDataDir = dir; }

const std::string& CUtils::getDataDir() { return gDataDir; }

std::string CUtils::dataPath(const std::string& file) {
    if (gDataDir.empty()) return file;
    const char last = gDataDir.back();
    if (last == '/' || last == '\\') return gDataDir + file;
    return gDataDir + "/" + file;
}

uint32_t CUtils::mChecksum = 0;
std::vector<std::pair<char, uint32_t>>* CUtils::mChecksumTrace = nullptr;

// Ghidra 0x3c2790: one update per element.
void CUtils::updateChecksum(const std::vector<bool>& v) {
    uint32_t sum = 0;
    for (bool b : v) {
        mChecksum += b ? 1u : 2u;
        sum += b ? 1u : 2u;
    }
    if (mChecksumTrace) mChecksumTrace->push_back({'v', sum});
}

void notPorted(const char* what) {
    static std::set<std::string> seen;
    if (seen.insert(what).second) CUtils::log("NOT PORTED YET: %s", what);
}

uint64_t allocSerial() {
    static uint64_t next = 1;
    return next++;
}

}  // namespace py
