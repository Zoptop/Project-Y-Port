// Phase 2 tests: mesh parsing, shader file handling, level scripts.
// Expected values were computed independently (Python over the original
// files), not copied from the port's output. Tests that need the game data
// skip themselves when data/ is missing (CI).
#include <cmath>
#include <string>

#include "../src/core/CConfig.h"
#include "../src/core/CMemoryFile.h"
#include "../src/core/CUtils.h"
#include "../src/core/FloatBits.h"
#include "../src/game/CLevel.h"
#include "../src/render/CGLSLProgram.h"
#include "../src/render/CMesh.h"
#include "test_main.h"

using py::fbits;

namespace {
bool haveData(const char* file) {
    if (py::CMemoryFile::resolve(file).empty()) {
        std::printf("    (skipped: %s not found - run tools/extract_assets.py)\n", file);
        return false;
    }
    return true;
}
std::string readData(const char* file) {
    std::string s;
    py::CMemoryFile::readAll(file, s);
    return s;
}
}  // namespace

TEST(mesh_parser_synthetic) {
    // v3 header, layout taken from the LAST submesh line, short lines keep zeros.
    const char* text =
        "meshversion 3\r\n"
        "meshcount 2\r\n"
        "mesh a 0 1 1\r\n"
        "mesh b 1 1 2\r\n"
        "2\r\n"
        "0 1 2\r\n"
        "2 1 3\r\n"
        "4\r\n"
        "1 2 3 0.5 0.25 7 8 0 0 1\r\n"
        "4 5 6 0.1 0.2 9 10\r\n"
        "7 8 9 0.3 0.4 0 0 1\r\n"
        "1 1 1 1 1 1 1 1 1 1\r\n";
    py::CMesh unlit;
    CHECK(unlit.parse(text, false, true));
    CHECK(unlit.version() == 3 && unlit.layout() == 2);
    CHECK(unlit.triangleCount() == 2 && unlit.vertexCount() == 4);
    uint32_t f = 9, c = 9;
    CHECK(unlit.getSubMeshData("b", f, c) && f == 1 && c == 1);
    CHECK(unlit.indices()[3] == 2 && unlit.indices()[5] == 3);
    const auto& v = unlit.vertices();
    CHECK(v[0].px == 1 && v[0].u == 0.5f && v[0].s0 == 7 && v[0].s1 == 8);   // TEXCOORD1 = u2 v2
    CHECK(v[1].s0 == 9 && v[1].s1 == 10);
    CHECK(v[2].s0 == 0 && v[2].s1 == 0);   // 8-number line under layout 2: u2 v2 = the 6th/7th numbers
    py::CMesh lit;
    CHECK(lit.parse(text, true, false));
    CHECK(lit.vertices()[0].s0 == 0 && lit.vertices()[0].s2 == 1);   // lit layout 2: (u2 v2) skipped, normal
    CHECK(!lit.getSubMeshData("missing", f, c) && f == 0 && c == 0);
}

TEST(mesh_parser_original_files) {
    if (!haveData("gameobjects.3DS.mesh") || !haveData("map1.DAE.mesh")) return;
    py::CMesh go;
    CHECK(go.parse(readData("gameobjects.3DS.mesh"), true, false));
    CHECK(go.version() == 3 && go.layout() == 1);
    CHECK(go.triangleCount() == 18443 && go.vertexCount() == 26781);
    CHECK(go.subMeshes().size() == 55);
    uint32_t f = 0, c = 0;
    CHECK(go.getSubMeshData("Spider1", f, c) && f == 2737 && c == 216);
    const auto& g0 = go.vertices()[0];
    CHECK(fbits(g0.px) == 0x4124ace0u && fbits(g0.py) == 0xc06fc67eu && fbits(g0.u) == 0x3f27912eu);
    CHECK(fbits(g0.s0) == 0x3f4f0c99u && fbits(g0.s2) == 0x358637bdu);   // normal

    py::CMesh map;
    CHECK(map.parse(readData("map1.DAE.mesh"), false, true));
    CHECK(map.layout() == 2 && map.triangleCount() == 51702 && map.vertexCount() == 28648);
    CHECK(map.getSubMeshData("water", f, c) && f == 49324 && c == 229);
    const auto& m0 = map.vertices()[0];
    CHECK(fbits(m0.px) == 0xc21883f2u && fbits(m0.v) == 0xbf22fec5u);
    CHECK(fbits(m0.s0) == 0x3ef63d3eu && fbits(m0.s1) == 0xbf13167fu);   // TEXCOORD1 (FOW UV)

    if (!haveData("map201.DAE.mesh")) return;
    py::CMesh m201;
    CHECK(m201.parse(readData("map201.DAE.mesh"), false, true));
    CHECK(m201.layout() == 2);   // last submesh line says 2, although "mm"/"mmborder" are layout 1
    // vertex 39161 (first of "mm") has 8 numbers: px py pz u v nx ny nz -> u2 v2 = nx ny = 0 0
    const auto& q = m201.vertices()[39161];
    CHECK(fbits(q.px) == fbits(-392.311859f) && q.s0 == 0.0f && q.s1 == 0.0f);
}

TEST(gfx_split_and_desktop_translation) {
    const char* gfx =
        "GL_VERTEX_SHADER\n"
        "attribute mediump vec3 POSITION;\nvarying mediump vec2 uv;\n"
        "void main( void ) { gl_Position = vec4(POSITION, 1.0); }\n"
        "GL_FRAGMENT_SHADER\n"
        "uniform sampler2D T;\nvarying mediump vec2 uv;\n"
        "void main( void ) \n{\n gl_FragColor.rgb = texture2D(T, uv).rgb; }\n";
    std::string vs, fs;
    CHECK(py::CGLSLProgram::splitGfx(gfx, vs, fs));
    CHECK(vs.find("attribute") != std::string::npos && fs.find("sampler2D") != std::string::npos);
    const std::string v = py::CGLSLProgram::translateForDesktop(vs, false);
    const std::string f = py::CGLSLProgram::translateForDesktop(fs, true);
    CHECK(v.rfind("#version 330 core", 0) == 0);
    CHECK(v.find("in mediump vec3 POSITION") != std::string::npos);
    CHECK(v.find("out mediump vec2 uv") != std::string::npos);
    CHECK(f.find("in mediump vec2 uv") != std::string::npos);
    CHECK(f.find("texture(T, uv)") != std::string::npos);
    CHECK(f.find("py_FragColor.rgb") != std::string::npos);
    CHECK(f.find("gl_FragColor") == std::string::npos);
    CHECK(f.find("py_FragColor = vec4(0.0, 0.0, 0.0, 1.0);") != std::string::npos);
    CHECK(!py::CGLSLProgram::splitGfx("no markers here", vs, fs));
}

TEST(all_original_shaders_split) {
    if (!haveData("mapb.gfx")) return;
    const char* names[] = {"mapb.gfx", "mapb_nofow.gfx", "cube.gfx", "cube_nofow.gfx", "diffuselight.gfx",
                           "diffuseplayerlight.gfx", "diffuseplayersector_nofow.gfx",
                           "playercolorsector_nofow.gfx", "simple.gfx", "font.gfx", "shadow.gfx",
                           "particlesgray.gfx", "interfacecolored.gfx"};
    for (const char* n : names) {
        std::string vs, fs;
        CHECK_MSG(py::CGLSLProgram::splitGfx(readData(n), vs, fs), "%s", n);
    }
}

TEST(level_script_map1) {
    if (!haveData("map1English.txt")) return;
    if (!py::CConfig::get().has("Player1Color")) py::CConfig::get().load(py::CUtils::dataPath("config.txt"));
    py::CLevel lvl;
    CHECK(lvl.parseScript(readData("map1English.txt")));
    CHECK(lvl.mapName() == "Icy Isle");
    CHECK(lvl.mapWidth() == 1000.0f && lvl.mapHeight() == 1000.0f);
    CHECK(lvl.assets().size() == 168);
    CHECK(lvl.sectors().size() == 10);
    CHECK(lvl.sectors()[0].owner == 1 && lvl.sectors()[9].owner == 2);
    CHECK(lvl.loads().at("mapmesh") == "map1.DAE.mesh");
    CHECK(lvl.detailTextures()[1] == "map_tx_snow.png");
    CHECK(lvl.waterActive());
    // lightdirection 1.0 -0.3 -1.0 -> negated and normalised
    const py::Vec3 d = lvl.lightDirection();
    const double n = std::sqrt(1.0 + 0.09 + 1.0);
    CHECK(std::fabs(d.x - (-1.0 / n)) < 1e-6 && std::fabs(d.y - (0.3 / n)) < 1e-6 && std::fabs(d.z - (1.0 / n)) < 1e-6);
    CHECK(lvl.lightColor().x == 1.5f);
    // first asset: "create asset silo position -379.802 -421.227 0.0 quat 0.0 0.0 -0.00468705 0.999989 scale 1 1 1"
    const auto& a = lvl.assets()[0];
    CHECK(a.name == "silo" && a.position.x == -379.802f && a.position.z == 0.0f);
    CHECK(a.rotation.w == 0.999989f && a.rotation.z == -0.00468705f && a.scale.y == 1.0f);
    CHECK(lvl.playerColor(1).x == 1.0f && lvl.playerColor(1).y == 0.0f);   // Player1Color 1 0 0
}
