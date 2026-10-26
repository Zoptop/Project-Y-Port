#include "CMemoryFile.h"

#include <fstream>
#include <sstream>

#include "CUtils.h"

namespace py {

namespace {
bool exists(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return static_cast<bool>(f);
}
}  // namespace

std::string CMemoryFile::resolve(const std::string& name) {
    const std::string add = CUtils::dataPath("add/" + name);
    if (exists(add)) return add;
    const std::string base = CUtils::dataPath(name);
    if (exists(base)) return base;
    return std::string();
}

bool CMemoryFile::readAll(const std::string& name, std::string& out) {
    const std::string path = resolve(name);
    if (path.empty()) return false;
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

}  // namespace py
