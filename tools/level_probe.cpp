// level_probe - dumps what the port makes of a map script or of script lines.
//
//   level_probe --data data --map 301 [--type 1] [--setup 0] [--avail 0]
//       loads map301English.txt headless (no GL) and prints every sector,
//       game object, asset, script and level setting
//   level_probe --scripts lines.txt
//       feeds each line ("script <type> <params>") to a fresh CScriptManager
//       and prints the resulting scripts (compared against the original
//       engine by tools/emu/check_scripts.py)
//
// Floats are printed as bit patterns (%08x) so comparisons are exact.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CBuilding.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CScript.h"
#include "game/CScriptManager.h"
#include "game/CSector.h"
#include "game/CUnit.h"
#include "render/CMesh.h"

using namespace py;

namespace {

unsigned fb(float f) { return fbits(f); }

void dumpScript(const CScript* s) {
    std::printf("SCRIPT id=%d type=%d ready=%d did=%d fin=%d delay=%08x runat=%08x loop=%d fixed=%d req=", s->mId,
                s->mType, s->mReadyToRun, s->mDidRun, s->mFinished, fb(s->mDelay), fb(s->mRunAt), s->mLoopable,
                s->mFixedSpeed);
    for (size_t i = 0; i < s->mReqIds.size(); ++i) std::printf("%s%d", i ? ";" : "", s->mReqIds[i]);
    std::printf(" xor=");
    for (size_t i = 0; i < s->mXorIds.size(); ++i) std::printf("%s%d", i ? ";" : "", s->mXorIds[i]);
    std::printf("\n");
    for (const auto& kv : s->params()) std::printf("  P %s=%s|\n", kv.first.c_str(), kv.second.c_str());
    auto I = [](const char* n, int v) { std::printf("  F %s=%d\n", n, v); };
    auto B = [](const char* n, bool v) { std::printf("  F %s=%d\n", n, v ? 1 : 0); };
    auto Fl = [](const char* n, float v) { std::printf("  F %s=%08x\n", n, fbits(v)); };
    auto S = [](const char* n, const std::string& v) { std::printf("  F %s=%s|\n", n, v.c_str()); };
    auto V = [&](const char* n, const CVector2& v) {
        std::string a = std::string(n) + ".x", b = std::string(n) + ".y";
        Fl(a.c_str(), v.x);
        Fl(b.c_str(), v.y);
    };
    switch (s->mType) {
        case SCRIPT_AIRDROP: {
            auto* t = static_cast<const CScriptAirdrop*>(s);
            I("unittype", t->mUnitType); I("player", t->mPlayer); I("count", t->mCount);
            V("position", t->mPosition); B("scrollto", t->mScrollTo); I("unitid", t->mUnitId);
            break;
        }
        case SCRIPT_CRATEDROP: {
            auto* t = static_cast<const CScriptCratedrop*>(s);
            I("maxcrates", t->mMaxCrates); B("posrandom", t->mPosRandom); V("position", t->mPosition);
            break;
        }
        case SCRIPT_AREATRIGGER: {
            auto* t = static_cast<const CScriptAreaTrigger*>(s);
            V("center", t->mCenter); Fl("radius", t->mRadius); I("player", t->mPlayer); I("unittype", t->mUnitType);
            break;
        }
        case SCRIPT_SPEAKER: {
            auto* t = static_cast<const CScriptSpeaker*>(s);
            S("text", t->mText); I("player", t->mPlayer); B("hasscroll", t->mHasScrollTo);
            V("scrollto", t->mScrollTo); Fl("showtime", t->mShowTime); B("nopause", t->mNoPause);
            I("iconplayer", t->mIconPlayer); S("iconname", t->mIconName); S("cmdtext", t->mCmdText);
            break;
        }
        case SCRIPT_FLAGTRIGGER: {
            auto* t = static_cast<const CScriptSectorFlagTrigger*>(s);
            I("playernew", t->mPlayerNew); I("playerold", t->mPlayerOld); I("type", t->mTriggerType);
            I("sector", t->mSectorId);
            break;
        }
        case SCRIPT_SELECTTRIGGER: {
            auto* t = static_cast<const CScriptSelectTrigger*>(s);
            I("unitid", t->mUnitId); I("player", t->mPlayer);
            break;
        }
        case SCRIPT_UNITTRIGGER: {
            auto* t = static_cast<const CScriptUnitTrigger*>(s);
            I("unitid", t->mUnitId); I("player", t->mPlayer); I("type", t->mTriggerType);
            B("wasactive", t->mUnitWasActiveBefore);
            break;
        }
        case SCRIPT_GAMESTATE: {
            auto* t = static_cast<const CScriptGamestate*>(s);
            I("result", t->mResult); I("player", t->mPlayer); B("dontend", t->mDontEnd);
            break;
        }
        case SCRIPT_BEACON: {
            auto* t = static_cast<const CScriptBeacon*>(s);
            I("player", t->mPlayer); V("position", t->mPosition); I("unitid", t->mUnitId); B("scrollto", t->mScrollTo);
            break;
        }
        case SCRIPT_AIRSTRIKE: {
            auto* t = static_cast<const CScriptAirstrike*>(s);
            I("level", t->mLevel); I("player", t->mPlayer); B("scrollto", t->mScrollTo); V("position", t->mPosition);
            break;
        }
        case SCRIPT_FEATURE: {
            auto* t = static_cast<const CScriptFeature*>(s);
            I("feature", t->mFeature); B("disable", t->mDisable); I("param1", t->mParam1); I("param2", t->mParam2);
            break;
        }
        case SCRIPT_BLOCK: {
            auto* t = static_cast<const CScriptBlock*>(s);
            V("center", t->mCenter); Fl("radius", t->mRadius); B("disable", t->mDisable); I("blockid", t->mBlockId);
            break;
        }
        case SCRIPT_BUILDINGTRIGGER: {
            auto* t = static_cast<const CScriptBuildingTrigger*>(s);
            I("buildingid", t->mBuildingId); I("type", t->mTriggerType); I("occ0", t->mOccupantsAtStart);
            I("prod0", t->mProductionAtStart);
            break;
        }
        case SCRIPT_MARKER: {
            auto* t = static_cast<const CScriptMarker*>(s);
            I("player", t->mPlayer); B("disable", t->mDisable); I("type", t->mMarkerType); I("objid", t->mObjId);
            B("scrollto", t->mScrollTo);
            break;
        }
        case SCRIPT_MOD: {
            auto* t = static_cast<const CScriptMod*>(s);
            I("objid", t->mObjId); I("type", t->mModType); Fl("param1", t->mParam1); Fl("param2", t->mParam2);
            break;
        }
        case SCRIPT_TIMETRIGGER: {
            auto* t = static_cast<const CScriptTimeTrigger*>(s);
            Fl("start", t->mStartTime); Fl("time", t->mTime); S("text", t->mText); B("show", t->mShow);
            I("player", t->mPlayer);
            break;
        }
        case SCRIPT_AIOPTION: {
            auto* t = static_cast<const CScriptAIOption*>(s);
            I("player", t->mPlayer); I("type", t->mOptionType); Fl("param1", t->mParam1);
            if (!t->getParamString("param2").empty() || !t->getParamBool("param2")) Fl("param2", t->mParam2);
            break;
        }
        case SCRIPT_ADDACH: {
            auto* t = static_cast<const CScriptAddAch*>(s);
            I("player", t->mPlayer); I("type", t->mAchType);
            break;
        }
        case SCRIPT_COUNTTRIGGER: {
            auto* t = static_cast<const CScriptCountTrigger*>(s);
            B("show", t->mShow); I("player", t->mPlayer); I("count", t->mCount); I("type", t->mObjType);
            I("subtype", t->mSubType);
            break;
        }
        default:
            break;
    }
}

unsigned mapPlayerCount(int id) {
    std::string text;
    if (!CMemoryFile::readAll("map" + std::to_string(id) + "English.txt", text)) return 2;
    const size_t p = text.find("set numplayers");
    if (p == std::string::npos) return 2;
    const int n = std::atoi(text.c_str() + p + 14);
    return n >= 1 && n <= 4 ? static_cast<unsigned>(n) : 2u;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data", scriptsFile;
    int map = 1, type = -1, setup = 0, avail = 0;
    unsigned seed = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data") data = next();
        else if (a == "--map") map = std::atoi(next().c_str());
        else if (a == "--type") type = std::atoi(next().c_str());
        else if (a == "--setup") setup = std::atoi(next().c_str());
        else if (a == "--avail") avail = std::atoi(next().c_str());
        else if (a == "--seed") seed = static_cast<unsigned>(std::strtoul(next().c_str(), nullptr, 0));
        else if (a == "--scripts") scriptsFile = next();
    }
    CUtils::setQuiet(true);
    CUtils::setDataDir(data);
    CMath math(4096);
    CMeshManager::setHeadless(true);
    CEngine* e = CEngine::getSingletonPtr();

    if (!scriptsFile.empty()) {
        // "#RESET" starts a fresh script manager (one per map)
        auto sm = std::make_unique<CScriptManager>(e);
        auto flush = [&]() {
            for (const auto& s : sm->scripts()) dumpScript(s.second.get());
        };
        std::ifstream in(scriptsFile, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            if (line == "#RESET") {
                flush();
                sm.reset();
                sm = std::make_unique<CScriptManager>(e);
                continue;
            }
            sm->addScript(line);
        }
        flush();
        return 0;
    }

    CConfig::get().load(CUtils::dataPath("config.txt"));
    e->setupSkirmish(static_cast<unsigned>(map), mapPlayerCount(map));
    e->mGameType = type >= 0 ? type : (map >= 200 ? 1 : 0);
    e->mSectorSetupMode = setup;
    e->mUnitAvailabilityMode = avail;
    if (seed) CRngSafe::SetSeed(seed);
    auto game = std::make_unique<CGame>();
    e->mGame = game.get();
    if (!game->init(false, true)) {
        std::printf("LOAD FAILED\n");
        return 1;
    }
    CLevel* L = game->getLevel();
    std::printf("LEVEL name=%s| size=%08x,%08x players=%u ending=%d hqmode=%d revision=%d cratedrops=%d campaign=%d\n",
                L->mapName().c_str(), fb(L->mMapW), fb(L->mMapH), game->mNumPlayers, game->mGameEndingCondition,
                L->mHQMode, L->mRevision, L->mCrateDrops, L->isCampaignMap());
    std::printf("LIGHT color=%08x,%08x,%08x dir=%08x,%08x,%08x\n", fb(L->mLightColor.x), fb(L->mLightColor.y),
                fb(L->mLightColor.z), fb(L->mLightDir.x), fb(L->mLightDir.y), fb(L->mLightDir.z));
    std::printf("GRID pot=%dx%d occ=%dx%d grid=%dx%d far=%dx%d fow=%dx%d scale=%08x\n", L->mPotW, L->mPotH,
                L->mOccW, L->mOccH, L->mGridW, L->mGridH, L->mGridFarW, L->mGridFarH, L->mFowTexW, L->mFowTexH,
                fb(L->mSizeScale));
    for (const auto& p : game->players())
        std::printf("PLAYER %u type=%d team=%u start=%08x,%08x\n", p.first, p.second->mType, p.second->mTeam,
                    fb(p.second->mStartPosition.x), fb(p.second->mStartPosition.y));
    std::printf("AVAIL ");
    for (bool b : CConfig::get().mUnitAvailable) std::printf("%d", b ? 1 : 0);
    std::printf("\n");
    for (const auto& s : L->sectors())
        std::printf("SECTOR %u owner=%d hq=%d buildings=%zu border=%u+%u schematic=%u+%u\n", s.first,
                    s.second->mOwner ? static_cast<int>(s.second->mOwner->mId) : -1, s.second->mHasHQ,
                    s.second->mBuildings.size(), s.second->mBorderFirst, s.second->mBorderCount,
                    s.second->mSchematicFirst, s.second->mSchematicCount);
    for (unsigned id : game->objectList()) {
        CGameObject* o = game->getGameObject(id);
        std::printf("OBJ %u cat=%d type=%d owner=%d pos=%08x,%08x rot=%08x", id, o->mCategory, o->mType,
                    o->mOwner ? static_cast<int>(o->mOwner->mId) : -1, fb(o->mPosX), fb(o->mPosY), fb(o->mRotation));
        if (o->mCategory == CGameObject::CATEGORY_UNIT) {
            auto* u = static_cast<CUnit*>(o);
            std::printf(" watch=%d vehicle=%d driver=%d", u->mOnWatch, static_cast<int>(u->mVehicleId),
                        static_cast<int>(u->mDriverId));
        } else {
            auto* b = static_cast<CBuilding*>(o);
            std::printf(" sector=%d subtype=%d", b->mSector ? static_cast<int>(b->mSector->mId) : -1, b->mSubType);
        }
        std::printf("\n");
    }
    for (const auto& a : L->assets())
        std::printf("ASSET %s tri=%u+%u pos=%08x,%08x,%08x quat=%08x,%08x,%08x,%08x scale=%08x,%08x,%08x\n",
                    a.name.c_str(), a.firstTri, a.triCount, fb(a.position.x), fb(a.position.y), fb(a.position.z),
                    fb(a.rotation.w), fb(a.rotation.x), fb(a.rotation.y), fb(a.rotation.z), fb(a.scale.x),
                    fb(a.scale.y), fb(a.scale.z));
    for (const auto& s : L->getScriptManager()->scripts()) dumpScript(s.second.get());
    e->mGame = nullptr;
    return 0;
}
