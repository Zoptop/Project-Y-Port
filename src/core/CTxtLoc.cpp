#include "CTxtLoc.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include "CUtils.h"

namespace py {

namespace {
char* tok(char* s, const char* delim, char** save) {
    if (s == nullptr) s = *save;
    if (s == nullptr) return nullptr;
    s += std::strspn(s, delim);
    if (*s == '\0') {
        *save = nullptr;
        return nullptr;
    }
    char* end = s + std::strcspn(s, delim);
    if (*end == '\0') {
        *save = nullptr;
    } else {
        *end = '\0';
        *save = end + 1;
    }
    return s;
}
}  // namespace

CTxtLoc& CTxtLoc::get() {
    static CTxtLoc instance;
    return instance;
}

bool CTxtLoc::load(const std::string& language) {
    const std::string file = CUtils::dataPath("strings" + language + ".txt");
    CUtils::log("Loading strings from %s", file.c_str());
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        CUtils::log("Can't load config");   // original message, sic
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return loadFromBuffer(ss.str());
}

bool CTxtLoc::loadFromBuffer(std::string buffer) {
    // The indexed table (mTxts vector of the original, Ghidra 0x39cd68),
    // replayed with the original's exact strtok sequence: after the header
    // line, each iteration reads a key with strtok(" "); a key that doesn't
    // start with '#', ' ' or '\r' takes the rest of the line as its value
    // (strtok("\n")) into slot i; a comment/blank key consumes nothing more
    // (its remaining words become the next keys). i advances every iteration.
    {
        std::string copy = buffer;
        char* save = nullptr;
        char* line = tok(copy.data(), "\n", &save);
        int i = 0;
        if (line != nullptr) {
            do {
                char* key = tok(nullptr, " ", &save);
                if (key == nullptr) break;
                const char c = key[0];
                if (c != '#' && c != ' ' && c != '\r') {
                    line = tok(nullptr, "\n", &save);
                    if (i >= 0 && i < kNumIds) mIndexed[static_cast<size_t>(i)] = line ? line : "";
                }
                ++i;
            } while (line != nullptr);
        }
    }
    // Mirrors CTxtLoc::load @ 0x39cbxx: header line, then key ' ' value '\n'.
    char* save = nullptr;
    char* line = tok(buffer.data(), "\n", &save);
    while (line != nullptr) {
        char* key = tok(nullptr, " ", &save);
        if (key == nullptr) break;
        const char c = key[0];
        if (c != '#' && c != ' ' && c != '\r') {
            line = tok(nullptr, "\n", &save);
            mTxts[key] = line ? line : "";
        } else {
            line = tok(nullptr, "\n", &save);
        }
    }
    return true;
}

const std::string& CTxtLoc::getS(const std::string& key) const {
    auto it = mTxts.find(key);
    return it == mTxts.end() ? mEmpty : it->second;
}

const std::string& CTxtLoc::getS(int id) const {
    if (id < 0 || id >= kNumIds) return mEmpty;
    return mIndexed[static_cast<size_t>(id)];
}

std::string CTxtLoc::text(const std::string& key) const {
    std::string s = getS(key);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

}  // namespace py
