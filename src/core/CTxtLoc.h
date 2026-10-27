// CTxtLoc.h - port of ProjectY::CTxtLoc (strings<Language>.txt).
//
// Format (CTxtLoc::load): first line is a header; then key = text up to the
// first ' ', value = rest of the line (including a trailing '\r' from the
// CRLF files - kept, as in the original). Files are Latin-1.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace py {

class CTxtLoc {
public:
    static CTxtLoc& get();

    // language: "English" or "Deutsch"
    bool load(const std::string& language);
    bool loadFromBuffer(std::string buffer);

    // Raw value as stored by the original (may end in '\r').
    const std::string& getS(const std::string& key) const;
    // Value without trailing CR/LF, for display.
    std::string text(const std::string& key) const;

    // CTxtLoc::getS / getC(ETxtLocIDs) (Ghidra 0x39cbd8 / 0x39cbfc): the
    // strings by index in load order. The index advances once per key token
    // read by CTxtLoc::load, comment/blank tokens included (see loadFromBuffer),
    // which for the shipped files is (line number - 2). 0x293 slots; values
    // keep their trailing '\r' like the original.
    static constexpr int kNumIds = 0x293;
    const std::string& getS(int id) const;
    const char* getC(int id) const { return getS(id).c_str(); }

    size_t size() const { return mTxts.size(); }

private:
    std::map<std::string, std::string> mTxts;
    std::vector<std::string> mIndexed = std::vector<std::string>(kNumIds);
    std::string mEmpty;
};

}  // namespace py
