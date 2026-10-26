// CMesh.h - port of ProjectY::CMesh / CMeshManager (the text ".mesh" format).
//
// Format (CMesh::init, Ghidra 0x2cxxxx; see docs/re/scripts_rendering_ui.md B.9):
//   meshversion <v>
//   v=2: "<w> <count>" then count x "<w> <name> <firstTri> <triCount>"            layout = 1
//   v=3: "<w> <count>" then count x "<w> <name> <firstTri> <triCount> <layout>"   layout = value on the LAST line
//   <triCount>  then "%hi %hi %hi" per triangle (16-bit indices)
//   <vertCount> then one vertex per line, parsed for the file-wide layout:
//     lit,  layout 1 : px py pz u v nx ny nz            -> VBO slot@20 = normal
//     lit,  layout 2 : px py pz u v (u2 v2) nx ny nz    -> VBO slot@20 = normal
//     !lit, layout 1 : px py pz u v                     -> slot@20 unused
//     !lit, layout 2 : px py pz u v u2 v2               -> slot@20 = TEXCOORD1 (map meshes)
//   Lines with fewer numbers leave the remaining slots untouched (sscanf); the
//   port zero-initialises them (map201's "mm"/"mmborder" hit this case).
// VBO stride is 32 bytes: POSITION@0, TEXCOORD0@12, slot@20.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace py {

struct MeshVertex {
    float px, py, pz;
    float u, v;
    float s0, s1, s2;   // normal, or TEXCOORD1 in s0/s1
};
static_assert(sizeof(MeshVertex) == 32, "VBO stride must be 32 bytes");

class CMesh {
public:
    ~CMesh();
    bool init(const std::string& name, bool lit, bool keepCPU);
    bool parse(std::string text, bool lit, bool keepCPU);   // without GL upload (tests)
    void upload();

    bool getSubMeshData(const std::string& name, uint32_t& firstTri, uint32_t& triCount) const;
    bool hasSubMesh(const std::string& name) const { return mSubMeshes.count(name) != 0; }
    void bindBuffer() const;
    void draw(uint32_t firstTri, uint32_t triCount) const;

    int version() const { return mVersion; }
    int layout() const { return mLayout; }
    size_t triangleCount() const { return mIndices.size() / 3; }
    size_t vertexCount() const { return mVertices.size(); }
    const std::vector<MeshVertex>& vertices() const { return mVertices; }
    const std::vector<uint16_t>& indices() const { return mIndices; }
    const std::map<std::string, std::pair<uint32_t, uint32_t>>& subMeshes() const { return mSubMeshes; }

private:
    std::string mName;
    int mVersion = 0;
    int mLayout = 1;
    bool mLit = true;
    bool mKeepCPU = false;
    std::map<std::string, std::pair<uint32_t, uint32_t>> mSubMeshes;
    std::vector<MeshVertex> mVertices;
    std::vector<uint16_t> mIndices;
    unsigned mVBO = 0, mIBO = 0;
};

class CMeshManager {
public:
    static CMeshManager& get();
    CMesh* getMesh(const std::string& name, bool lit, bool keepCPU);
    void clear();

    // Headless: meshes are parsed (submesh tables, CPU data) but never
    // uploaded, so map loading works without an OpenGL context (tests, tools).
    static void setHeadless(bool on) { sHeadless = on; }
    static bool headless() { return sHeadless; }

private:
    static bool sHeadless;
    std::map<std::string, std::unique_ptr<CMesh>> mMeshes;
};

}  // namespace py
