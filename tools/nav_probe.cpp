// nav_probe - runs a navmesh scenario (see tools/emu/nav_oracle.py) on the
// port's CNav and prints the results in the oracle's format.
//
//   nav_probe --data data --scenario s.txt [--out res.txt]
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CNav.h"

using namespace py;

namespace {

uint64_t fnv1a(const unsigned char* p, int n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

float hexf(const std::string& s) { return bitsf(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }

std::string fmt(const char* f, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data", scenario, outFile;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data") data = next();
        else if (a == "--scenario") scenario = next();
        else if (a == "--out") outFile = next();
    }
    std::ifstream in(scenario);
    if (!in) {
        std::fprintf(stderr, "usage: nav_probe --data data --scenario s.txt [--out res.txt]\n");
        return 2;
    }
    CUtils::setDataDir(data);

    std::string line, navbase;
    std::vector<std::string> ops;
    while (std::getline(in, line)) {
        if (line.rfind("# navbase ", 0) == 0) navbase = line.substr(10);
        else if (!line.empty() && line[0] != '#') ops.push_back(line);
    }
    CNav nav;
    if (!nav.initnav(navbase.c_str())) {
        std::fprintf(stderr, "initnav(%s) failed\n", navbase.c_str());
        return 1;
    }

    std::vector<std::string> out;
    std::vector<dtObstacleRef> refs;
    std::vector<float> pts(3 * CNav::kMaxPolys);
    for (const std::string& op : ops) {
        std::istringstream ss(op);
        std::string w;
        std::vector<std::string> a;
        ss >> w;
        for (std::string t; ss >> t;) a.push_back(t);
        if (w == "tiles") {
            const dtNavMesh* m = nav.navMesh();
            for (int i = 0; i < m->getMaxTiles(); ++i) {
                const dtMeshTile* t = m->getTile(i);
                if (!t->header) continue;
                out.push_back(fmt("tile %d salt %u size %d fnv %016llx", i, t->salt, t->dataSize,
                                  static_cast<unsigned long long>(fnv1a(t->data, t->dataSize))));
            }
        } else if (w == "path") {
            int n = 0, done = 0;
            nav.calcPath(CVector2(hexf(a[0]), hexf(a[1])), CVector2(hexf(a[2]), hexf(a[3])), pts.data(), &n, &done);
            std::string s = fmt("path n %d done %d ", n, done);
            for (int i = 0; i < 3 * n; ++i) s += fmt(i ? " %08x" : "%08x", fbits(pts[i]));
            out.push_back(s);
        } else if (w == "onmesh") {
            CVector2 np;
            const bool r = nav.isOnNavMesh(CVector2(hexf(a[0]), hexf(a[1])), np);
            out.push_back(fmt("onmesh %d %08x %08x", r ? 1 : 0, fbits(np.x), fbits(np.y)));
        } else if (w == "addobs") {
            const float p[3] = {hexf(a[0]), hexf(a[1]), hexf(a[2])};
            dtObstacleRef ref = 0;
            const dtStatus st = nav.addTempObstacle(p, hexf(a[3]), &ref);
            refs.push_back(ref);
            out.push_back(fmt("addobs status %08x ref %08x", st, ref));
        } else if (w == "removeobs") {
            const dtObstacleRef ref = refs.at(std::stoul(a[0]));
            nav.removeTempObstacle(ref);
            out.push_back(fmt("removeobs %08x", ref));
        } else if (w == "update") {
            std::string s = "update";
            for (int i = 0, n = std::stoi(a[0]); i < n; ++i) s += fmt(" %x", nav.updateTileCache(0.1f));
            out.push_back(s);
        } else if (w == "clear") {
            nav.clearAllTempObstacles();
            out.push_back("clear");
        } else {
            std::fprintf(stderr, "unknown op %s\n", w.c_str());
            return 1;
        }
    }
    std::ofstream of;
    std::ostream& os = outFile.empty() ? std::cout : (of.open(outFile), of);
    for (const auto& s : out) os << s << '\n';
    return 0;
}
