// CTxtLoc.h - port of ProjectY::CTxtLoc (strings<Language>.txt).
//
// Format (CTxtLoc::load): first line is a header; then key = text up to the
// first ' ', value = rest of the line (including a trailing '\r' from the
// CRLF files - kept, as in the original). Files are Latin-1.
#pragma once

#include <map>
#include <string>

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

    size_t size() const { return mTxts.size(); }

private:
    std::map<std::string, std::string> mTxts;
    std::string mEmpty;
};

}  // namespace py
