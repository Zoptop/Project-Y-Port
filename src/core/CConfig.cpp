#include "CConfig.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "CUtils.h"

namespace py {

namespace {

// Portable re-entrant strtok with the exact semantics of C strtok.
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

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

}  // namespace

CConfig& CConfig::get() {
    static CConfig instance;
    return instance;
}

bool CConfig::load(const std::string& path) {
    CUtils::log("Loading config %s", path.c_str());
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        CUtils::log("Can't load config");
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return loadFromBuffer(ss.str());
}

bool CConfig::loadFromBuffer(std::string buffer) {
    // Mirrors CConfig::load @ 0x15fe68 token for token.
    char* save = nullptr;
    char* line = tok(buffer.data(), "\n", &save);
    char* key = line;
    while (line != nullptr && key != nullptr) {
        key = tok(nullptr, " ", &save);
        if (key == nullptr) break;
        const char c = key[0];
        if (c != '#' && c != ' ' && c != '\r') {
            char* value = tok(nullptr, "#", &save);
            mValues[key] = value ? value : "";
        }
        line = tok(nullptr, "\n", &save);
    }
    return true;
}

bool CConfig::has(const std::string& key) const { return mValues.count(key) != 0; }

std::string CConfig::getRaw(const std::string& key) const {
    auto it = mValues.find(key);
    return it == mValues.end() ? std::string() : it->second;
}

std::string CConfig::getString(const std::string& key, const std::string& def) const {
    auto it = mValues.find(key);
    return it == mValues.end() ? def : trim(it->second);
}

int CConfig::getInt(const std::string& key, int def) const {
    auto it = mValues.find(key);
    return it == mValues.end() ? def : std::atoi(it->second.c_str());
}

unsigned CConfig::getUInt(const std::string& key, unsigned def) const {
    auto it = mValues.find(key);
    return it == mValues.end() ? def : static_cast<unsigned>(std::atoi(it->second.c_str()));
}

float CConfig::getFloat(const std::string& key, float def) const {
    auto it = mValues.find(key);
    if (it == mValues.end()) return def;
    // Original: strtod (double) narrowed to float - not strtof.
    return static_cast<float>(std::strtod(it->second.c_str(), nullptr));
}

bool CConfig::getBool(const std::string& key, bool def) const {
    auto it = mValues.find(key);
    if (it == mValues.end()) return def;
    return trim(it->second) != "false";
}

int CConfig::getFloats(const std::string& key, float* out, int n) const {
    auto it = mValues.find(key);
    if (it == mValues.end()) return 0;
    const char* p = it->second.c_str();
    int count = 0;
    while (count < n) {
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p) break;
        out[count++] = static_cast<float>(v);
        p = end;
    }
    return count;
}

void CConfig::updateConfigValue(unsigned& v, const std::string& key) const {
    v = static_cast<unsigned>(std::atoi(getRaw(key).c_str()));
}

void CConfig::updateConfigValue(int& v, const std::string& key) const { v = std::atoi(getRaw(key).c_str()); }

void CConfig::updateConfigValue(Real& v, const std::string& key) const {
    v = static_cast<float>(std::strtod(getRaw(key).c_str(), nullptr));
}

void CConfig::updateConfigValue(bool& v, const std::string& key) const { v = trim(getRaw(key)) != "false"; }

void CConfig::updateConfigValue(std::string& v, const std::string& key) const { v = trim(getRaw(key)); }

namespace {
// sscanf("%f ...") as bionic does it: strtod per field, narrowed to float;
// stops at the first field that does not parse.
int scanFloats(const std::string& s, Real* out, int n) {
    const char* p = s.c_str();
    int count = 0;
    while (count < n) {
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p) break;
        out[count++] = static_cast<float>(v);
        p = end;
    }
    return count;
}
}  // namespace

void CConfig::updateConfigValue(CVector2& v, const std::string& key) const {
    Real t[2] = {v.x, v.y};
    scanFloats(getRaw(key), t, 2);
    v.x = t[0];
    v.y = t[1];
}

void CConfig::updateConfigValue(CVector3& v, const std::string& key) const {
    Real t[3] = {v.x, v.y, v.z};
    scanFloats(getRaw(key), t, 3);
    v.x = t[0];
    v.y = t[1];
    v.z = t[2];
}

}  // namespace py
