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

private:
    std::map<std::string, std::string> mValues;
};

}  // namespace py
