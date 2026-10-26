// CMemoryFile.h - file access with the original lookup order.
//
// CMemoryFile::mopen (Ghidra 0x2cd1xx) looked for a file in the user
// directory's "add" folder first (downloaded maps, plain or <name>.zip),
// then inside the APK under assets/<name>. The port's data folder mirrors
// that: data/add/<name>, then data/<name>. (.zip variants are unpacked by
// tools/extract_assets.py, so they are not needed at runtime.)
#pragma once

#include <string>

namespace py {

class CMemoryFile {
public:
    // Returns the resolved path, or "" if the file does not exist.
    static std::string resolve(const std::string& name);
    // Reads the whole file; returns false if not found.
    static bool readAll(const std::string& name, std::string& out);
};

}  // namespace py
