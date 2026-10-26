// CConfig.h - port of ProjectY::CConfig loading (config.txt).
//
// File format (CConfig::load): the buffer is tokenised with strtok exactly
// like the original - the first line is consumed as a header, then for each
// line: key = up to the first ' ', value = up to the next '#', rest of the
// line discarded. Lines whose key starts with '#', ' ' or '\r' are skipped.
// Typed reads mirror CConfig::updateConfigValue:
//   uint/int -> atoi, float -> (float)strtod (note: double then narrowed),
//   string -> trimmed, bool -> anything but "false" is true,
//   CVector2/3 -> whitespace-separated floats.
// The original copies values into ~400 named struct fields in refresh();
// the port keeps the key/value map and reads it through typed getters.
#pragma once

#include <map>
#include <string>

#include "CVector2.h"
#include "CVector3.h"
#include "Real.h"

namespace py {

class CConfig {
public:
    static CConfig& get();

    bool load(const std::string& path);
    bool loadFromBuffer(std::string buffer);

    bool has(const std::string& key) const;
    std::string getRaw(const std::string& key) const;   // untrimmed

    std::string getString(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def = 0) const;
    unsigned getUInt(const std::string& key, unsigned def = 0) const;
    float getFloat(const std::string& key, float def = 0.0f) const;
    bool getBool(const std::string& key, bool def = false) const;
    // Fills up to n floats; returns how many were parsed.
    int getFloats(const std::string& key, float* out, int n) const;

    const std::map<std::string, std::string>& values() const { return mValues; }

    // CConfig::updateConfigValue (Ghidra 0x15f350..0x15f70c), exactly: a
    // missing key reads as "" (so numbers become 0 and bools true), and the
    // vector forms sscanf "%f %f[ %f]" (missing / short values leave the
    // remaining components unchanged).
    void updateConfigValue(unsigned& v, const std::string& key) const;
    void updateConfigValue(int& v, const std::string& key) const;
    void updateConfigValue(Real& v, const std::string& key) const;
    void updateConfigValue(bool& v, const std::string& key) const;
    void updateConfigValue(std::string& v, const std::string& key) const;
    void updateConfigValue(CVector2& v, const std::string& key) const;
    void updateConfigValue(CVector3& v, const std::string& key) const;

    // ---- per-game switches the original keeps in CConfig (+0x124..+0x15c).
    // CLevel::initByScript resets them before reading a map; the map script
    // ("set unitavailability", "set kisend2repair", ...) and the skirmish
    // presets (applySpecialUnitAvailability) change them.
    static constexpr int kUnitTypes = 30;
    static constexpr int kCrateActions = 20;
    bool mUnitAvailable[kUnitTypes] = {};          // +0x124 + unit type
    bool mCrateActionAvailable[kCrateActions] = {};  // +0x142 + action
    bool mKISend2Repair = true;                    // +0x156
    bool mKIEnterUnits = true;                     // +0x157
    bool mKIGetsCrates = true;                     // +0x158
    bool mNeutralUnitAutoDamage = true;            // +0x159
    bool mAllNonCmdUnitAutoDamage = false;         // +0x15a
    float mAllNonCmdUnitAutoDamageValue = 0.0f;    // +0x15c

private:
    std::map<std::string, std::string> mValues;
};

}  // namespace py
