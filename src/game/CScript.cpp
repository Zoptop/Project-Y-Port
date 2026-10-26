// CScript.cpp - base script class: parameter parsing and repeat handling.
#include "CScript.h"

#include <cerrno>
#include <cfloat>
#include <climits>
#include <cstdlib>
#include <stdexcept>

#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CScriptManager.h"

namespace py {

std::vector<std::string> tokenize(const std::string& s, const char* separators) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        bool sep = false;
        for (const char* p = separators; *p; ++p)
            if (*p == c) sep = true;
        if (sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

// std::istringstream(s) >> int  (libstdc++ num_get, decimal, C++11 failure rules).
// An empty / whitespace-only string fails in the stream sentry before num_get
// runs, leaving the caller's variable untouched (uninitialised in the
// original); the port returns 0 for that case.
int streamToInt(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && isSpace(s[i])) ++i;
    bool neg = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) neg = s[i++] == '-';
    long long v = 0;
    bool any = false, overflow = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        any = true;
        v = v * 10 + (s[i] - '0');
        if (v > 0x80000000LL) overflow = true, v = 0x80000000LL;
        ++i;
    }
    if (!any) return 0;
    if (neg) v = -v;
    if (overflow || v > INT_MAX) return v < 0 ? INT_MIN : INT_MAX;
    if (v < INT_MIN) return INT_MIN;
    return static_cast<int>(v);
}

// std::istringstream(s) >> float  (libstdc++ num_get collects the longest
// "[+-]digits[.digits][(e|E)[+-]digits]" prefix, then __convert_to_v<float>
// calls strtod and narrows to float; overflow saturates to +-FLT_MAX).
float streamToFloat(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && isSpace(s[i])) ++i;
    std::string buf;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) buf += s[i++];
    bool digits = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') buf += s[i++], digits = true;
    if (i < s.size() && s[i] == '.') {
        buf += s[i++];
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') buf += s[i++], digits = true;
    }
    if (digits && i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        buf += s[i++];
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) buf += s[i++];
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') buf += s[i++];
    }
    if (buf.empty()) return 0.0f;
    char* end = nullptr;
    const double d = std::strtod(buf.c_str(), &end);
    if (end == buf.c_str() || *end != '\0') return 0.0f;
    const float f = static_cast<float>(d);
    if (f > FLT_MAX) return FLT_MAX;
    if (f < -FLT_MAX) return -FLT_MAX;
    return f;
}

}  // namespace

// ---------------------------------------------------------------- CScript

CScript::CScript(const std::string& params) : mEngine(CEngine::getSingletonPtr()), mParamString(params) {}

// Ghidra 0x38802c
void CScript::enableRun() { mRunAt = Real(mEngine->mGameTime) + mDelay; }

// Ghidra 0x3881e4
void CScript::run() {
    if (!mLoopable)
        mFinished = true;
    else
        mRunAt = Real(mEngine->mGameTime) + mDelay;
}

// Ghidra 0x38963c
void CScript::parseParams() {
    const std::vector<std::string> toks = tokenize(mParamString, " \n\r");
    for (size_t i = 0; i < toks.size(); ++i) {
        const std::string& t = toks[i];
        const size_t q = t.find('"');
        if (q != std::string::npos) {
            bool closed = t.find('"', q + 1) != std::string::npos;
            std::string acc = t;
            while (i + 1 < toks.size() && !closed) {
                ++i;
                closed = toks[i].find('"') != std::string::npos;
                acc += " ";
                acc += toks[i];
            }
            const size_t p = acc.find('(');
            const std::string key = acc.substr(0, p);
            if (acc.size() < p + 2) throw std::out_of_range("basic_string::substr");
            mParams[key] = acc.substr(p + 2, acc.size() - p - 4);
            continue;
        }
        const size_t p = t.find('(');
        if (p != std::string::npos)
            mParams[t.substr(0, p)] = t.substr(p + 1, t.size() - p - 2);
        else
            mParams[t] = "";
    }

    // engine->game->level->scriptManager->counter++ (value before the increment)
    int counter = 0;
    if (CScriptManager* sm = CScriptManager::current()) counter = sm->mIdCounter++;
    mId = getParamInt("id", counter);
    mLoopable = getParamBool("loopable");
    mFixedSpeed = getParamBool("fixedspeed");
    getParamVarIntList("reqid", mReqIds);
    getParamVarIntList("xorid", mXorIds);
    const float ms = getParamFloat("delay", 0.0f);
    const float sec = ms * bitsf(0x3a83126fu);   // 0.001f
    const float factor = mFixedSpeed ? 1.0f : mEngine->getSpeedFactor();
    mDelay = sec * factor;
    mRunAt = mDelay;
}

int CScript::getParamInt(const std::string& key, int def) const {
    auto it = mParams.find(key);
    return it == mParams.end() ? def : streamToInt(it->second);
}

float CScript::getParamFloat(const std::string& key, float def) const {
    auto it = mParams.find(key);
    return it == mParams.end() ? def : streamToFloat(it->second);
}

bool CScript::getParamBool(const std::string& key) const { return mParams.count(key) != 0; }

std::string CScript::getParamString(const std::string& key) const {
    auto it = mParams.find(key);
    return it == mParams.end() ? std::string() : it->second;
}

CVector2 CScript::getParamVec2(const std::string& key, const CVector2& def) const {
    auto it = mParams.find(key);
    if (it == mParams.end()) return def;
    const std::vector<std::string> t = tokenize(it->second, ";");
    // [!] the original dereferences the second token unchecked (boost assert)
    if (t.size() < 2) throw std::runtime_error("CScript::getParamVec2: '" + key + "' needs x;y");
    return {streamToFloat(t[0]), streamToFloat(t[1])};
}

void CScript::getParamVarIntList(const std::string& key, std::vector<int>& out) const {
    auto it = mParams.find(key);
    if (it == mParams.end()) return;
    for (const std::string& t : tokenize(it->second, ";")) out.push_back(streamToInt(t));
}

}  // namespace py
