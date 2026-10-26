#include "CLevel.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CNav.h"
#include "core/fdlibm.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CScript.h"
#include "game/CScriptManager.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_DEPTH_TEST_ = 0x0B71, GL_CULL_FACE_ = 0x0B44, GL_BLEND_ = 0x0BE2;
constexpr unsigned GL_SRC_ALPHA_ = 0x0302, GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;
const Vec3 kGray25(0.25f, 0.25f, 0.25f), kGray75(0.75f, 0.75f, 0.75f);

// sscanf buffers: the original uses 20/20/256/1024-byte stack arrays (and
// overflows them on longer words); the port uses one size with a width limit.
constexpr size_t kBuf = 1024;
#define PY_S "%1023s"
}  // namespace

// Ghidra 0x2df96c
CLevel::CLevel() : mScriptManager(std::make_unique<CScriptManager>(CEngine::getSingletonPtr())) {}

CLevel::~CLevel() = default;

CTexture* CLevel::texture(const std::string& name, unsigned type) const {
    if (mHeadless) return nullptr;
    return CTextureManager::get().getTexture(name, type, 2, 2, true);
}

CGLSLProgram* CLevel::shader(const std::string& name) const {
    if (mHeadless) return nullptr;
    return CGLSLProgramManager::get().getGLSLProgram(name);
}

// ------------------------------------------------------------------ helpers

// Ghidra 0x2da6f8
CSector* CLevel::getSector(unsigned id) const {
    auto it = mSectors.find(id);
    if (it != mSectors.end()) return it->second.get();
    CUtils::log("ERROR: CLevel::getSector sector %i not found", id);
    return nullptr;
}

// Ghidra 0x2e0324 (an existing id keeps its sector and the count stays)
void CLevel::createSector(unsigned id, unsigned playerId) {
    auto s = std::make_unique<CSector>(id, playerId);
    if (mSectors.count(id)) return;
    mSectors[id] = std::move(s);
    ++mNumSectors;
}

// Ghidra 0x2dabe8. Unit availability decides which factory a map gets.
int CLevel::getBuildingTypeAlternative(int type) const {
    const bool* u = CConfig::get().mUnitAvailable;
    const int gameType = CEngine::getSingletonPtr()->mGameType;
    if (type == BUILDING_REFINERY) return gameType == 3 ? BUILDING_REFINERY : -1;
    if (type == BUILDING_PRODUCTION_ADVTECH) {
        if (u[14] || u[13] || u[18] || u[16]) return type;
        type = BUILDING_PRODUCTION_LARGE;
    }
    if (type == BUILDING_PRODUCTION_LARGE) {
        if (u[5] || u[10] || u[8] || u[24]) return type;
        type = BUILDING_PRODUCTION_MEDIUM;
    }
    if (type == BUILDING_PRODUCTION_MEDIUM) {
        if (!u[2] && !u[3] && !u[17] && !u[12]) return BUILDING_PRODUCTION_LIGHT;
    }
    return type;
}

// Ghidra 0x2dace8. Skirmish "unit availability" presets.
void CLevel::applySpecialUnitAvailability(int mode) {
    bool* u = CConfig::get().mUnitAvailable;
    u[22] = u[23] = true;
    u[1] = u[15] = u[4] = u[7] = u[11] = true;
    u[24] = CEngine::getSingletonPtr()->mGameType == 3;
    auto set = [u](std::initializer_list<int> ids, bool v) {
        for (int i : ids) u[i] = v;
    };
    switch (mode) {
        case 1:
            set({2, 3, 5, 6, 8, 9, 10}, true);
            set({12, 13, 14, 16, 17, 18}, true);
            set({19, 20, 21}, true);
            break;
        case 2:
            set({2, 3, 5}, false);
            u[6] = true;
            u[8] = false;
            u[9] = true;
            set({10, 12, 13, 14, 16, 17, 18}, false);
            set({19, 20, 21}, true);
            break;
        case 3:
            set({2, 3}, true);
            u[5] = false;
            u[6] = true;
            u[8] = false;
            u[9] = true;
            u[10] = false;
            u[12] = true;
            set({13, 14, 16}, false);
            u[17] = true;
            u[18] = false;
            set({19, 20, 21}, true);
            break;
        case 4:
            set({2, 3}, false);
            set({5, 6, 8, 9, 10}, true);
            u[12] = false;
            set({13, 14, 16}, true);
            u[17] = false;
            u[18] = true;
            set({19, 20, 21}, true);
            break;
        case 5:
            set({2, 3, 5, 6, 8, 9, 10}, true);
            set({12, 13, 14, 16, 17, 18}, false);
            set({19, 20, 21}, false);
            break;
        case 6:
            set({2, 3}, true);
            u[5] = false;
            set({6, 8, 9, 10, 12, 13, 14}, true);
            u[16] = false;
            set({17, 18}, true);
            u[19] = false;
            set({20, 21}, true);
            break;
        case 7: {
            // CRngSafe::GetUniform(0x12) > 0.5 per type, in this order
            for (int i : {2, 3, 5, 6, 8}) u[i] = CRngSafe::GetUniform(0x12) > 0.5f;
            u[9] = true;
            for (int i : {10, 12, 13, 14, 16, 17, 18, 19, 20, 21}) u[i] = CRngSafe::GetUniform(0x12) > 0.5f;
            if (!u[5] && !u[10] && !u[2] && !u[3]) u[9] = true;
            break;
        }
        default:
            break;
    }
}

// Ghidra 0x2e0408. Skirmish "sector setup" presets. A pre-pass over the
// whole map file collects the HQ sector of each player and the flagpost
// position of each sector (raw building types, before alternatives).
void CLevel::applySpecialSectorSetup(int mode, const std::vector<std::string>& lines) {
    CGame* game = CEngine::getSingletonPtr()->mGame;
    CSector* hqSector[5] = {};          // by player id
    bool hasHQ[54] = {};                // by sector id
    CVector2 flagPos[50];               // by sector id

    char w1[kBuf] = {0}, w2[kBuf] = {0}, w3[kBuf] = {0};
    int type = 0, subtype = 0, id = 0;
    unsigned sec = 0;
    float x = 0, y = 0, z = 0, rot = 0;
    for (const std::string& line : lines) {
        const char* s = line.c_str();
        std::sscanf(s, PY_S, w1);
        if (std::strcmp(w1, "create") != 0) continue;
        std::sscanf(s, "%*s " PY_S, w2);
        if (std::strcmp(w2, "building") != 0) continue;
        std::sscanf(s, "%*s %*s %i %*s %u " PY_S, &type, &sec, w3);
        if (std::strcmp(w3, "subtype") == 0)
            std::sscanf(s, "%*s %*s %*i %*s %*i %*s %i %*s %f %f %f %*s %f %*s %i", &subtype, &x, &y, &z, &rot, &id);
        else if (std::strcmp(w3, "position") == 0)
            std::sscanf(s, "%*s %*s %*i %*s %*i %*s %f %f %f %*s %f", &x, &y, &z, &rot);
        if (type == BUILDING_HQ) {
            CSector* sector = getSector(sec);
            if (sector && sec < 54) {
                hasHQ[sec] = true;
                if (sector->mOwner && sector->mOwner->mId < 5) hqSector[sector->mOwner->mId] = sector;
            }
        }
        if (type == BUILDING_FLAGPOST && sec < 50) flagPos[sec] = CVector2(x, y);
    }

    auto neutraliseSectorsWithoutHQ = [&]() {
        for (auto& e : mSectors) {
            CSector* s = e.second.get();
            if (!(s->mId < 54 && hasHQ[s->mId])) {
                s->mOwner = game->getNeutralPlayer();
                s->ownerChanged();
            }
        }
    };
    auto randomNeutralSector = [&](int callSite) {
        for (;;) {
            CSector* s = nullptr;
            do {
                const float r = CRngSafe::GetUniform(callSite);
                const float v = r * static_cast<float>(mNumSectors) + 1.0f;
                s = getSector(static_cast<unsigned>(f2i(v)));
            } while (!s);
            if (s->mOwner == game->getNeutralPlayer()) return s;
        }
    };
    auto nearestNeutralSector = [&](const CVector2& target) {
        float best = bitsf(0x7f7fffffu);
        CSector* sel = nullptr;
        for (auto& e : mSectors) {
            CSector* s = e.second.get();
            if (s->mOwner != game->getNeutralPlayer()) continue;
            const CVector2& f = s->mId < 50 ? flagPos[s->mId] : CVector2::ZERO;
            const float dx = target.x - f.x;
            const float dy = target.y - f.y;
            const float d = CMath::Sqrt(dx * dx + dy * dy);
            if (d < best) {
                best = d;
                sel = s;
            }
        }
        return sel;
    };

    switch (mode) {
        case 1:
            neutraliseSectorsWithoutHQ();
            break;
        case 2: {
            neutraliseSectorsWithoutHQ();
            const int rounds = static_cast<int>((mNumSectors - game->mNumPlayers) / game->mNumPlayers);
            for (int i = 0; i < rounds; ++i)
                for (unsigned pid = 1; pid <= game->mNumPlayers; ++pid) {
                    CPlayer* p = game->getPlayer(pid);
                    CSector* s = randomNeutralSector(0x10);
                    s->mOwner = p;
                    s->ownerChanged();
                }
            break;
        }
        case 3: {
            neutraliseSectorsWithoutHQ();
            const int n = static_cast<int>(mNumSectors - game->mNumPlayers);
            unsigned pid = 0;
            for (int i = 0; i < n; ++i) {
                ++pid;
                if (pid > game->mNumPlayers) pid = 1;
                CPlayer* p = game->getPlayer(pid);
                CSector* s = randomNeutralSector(0x11);
                CUtils::log("%i", s->mId);
                s->mOwner = p;
                s->ownerChanged();
            }
            break;
        }
        case 4:
        case 5: {
            neutraliseSectorsWithoutHQ();
            const unsigned np = game->mNumPlayers;
            const int rounds = static_cast<int>(mode == 4 ? (mNumSectors - np) / np : mNumSectors / np);
            for (int i = 0; i < rounds; ++i)
                for (unsigned pid = 1; pid <= game->mNumPlayers; ++pid) {
                    CPlayer* p = game->getPlayer(pid);
                    CSector* hq = p && p->mId < 5 ? hqSector[p->mId] : nullptr;
                    const CVector2 target = hq && hq->mId < 50 ? flagPos[hq->mId] : CVector2::ZERO;
                    if (mSectors.empty()) continue;
                    if (CSector* s = nearestNeutralSector(target)) {
                        s->mOwner = p;
                        s->ownerChanged();
                    }
                }
            break;
        }
        default:
            break;
    }
}

// Ghidra 0x2da760. "<name>occ.dat": one "%i" per line, x-major
// (occupancy[y*occW + x], stored as a byte); "<name>.dat": one "%f;%f" per
// line, x-major (potential[y*potW + x]).
void CLevel::initPotential(const std::string& name) {
    mStaticPotential.assign(static_cast<size_t>(mPotW) * static_cast<size_t>(mPotH), CVector2());
    mOccupancy.assign(static_cast<size_t>(mOccW) * static_cast<size_t>(mOccH), 0);
    std::string text;
    if (!CMemoryFile::readAll(name + "occ.dat", text)) {
        CUtils::log("Can't load occupancy map from %socc.dat", name.c_str());
    } else {
        const std::vector<std::string> lines = tokenize(text, "\n");   // strtok(.., "\n")
        size_t li = 0;
        int v = 0;
        for (int x = 0; x < mOccW; ++x)
            for (int y = 0; y < mOccH; ++y) {
                if (li >= lines.size()) break;   // the original reads past the end and crashes
                std::sscanf(lines[li++].c_str(), "%i", &v);
                mOccupancy[static_cast<size_t>(mOccW) * static_cast<size_t>(y) + static_cast<size_t>(x)] =
                    static_cast<uint8_t>(v);
            }
    }
    if (!CMemoryFile::readAll(name + ".dat", text)) {
        CUtils::log("Can't load static potential from %s", name.c_str());
        return;
    }
    const std::vector<std::string> lines = tokenize(text, "\n");
    size_t li = 0;
    for (int x = 0; x < mPotW; ++x)
        for (int y = 0; y < mPotH; ++y) {
            if (li >= lines.size()) break;
            CVector2& p = mStaticPotential[static_cast<size_t>(y) * static_cast<size_t>(mPotW) + static_cast<size_t>(x)];
            std::sscanf(lines[li++].c_str(), "%f;%f", &p.x, &p.y);
        }
}

// Ghidra 0x2daa28 / 0x2dab08: cell (x, y) at index y*W + x.
void CLevel::initUnitLookupGrid() {
    mGrid.assign(static_cast<size_t>(mGridW > 0 && mGridH > 0 ? mGridW * mGridH : 0), CGridCell());
    for (int x = 0; x < mGridW; ++x)
        for (int y = 0; y < mGridH; ++y) {
            mGrid[static_cast<size_t>(mGridW * y + x)].x = x;
            mGrid[static_cast<size_t>(mGridW * y + x)].y = y;
        }
}

void CLevel::initUnitLookupGridFar() {
    mGridFar.assign(static_cast<size_t>(mGridFarW > 0 && mGridFarH > 0 ? mGridFarW * mGridFarH : 0), CGridCell());
    for (int x = 0; x < mGridFarW; ++x)
        for (int y = 0; y < mGridFarH; ++y) {
            mGridFar[static_cast<size_t>(mGridFarW * y + x)].x = x;
            mGridFar[static_cast<size_t>(mGridFarW * y + x)].y = y;
        }
}

// The shaders/textures initByScript loads before reading the file.
void CLevel::loadSharedGraphics() {
    CEngine* e = CEngine::getSingletonPtr();
    const bool fow = e->mUseFOW, res = e->mGameType == 3;
    mSectorShader = shader(fow ? "diffuseplayersector.gfx" : "diffuseplayersector_nofow.gfx");
    if (mSectorShader) {
        if (fow) mSectorShader->setupUniformTextureUnit("MAPFOW", 1);
        mSectorShader->setupUniformTextureUnit("DIFFUSE", 0);
    }
    mSectorTexture = texture("sectors.png");
    mBackground = texture("background.png");
    mSimpleShader = shader("simple.gfx");
    if (mSimpleShader) mSimpleShader->setupUniformTextureUnit("DIFFUSE", 0);
    mMenuMesh = CMeshManager::get().getMesh("menu.3DS.mesh", true, false);
    const std::string suffix = std::string(fow ? "" : "_nofow");
    mSchematicShader = shader("playercolorsector" + suffix + (res ? "_res" : "") + ".gfx");
    mSchematicMMShader = shader("playercolorsector" + suffix + "_mm" + (res ? "_res" : "") + ".gfx");
    if (fow) {
        for (CGLSLProgram* p : {mSchematicShader, mSchematicMMShader})
            if (p) {
                p->setupUniformTextureUnit("MAPFOW", 0);
                if (res) p->setupUniformTextureUnit("MAPRES", 1);
            }
    } else if (res) {
        for (CGLSLProgram* p : {mSchematicShader, mSchematicMMShader})
            if (p) p->setupUniformTextureUnit("MAPRES", 0);
    }
    mSchematicMMNoFowShader = shader("playercolorsector_nofow_mm.gfx");
    mMapMMShader = shader(fow ? "mapb_mm.gfx" : "mapb_nofow_mm.gfx");
    mAssetShader = shader("diffuselight.gfx");
    if (mAssetShader) mAssetShader->setupUniformTextureUnit("DIFFUSE", 0);
    mPointShader = shader("point.gfx");
}

// ------------------------------------------------------------ initByScript

// Ghidra 0x2e1098
bool CLevel::initByScript(const std::string& file, bool fromSavegame) {
    CEngine* e = CEngine::getSingletonPtr();
    CGame* game = e->mGame;
    CConfig& cfg = CConfig::get();
    mMapId = e->mMapId;
    mIsCampaignMap = e->mMapId >= 200;
    loadSharedGraphics();
    for (bool& b : cfg.mUnitAvailable) b = true;
    for (bool& b : cfg.mCrateActionAvailable) b = true;
    cfg.mKISend2Repair = cfg.mKIEnterUnits = cfg.mKIGetsCrates = cfg.mNeutralUnitAutoDamage = true;
    cfg.mAllNonCmdUnitAutoDamage = false;
    cfg.mAllNonCmdUnitAutoDamageValue = 0.0f;

    std::string text;
    if (!CMemoryFile::readAll(file, text)) {
        CUtils::log("Can't load level setup from %s", file.c_str());
        return false;
    }
    CUtils::log("Loading level %s", file.c_str());
    const std::vector<std::string> lines = tokenize(text, "\n");

    // Buffers and values persist between lines, like the original's locals.
    char cmd[kBuf] = {0}, word2[kBuf] = {0}, word[kBuf] = {0}, arg[kBuf] = {0};
    int i0 = 0, i1 = 0, type = 0, sec = 0, idv = 0;
    float x = 0, y = 0, z = 0, rot = 0;
    float px = 0, py = 0, pz = 0, qx = 0, qy = 0, qz = 0, qw = 0, sx = 0, sy = 0, sz = 0;
    bool assetSeen = false;

    for (const std::string& line : lines) {
        const char* s = line.c_str();
        std::sscanf(s, PY_S, cmd);
        if (std::strcmp(cmd, "create") == 0) {
            std::sscanf(s, "%*s " PY_S, word2);
            if (std::strcmp(word2, "sector") == 0) {
                std::sscanf(s, "%*s %*s %i %*s %i", &i0, &i1);
                createSector(static_cast<unsigned>(i0), static_cast<unsigned>(i1));
            } else if (std::strcmp(word2, "building") == 0) {
                std::sscanf(s, "%*s %*s %i %*s %i " PY_S, &type, &sec, word);
                type = getBuildingTypeAlternative(type);
                if (type == -1) continue;
                if (std::strcmp(word, "subtype") == 0) {
                    const int n = std::sscanf(s, "%*s %*s %*i %*s %*i %*s %i %*s %f %f %f %*s %f %*s %i", &i1, &x, &y,
                                              &z, &rot, &idv);
                    CSector* sector = getSector(static_cast<unsigned>(sec));
                    const float rad = CMath::fDeg2Rad * rot;
                    if (sector) {
                        if (n == 5)
                            sector->createBuilding(type, x, y, rad, i1);
                        else
                            sector->createBuilding(static_cast<unsigned>(idv), type, x, y, rad, i1);
                    }
                } else if (std::strcmp(word, "position") == 0) {
                    std::sscanf(s, "%*s %*s %*i %*s %*i %*s %f %f %f %*s %f", &x, &y, &z, &rot);
                    CSector* sector = getSector(static_cast<unsigned>(sec));
                    if (sector) sector->createBuilding(type, x, y, CMath::fDeg2Rad * rot, 0);
                }
                // [!] an HQ in a non-neutral sector drops 4 RoboBrains in front of it
                // (also when the line's 5th word was neither "subtype" nor "position")
                if (type == BUILDING_HQ) {
                    CSector* sector = getSector(static_cast<unsigned>(sec));
                    if (sector && sector->mOwner && !sector->mOwner->isNeutral()) {
                        const float rad = CMath::fDeg2Rad * rot;
                        const float sx0 = CMath::SinTable(rad + CMath::HALF_PI);
                        const float dropX = sx0 * 40.0f + x;
                        const float sy0 = CMath::SinTable(CMath::fDeg2Rad * rot);
                        const float dropY = sy0 * 40.0f + y;
                        char scriptLine[1024];
                        std::snprintf(scriptLine, sizeof(scriptLine),
                                      "script unitdrop delay(1000.0) unittype(1) position(%f;%f) player(%i) count(4)",
                                      static_cast<double>(dropX), static_cast<double>(dropY),
                                      static_cast<int>(sector->mOwner->mId));
                        mScriptManager->addScript(scriptLine);
                    }
                }
            } else if (std::strcmp(word2, "unit") == 0) {
                if (fromSavegame) continue;
                const int n = std::sscanf(s, "%*s %*s %i " PY_S " %i %*s %f %f %f %*s %f %*s %i", &type, arg, &sec, &x,
                                          &y, &z, &rot, &idv);
                CPlayer* owner = nullptr;
                unsigned sectorArg = 0xffffffffu;
                if (std::strcmp(arg, "player") == 0) {
                    owner = game->getPlayer(static_cast<unsigned>(sec));
                } else {
                    CSector* sector = getSector(static_cast<unsigned>(sec));
                    owner = sector ? sector->mOwner : nullptr;
                    sectorArg = static_cast<unsigned>(sec);
                }
                if (!owner) continue;   // the original dereferences null here
                const CVector2 pos(x, y);
                const float rad = CMath::fDeg2Rad * rot;
                CUnitFactory* f = CUnitFactory::getSingletonPtr();
                if (owner->mType == CPlayer::TYPE_NEUTRAL) {
                    if (n == 7)
                        f->createUnit(type, owner->mId, pos, rad, false, sectorArg);
                    else
                        f->createUnit(static_cast<unsigned>(idv), type, owner->mId, pos, rad, false, sectorArg);
                } else {
                    CUnit* u = n == 7 ? f->createUnit(type, owner->mId, pos, rad, true, sectorArg)
                                      : f->createUnit(static_cast<unsigned>(idv), type, owner->mId, pos, rad, true,
                                                      sectorArg);
                    u->setUnitOrderOnWatch();
                }
            } else if (std::strcmp(word2, "asset") == 0) {
                if (!mAssetMesh) mAssetMesh = CMeshManager::get().getMesh("assets_mud.3DS.mesh", true, false);
                if (!mAssetTexture) mAssetTexture = texture("assets_mud.png");
                if (!assetSeen) {
                    if (!fromSavegame) applySpecialSectorSetup(e->mSectorSetupMode, lines);
                    applySpecialUnitAvailability(e->mUnitAvailabilityMode);
                }
                std::sscanf(s, "%*s %*s " PY_S " %*s %f %f %f %*s %f %f %f %f %*s %f %f %f", arg, &px, &py, &pz, &qx,
                            &qy, &qz, &qw, &sx, &sy, &sz);
                CMapAsset a;
                a.name = arg;
                if (mAssetMesh) mAssetMesh->getSubMeshData(a.name, a.firstTri, a.triCount);
                a.position = Vec3(px, py, pz);
                a.scale = Vec3(sx, sy, sz);
                a.rotation = Quat(qw, qx, qy, qz);
                mAssets.push_back(a);
                assetSeen = true;
            }
        } else if (std::strcmp(cmd, "load") == 0) {
            std::sscanf(s, "%*s " PY_S, word);
            const std::string key = word;
            if (key == "assetmesh") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                mAssetMesh = CMeshManager::get().getMesh(arg, true, false);
                mLoaded[key] = arg;
            }
            if (key == "assettexture") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                mAssetTexture = texture(arg);
                mLoaded[key] = arg;
            }
            if (key == "watercubemap") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                mWaterCube = texture(arg, 2);
                mWaterNormal = texture("waternormal.png");
                mWaterShader = shader(e->mUseFOW ? "cube.gfx" : "cube_nofow.gfx");
                if (mWaterShader) {
                    if (e->mUseFOW) mWaterShader->setupUniformTextureUnit("MAPFOW", 2);
                    mWaterShader->setupUniformTextureUnit("CUBE", 0);
                    mWaterShader->setupUniformTextureUnit("NORMALMAP", 1);
                }
                mLoaded[key] = arg;
            }
            if (key == "mapmesh") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                mMapMesh = CMeshManager::get().getMesh(arg, false, true);
                const bool res = e->mGameType == 3;
                mMapShader = shader(std::string(e->mUseFOW ? "mapb" : "mapb_nofow") + (res ? "_res" : "") + ".gfx");
                if (mMapMesh) {
                    mMapMesh->getSubMeshData("map", mMapFirst, mMapCount);
                    mMMName = "mm" + std::to_string(e->mMapId);
                    if (!mMapMesh->hasSubMesh(mMMName)) mMMName = "mm";
                    mMMBorderName = "mmborder" + std::to_string(e->mMapId);
                    if (!mMapMesh->hasSubMesh(mMMBorderName)) mMMBorderName = "mmborder";
                }
                mLoaded[key] = arg;
            }
            if (key == "mapmixtexture") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                if (mMapShader) mMapShader->setupUniformTextureUnit("MIX", 0);
                mMixTexture = texture(arg);
                if (mMapMMShader) mMapMMShader->setupUniformTextureUnit("MIX", 0);
                if (e->mUseFOW) {
                    if (mMapMMShader) mMapMMShader->setupUniformTextureUnit("MAPFOW", 1);
                    if (mMapShader) {
                        mMapShader->setupUniformTextureUnit("MAPFOW", 4);
                        if (e->mGameType == 3) {
                            mMapShader->setupUniformTextureUnit("MAPRES", 5);
                            mMapShader->setupUniformTextureUnit("MAPRESTXT", 6);
                        }
                    }
                } else if (e->mGameType == 3 && mMapShader) {
                    mMapShader->setupUniformTextureUnit("MAPRES", 4);
                    mMapShader->setupUniformTextureUnit("MAPRESTXT", 5);
                }
                mLoaded[key] = arg;
            }
            if (key == "mapdetailtexture") {
                std::sscanf(s, "%*s %*s %i " PY_S, &i0, arg);
                if (i0 >= 1 && i0 <= 3) {
                    static const char* kNames[3] = {"MAP1", "MAP2", "MAP3"};
                    if (mMapShader) mMapShader->setupUniformTextureUnit(kNames[i0 - 1], i0);
                    mDetail[i0 - 1] = texture(arg);
                    mDetailTextureNames[i0 - 1] = arg;
                }
            }
            if (key == "potential") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                initPotential(arg);
                mLoaded[key] = arg;
            }
            if (key == "navbasemesh") {
                std::sscanf(s, "%*s %*s " PY_S, arg);
                mNav = std::make_unique<CNav>();
                mNav->initnav(arg);
                mLoaded[key] = arg;
            }
            // "load navmesh" has no branch in the original: silently ignored.
        } else {
            std::sscanf(s, "%*s " PY_S, word);
            if (std::strcmp(cmd, "set") == 0) {
                const std::string key = word;
                if (key == "hqmode") std::sscanf(s, "%*s %*s %i", &mHQMode);
                if (key == "fowsize") {
                    std::sscanf(s, "%*s %*s %f %f", &mFowW, &mFowH);
                    mFowTexW = f2i(mFowW * 32.0f / 1000.0f);
                    mFowTexH = f2i(mFowH * 32.0f / 1000.0f);
                    mFowScaleX = mFowW / mMapW;
                    mFowScaleY = mFowH / mMapH;
                    // With UseFOW every player (0..numPlayers) creates its FOW and
                    // radar grids (vtable +0x20, +0x24, +0xa0).
                    if (e->mUseFOW) {
                        for (unsigned i = 0; i <= game->mNumPlayers; ++i) {
                            // [!] players 0..numplayers of the map: the original
                            // crashes when the game has fewer players
                            CPlayer* p = game->getPlayer(i);
                            if (!p) continue;
                            p->initFOWTexture();
                            p->updateFOWtexture();
                            p->initRadarMap();
                        }
                    }
                    if (e->mGameType == 3) {
                        // "setup res map": the resource grid has the FOW grid's size.
                        // TODO(resources): the level's vtable +0/+4 calls (grid setup).
                        mResScaleX = mFowScaleX;
                        mResScaleY = mFowScaleY;
                        mResW = mFowTexW;
                        mResH = mFowTexH;
                        mResTexture = texture("map_tx_res.png");
                    }
                }
                if (key == "unitavailability") {
                    std::sscanf(s, "%*s %*s %*s %i %*s %i", &i0, &i1);
                    if (i0 >= 0 && i0 < CConfig::kUnitTypes) cfg.mUnitAvailable[i0] = i1 == 1;
                }
                if (key == "crateactionavailability") {
                    std::sscanf(s, "%*s %*s %*s %i %*s %i", &i0, &i1);
                    if (i0 >= 0 && i0 < CConfig::kCrateActions) cfg.mCrateActionAvailable[i0] = i1 == 1;
                }
                if (key == "cratedrops") {
                    std::sscanf(s, "%*s %*s %i", &mCrateDrops);
                    if (mCrateDrops == 1) {
                        const float a = mMapW / 1000.0f;
                        const float b = mMapH / 1000.0f;
                        const unsigned n = f2u(a * b);
                        char scriptLine[256];
                        std::snprintf(scriptLine, sizeof(scriptLine),
                                      "script cratedrop delay(90000.0) loopable maxcrates(%i) posrandom",
                                      static_cast<int>(n << 1));
                        mScriptManager->addScript(scriptLine);
                    }
                }
                if (key == "start") {
                    std::sscanf(s, "%*s %*s %i %*s %f %f", &i0, &x, &y);
                    if (CPlayer* p = game->getPlayer(static_cast<unsigned>(i0))) p->mStartPosition = CVector2(x, y);
                }
                if (key == "team" && e->mGameType == 1) {
                    std::sscanf(s, "%*s %*s %i %i", &i0, &i1);
                    if (CPlayer* p = game->getPlayer(static_cast<unsigned>(i0))) p->setTeam(static_cast<unsigned>(i1));
                }
                if (key == "kimode" && e->mGameType == 1) {
                    std::sscanf(s, "%*s %*s %i %i", &i0, &i1);
                    CPlayer* p = game->getPlayer(static_cast<unsigned>(i0));
                    if (p && p->mType == CPlayer::TYPE_AI) static_cast<CPlayerKI*>(p)->mKIMode = i1;
                }
                if (key == "kimainmode" && e->mGameType == 1) {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    for (auto& p : game->mPlayers)
                        if (p.second->mType == CPlayer::TYPE_AI) static_cast<CPlayerKI*>(p.second.get())->mKIMode = i0;
                }
                if (key == "revision") std::sscanf(s, "%*s %*s %i", &mRevision);
                if (key == "minimapz") std::sscanf(s, "%*s %*s %f", &mMinimapZ);
                if (key == "minimapxy") std::sscanf(s, "%*s %*s %f %f", &mMinimapX, &mMinimapY);
                if (key == "numplayers") {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    game->mNumPlayers = static_cast<unsigned>(i0);
                }
                if (key == "gameendingcondition") std::sscanf(s, "%*s %*s %i", &game->mGameEndingCondition);
                if (key == "kisend2repair") {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    cfg.mKISend2Repair = i0 == 1;
                }
                if (key == "kienterunits") {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    cfg.mKIEnterUnits = i0 == 1;
                }
                if (key == "kigetscrates") {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    cfg.mKIGetsCrates = i0 == 1;
                }
                if (key == "neutralunitautodamage") {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    cfg.mNeutralUnitAutoDamage = i0 == 1;
                }
                if (key == "allnoncmdunitautodamage") {
                    cfg.mAllNonCmdUnitAutoDamage = true;
                    std::sscanf(s, "%*s %*s %f", &cfg.mAllNonCmdUnitAutoDamageValue);
                }
                if (key == "mapmixbasecolor")
                    std::sscanf(s, "%*s %*s %f %f %f", &mMixBaseColor.x, &mMixBaseColor.y, &mMixBaseColor.z);
                if (key == "wateractive") {
                    std::sscanf(s, "%*s %*s %i", &i0);
                    mWaterActive = i0 == 1;
                    if (mWaterActive && mMapMesh) mMapMesh->getSubMeshData("water", mWaterFirst, mWaterCount);
                }
                if (key == "mapname") mMapName = line.size() > 12 ? line.substr(12) : std::string();
                if (key == "lightcolor")
                    std::sscanf(s, "%*s %*s %f %f %f", &mLightColor.x, &mLightColor.y, &mLightColor.z);
                if (key == "watercolor")
                    std::sscanf(s, "%*s %*s %f %f %f", &mWaterColor.x, &mWaterColor.y, &mWaterColor.z);
                if (key == "lightdirection") {
                    std::sscanf(s, "%*s %*s %f %f %f", &mLightDir.x, &mLightDir.y, &mLightDir.z);
                    // [!] stored negated (sign bit flip), then normalised
                    mLightDir.x = bitsf(fbits(mLightDir.x) ^ 0x80000000u);
                    mLightDir.y = bitsf(fbits(mLightDir.y) ^ 0x80000000u);
                    mLightDir.z = bitsf(fbits(mLightDir.z) ^ 0x80000000u);
                    const float xx = mLightDir.x * mLightDir.x;
                    const float yy = mLightDir.y * mLightDir.y;
                    const float sum = xx + yy;
                    const float zz = mLightDir.z * mLightDir.z;
                    const float len = fdlibm::sqrtf(sum + zz);
                    if (len > bitsf(0x322bcc77u)) {   // 1e-8
                        const float inv = 1.0f / len;
                        mLightDir.x = inv * mLightDir.x;
                        mLightDir.y = inv * mLightDir.y;
                        mLightDir.z = inv * mLightDir.z;
                    }
                }
                if (key == "mapsize") {
                    std::sscanf(s, "%*s %*s %f %f", &mMapW, &mMapH);
                    mHalfW = mMapW * 0.5f;
                    mHalfH = mMapH * 0.5f;
                    mPotW = f2i(mMapW / 2.5f);
                    mPotH = f2i(mMapH / 2.5f);
                    mPotWf = static_cast<float>(mPotW);
                    mPotHf = static_cast<float>(mPotH);
                    mOccHalfH = mPotH / 2;
                    mOccHalfW = mPotW / 2;
                    mOccW = mPotW;
                    mOccH = mPotH;
                    mGridW = f2i(mMapW / 10.0f);
                    mGridWf = static_cast<float>(mGridW);
                    mGridHalfW = f2i(mGridWf * 0.5f);
                    mGridFarW = f2i(mMapW / 100.0f);
                    mGridFarWf = static_cast<float>(mGridFarW);
                    mGridFarHalfW = mGridFarWf * 0.5f;
                    mGridH = f2i(mMapH / 10.0f);
                    mGridHf = static_cast<float>(mGridH);
                    mGridHalfH = f2i(mGridHf * 0.5f);
                    mGridFarH = f2i(mMapH / 100.0f);
                    mGridFarHf = static_cast<float>(mGridFarH);
                    mGridFarHalfH = mGridFarHf * 0.5f;
                    initUnitLookupGrid();
                    initUnitLookupGridFar();
                    mSizeScale = mMapW < 1200.0f ? 1.5f : mMapW < 2200.0f ? 2.5f : mMapW < 3200.0f ? 3.0f : 4.0f;
                }
            } else if (std::strcmp(cmd, "script") == 0) {
                mScriptManager->addScript(line);
            } else {
                CUtils::log("CLevel::initByScript: Unknown command %s", cmd);
            }
        }
    }

    if (e->mGameType == 3 && !fromSavegame) {
        // TODO(phase 4): generateResources() - needs the navmesh (CNav::isOnNavMesh)
        CUtils::log("CLevel: resources mode needs generateResources (phase 4)");
    }
    CUtils::log("Level \"%s\": %zu assets, %u sectors, %zu scripts", mMapName.c_str(), mAssets.size(), mNumSectors,
                mScriptManager->scripts().size());
    return true;
}

// ------------------------------------------------------------------ drawing

void CLevel::draw(const CCameraLevel& cam, float time) const {
    if (cam.isStrategyView() && mMenuMesh && mSimpleShader && mBackground) {
        mMenuMesh->bindBuffer();
        mBackground->bind(GL_TEXTURE0_);
        mSimpleShader->startDraw();
        gl::Disable(GL_DEPTH_TEST_);
        gl::DepthMask(0);
        gl::Disable(GL_CULL_FACE_);
        const Mat4 model = Mat4::transform({0, 0, 0}, Quat(), {4, 4, 4});
        mSimpleShader->setUniformMVPMatrix(cam.viewProj() * model);
        uint32_t f = 0, c = 0;
        if (mMenuMesh->getSubMeshData("background", f, c)) mMenuMesh->draw(f, c);
        mSimpleShader->endDraw();
        gl::Enable(GL_DEPTH_TEST_);
        gl::Enable(GL_CULL_FACE_);
        gl::DepthMask(1);
    }
    drawSectors(cam);
    if (!cam.isStrategyView()) drawAssets(cam);
    drawMap(cam, time);
}

void CLevel::drawSectors(const CCameraLevel& cam) const {
    // startSectorDraw / CSector::draw / endSectorDraw
    if (!mMapMesh) return;
    mMapMesh->bindBuffer();
    auto colorOf = [](const CSector* s) { return s->mOwner ? s->mOwner->mColor : Vec3(0.7f, 0.7f, 0.7f); };
    if (cam.isStrategyView()) {
        if (!mSchematicShader) return;
        mSchematicShader->startDraw();
        mSchematicShader->setUniformMVPMatrix(cam.viewProj());
        for (const auto& e : mSectors) {
            mSchematicShader->setUniformPlayerColor(colorOf(e.second.get()) * 0.25f + kGray25);
            mMapMesh->draw(e.second->mSchematicFirst, e.second->mSchematicCount);
        }
        mSchematicShader->endDraw();
        return;
    }
    if (!mSectorShader || !mSectorTexture) return;
    mSectorTexture->bind(GL_TEXTURE0_);
    mSectorShader->startDraw();
    mSectorShader->setUniformMVPMatrix(cam.viewProj());
    for (const auto& e : mSectors) {
        mSectorShader->setUniformPlayerColor(colorOf(e.second.get()));
        mMapMesh->draw(e.second->mBorderFirst, e.second->mBorderCount);
    }
    mSectorShader->endDraw();
}

void CLevel::drawAssets(const CCameraLevel& cam) const {
    if (!mAssetMesh || !mAssetShader || !mAssetTexture) return;
    mAssetMesh->bindBuffer();
    mAssetShader->startDraw();
    mAssetTexture->bind(GL_TEXTURE0_);
    for (const auto& a : mAssets) {
        // (the original also culls against the frustum planes with radius ~100)
        const Mat4 model = Mat4::transform(a.position, a.rotation, a.scale);
        mAssetShader->setUniformMVPMatrix(cam.viewProj() * model);
        mAssetShader->setUniformLightColor(mLightColor);   // x FOW visibility in the original
        mAssetShader->setUniformLightDir(a.rotation.inverse().rotate(mLightDir));
        mAssetMesh->draw(a.firstTri, a.triCount);
    }
    mAssetShader->endDraw();
}

void CLevel::drawMap(const CCameraLevel& cam, float time) const {
    if (!mMapMesh) return;
    if (!cam.isStrategyView()) {
        if (mMapShader && mMixTexture) {
            mMapMesh->bindBuffer();
            mMapShader->startDraw();
            mMixTexture->bind(GL_TEXTURE0_);
            for (int i = 0; i < 3; ++i)
                if (mDetail[i]) mDetail[i]->bind(GL_TEXTURE0_ + 1 + static_cast<unsigned>(i));
            mMapShader->setUniformMVPMatrix(cam.viewProj());
            mMapShader->setUniformPlayerColor(mMixBaseColor);
            mMapMesh->draw(mMapFirst, mMapCount);
            mMapShader->endDraw();
        }
        // water (only if "wateractive" and the water option is on)
        if (mWaterActive && mWaterShader && mWaterCube && mWaterNormal && mWaterCount) {
            mWaterShader->startDraw();
            mWaterCube->bind(GL_TEXTURE0_);
            mWaterNormal->bind(GL_TEXTURE0_ + 1);
            const Mat4 model = Mat4::identity();
            mWaterShader->setUniformMVPMatrix(cam.viewProj() * model);
            mWaterShader->setUniformMMatrix(model);
            mWaterShader->setUniformEyePosition(cam.eye());   // eye in object space (identity model)
            mWaterShader->setUniformPlayerColor(mWaterColor);
            mWaterShader->setUniformTime(time);
            gl::Enable(GL_BLEND_);
            gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
            mMapMesh->draw(mWaterFirst, mWaterCount);
            gl::Disable(GL_BLEND_);
            mWaterShader->endDraw();
        }
        return;
    }
    // strategy view: "mm<id>" in gray25, then "mmborder<id>" in gray75 (blended), 20 units down
    if (!mSchematicShader) return;
    mMapMesh->bindBuffer();
    mSchematicShader->startDraw();
    const Mat4 model = Mat4::transform({0, 0, -20.0f}, Quat(), {1, 1, 1});
    mSchematicShader->setUniformMVPMatrix(cam.viewProj() * model);
    uint32_t f = 0, c = 0;
    mMapMesh->getSubMeshData(mMMName, f, c);
    mSchematicShader->setUniformPlayerColor(kGray25);
    mMapMesh->draw(f, c);
    gl::Enable(GL_BLEND_);
    gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    mMapMesh->getSubMeshData(mMMBorderName, f, c);
    mSchematicShader->setUniformPlayerColor(kGray75);
    mMapMesh->draw(f, c);
    gl::Disable(GL_BLEND_);
    mSchematicShader->endDraw();
}

// ------------------------------------------------------------------ unit grids

// Ghidra 0x2dc830. [!] Original bug, kept: when the position is outside the
// grid, the clamped cell index uses the *far* grid's width (+0x134) as the
// row stride, so such units land in a wrong (but valid) near cell.
CGridCell* CLevel::getGridCell(const CVector2& p) {
    int cx = f2i(p.x / Real(mMapW) * Real(mGridWf) + i2f(mGridHalfW));
    int cy = f2i(p.y / Real(mMapH) * Real(mGridHf) + i2f(mGridHalfH));
    int stride = mGridW;
    if (cx >= mGridW || cy >= mGridH || (cx | cy) < 0) {
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= mGridW) cx = mGridW - 1;
        stride = mGridFarW;
        if (cy >= mGridH) cy = mGridH - 1;
    }
    const size_t i = static_cast<size_t>(stride * cy + cx);
    return i < mGrid.size() ? &mGrid[i] : nullptr;
}

// Ghidra 0x2dc718
CGridCell* CLevel::getGridCellFar(const CVector2& p) {
    int cx = f2i(p.x / Real(mMapW) * Real(mGridFarWf) + Real(mGridFarHalfW));
    int cy = f2i(p.y / Real(mMapH) * Real(mGridFarHf) + Real(mGridFarHalfH));
    if (cx >= mGridFarW || cy >= mGridFarH || (cx | cy) < 0) {
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= mGridFarW) cx = mGridFarW - 1;
        if (cy >= mGridFarH) cy = mGridFarH - 1;
    }
    const size_t i = static_cast<size_t>(mGridFarW * cy + cx);
    return i < mGridFar.size() ? &mGridFar[i] : nullptr;
}

namespace {
// The 3x3 block around a cell, clamped to the grid; x outer, y inner.
template <class F>
void forNeighbours(const CGridCell* c, int w, int h, F f) {
    const int x0 = c->x < 1 ? 0 : c->x - 1;
    const int x1 = c->x < w - 1 ? c->x + 1 : w - 1;
    const int y0 = c->y < 1 ? 0 : c->y - 1;
    const int y1 = c->y < h - 1 ? c->y + 1 : h - 1;
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y) f(static_cast<size_t>(w * y + x));
}

void addId(std::vector<unsigned>& v, unsigned id) {
    if (std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
}

void removeId(std::vector<unsigned>& v, unsigned id) { v.erase(std::remove(v.begin(), v.end(), id), v.end()); }
}  // namespace

void CLevel::addUnitToCells(CGridCell* cell, unsigned id) {
    forNeighbours(cell, mGridW, mGridH, [&](size_t i) { addId(mGrid[i].units, id); });
}

CGridCell* CLevel::addUnitToCells(const CVector3& p, unsigned id) {
    CGridCell* c = getGridCell(CVector2(p.x, p.y));
    if (c) addUnitToCells(c, id);
    return c;
}

void CLevel::addUnitToCellsFar(CGridCell* cell, unsigned id) {
    forNeighbours(cell, mGridFarW, mGridFarH, [&](size_t i) { addId(mGridFar[i].units, id); });
}

CGridCell* CLevel::addUnitToCellsFar(const CVector3& p, unsigned id) {
    CGridCell* c = getGridCellFar(CVector2(p.x, p.y));
    if (c) addUnitToCellsFar(c, id);
    return c;
}

void CLevel::removeUnitFromCells(CGridCell* cell, unsigned id) {
    forNeighbours(cell, mGridW, mGridH, [&](size_t i) { removeId(mGrid[i].units, id); });
}

void CLevel::removeUnitFromCellsFar(CGridCell* cell, unsigned id) {
    forNeighbours(cell, mGridFarW, mGridFarH, [&](size_t i) { removeId(mGridFar[i].units, id); });
}

CGridCell* CLevel::updateGridCell(const CVector3& p, unsigned id, CGridCell* old) {
    CGridCell* c = getGridCell(CVector2(p.x, p.y));
    if (c != old) {
        removeUnitFromCells(old, id);
        if (c) addUnitToCells(c, id);
    }
    return c;
}

CGridCell* CLevel::updateGridCellFar(const CVector3& p, unsigned id, CGridCell* old) {
    CGridCell* c = getGridCellFar(CVector2(p.x, p.y));
    if (c != old) {
        removeUnitFromCellsFar(old, id);
        if (c) addUnitToCellsFar(c, id);
    }
    return c;
}

// ------------------------------------------------------------------ simulation

// Ghidra 0x2df380
void CLevel::update() {
    mScriptManager->update();
    if (CEngine::getSingletonPtr()->mGameType == 3) {
        // TODO(phase 4, resource mode): resource respawn (every 1 s, until the
        // total is 5000: CRngSafe positions, 4 navmesh probes, 70 m from buildings).
        static bool warned = false;
        if (!warned) CUtils::log("CLevel::update: resource respawn not ported yet");
        warned = true;
    }
}

// Ghidra 0x2dd074
void CLevel::calcChecksum() {
    for (auto& s : mSectors) s.second->calcChecksum();
    // TODO(phase 4, resource mode): + every resource cell value when GameType == 3
}

}  // namespace py
