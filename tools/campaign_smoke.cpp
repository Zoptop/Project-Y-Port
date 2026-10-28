// campaign_smoke - starts every campaign mission (and the tutorial) the way
// the menus do and runs it headless, advancing the speaker texts like a
// player who reads and clicks each one away.
//
//   campaign_smoke [--data data] [--steps 3000] [--maps 301,305|all] [--save-dir dir]
//                  [--dt 0.1] [--force-triggers] [--verbose]
//
// Per mission: the campaign is read (CCampaign::startInitAsync), the
// episode made current, then CInterfaceMenu::processAction(0x52) (the
// progress screen's "replay": the campaign player setup) or 0xe (the
// tutorial), and initSingleplayerGame (what draw mode 2 calls). Each frame
// is one CEngine frame of `dt` seconds without drawing (the ingame
// interface's timeUpdate steps the game once). The overlay text: a text is
// "typed out" after 0.01 s per character (drawOverlayText's 100 characters
// per second), then a modal one (showtime 0) is clicked away
// (IPP::processTouch), as the tests' dismissOverlay.
//
// Output, one line per mission:
//   map <id> <name>: loaded <0|1> steps <n> speakers <k> scripts <done>/<total>
//   result <won|lost|running> aftermath <0|1> unlocked <c>.<e>
//   waiting <ids of active triggers>
// The exit code is non-zero if a mission did not load.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CRng.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CPlayer.h"
#include "game/CScript.h"
#include "game/CScriptManager.h"
#include "render/CCameraMenu.h"
#include "render/CMesh.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

struct Result {
    bool loaded = false;
    int steps = 0;
    int speakers = 0;
    int done = 0, total = 0;
    std::string result = "running";
    bool aftermath = false;
    unsigned unlockedC = 0, unlockedE = 0;
    std::string waiting;
    int forced = 0;
};

bool isTrigger(int type) {
    switch (type) {
        case SCRIPT_AREATRIGGER:
        case SCRIPT_FLAGTRIGGER:
        case SCRIPT_SELECTTRIGGER:
        case SCRIPT_UNITTRIGGER:
        case SCRIPT_BUILDINGTRIGGER:
        case SCRIPT_TIMETRIGGER:
        case SCRIPT_COUNTTRIGGER:
            return true;
        default:
            return false;
    }
}

// the local player's result of a gamestate script: 1 won, -1 lost, 0 none (dontend)
int gamestateResult(const CScript* s) {
    const auto* g = dynamic_cast<const CScriptGamestate*>(s);
    if (!g || g->mDontEnd) return 0;
    return (g->mPlayer == 1) == (g->mResult == 1) ? 1 : -1;
}

// The triggers to force (see --force-triggers). A trigger is a defeat
// trigger if a defeat gamestate follows it without any other trigger in
// between (reqid chains of speakers, drops, ...). The others are forced when a
// victory follows them through scripts that are not defeat triggers.
std::set<int> triggersToForce(CScriptManager* sm, std::set<int>& defeat) {
    std::map<int, std::vector<int>> children;
    for (const auto& kv : sm->scripts())
        for (int r : kv.second->mReqIds) children[r].push_back(kv.first);
    auto isTrig = [&](int id) {
        CScript* s = sm->getScript(id);
        return s && isTrigger(s->type());
    };
    // what follows `from`, entering triggers only if `enter` allows it
    auto reaches = [&](int from, int result, const std::function<bool(int)>& enter) {
        std::set<int> seen;
        std::vector<int> todo{from};
        while (!todo.empty()) {
            const int id = todo.back();
            todo.pop_back();
            if (!seen.insert(id).second) continue;
            if (id != from && isTrig(id) && !enter(id)) continue;
            if (CScript* s = sm->getScript(id))
                if (gamestateResult(s) == result) return true;
            for (int c : children[id]) todo.push_back(c);
        }
        return false;
    };
    defeat.clear();
    for (const auto& kv : sm->scripts())
        if (isTrigger(kv.second->type()) && reaches(kv.first, -1, [](int) { return false; })) defeat.insert(kv.first);
    std::set<int> out;
    for (const auto& kv : sm->scripts()) {
        if (!isTrigger(kv.second->type()) || defeat.count(kv.first)) continue;
        if (reaches(kv.first, 1, [&](int id) { return defeat.count(id) == 0; })) out.insert(kv.first);
    }
    return out;
}

Result runMission(unsigned mapId, int steps, float dt, const std::string& saveDir, bool verbose, bool force) {
    Result r;
    CEngine::release();
    CEngine* e = CEngine::getSingletonPtr();
    e->mSaveDir = saveDir;
    e->mLanguage = "English";
    // a bare menu (no meshes, no text render) for the aftermath screen
    auto menu = std::make_unique<CInterfaceMenu>();
    menu->mCamera = std::make_unique<CCameraMenu>();
    menu->setResolution(1280, 720);
    e->mMenuInterface = std::move(menu);
    CInterfaceMenu* m = e->mMenuInterface.get();
    e->mCurrentInterface = m;
    CCampaign* cmp = e->initCampaign(1);
    if (mapId >= 300) {
        cmp->startInitAsync(false);
        CCampaignMapInfo* info = cmp->getMapInfo(mapId);
        if (!info) {
            std::printf("map %u: not in the campaign\n", mapId);
            return r;
        }
        e->mGameType = 1;   // the campaign screen (setupElements 25)
        cmp->mCurChapter = info->chapter;
        cmp->mCurEpisode = info->episode;
        m->mCurrentMenu = m->mNextMenu = MENU_CAMPAIGNPROGRESS;
        m->processAction(0x52, nullptr);
    } else {
        e->mGameType = 0;   // the single player screen (action 1)
        m->mCurrentMenu = m->mNextMenu = MENU_TUTORIAL;
        m->processAction(0xe, nullptr);
    }
    if (m->mDrawMode != 1) {
        std::printf("map %u: the start action did not start the game\n", mapId);
        return r;
    }
    m->mDrawMode = 0;
    m->initSingleplayerGame();   // draw mode 2
    // reproducible runs (initSingleplayerGame seeded both from the clock)
    CRng::SetSeed(0x5620, 0x5620f340);
    CRngSafe::SetSeed(0x1234, 0x5678);
    auto* ig = dynamic_cast<CInterfaceIngame*>(e->mIngameInterface.get());
    if (!e->mGame || !ig || !ig->ipp(0) || !e->mGame->getLevel() || !e->mGame->getLevel()->getScriptManager()) return r;
    r.loaded = true;
    CInterfaceIngamePerPlayer* ipp = ig->ipp(0);
    CScriptManager* sm = e->mGame->getLevel()->getScriptManager();
    std::set<int> forceSet, defeatSet;
    if (force) {
        forceSet = triggersToForce(sm, defeatSet);
        // the player never lets the defeat conditions happen
        for (int id : defeatSet)
            if (CScript* s = sm->getScript(id)) s->mFinished = true;
    }
    float lastForce = -1.0f;
    float now = 0.0f;
    e->mLastFrameTime = 0.0f;
    int lastSpeakerSeen = -1;
    for (int i = 0; i < steps; ++i) {
        if (e->mCurrentInterface != ig) break;   // the aftermath screen (or the menu)
        if (ipp->mOverlayDisplayed) {
            if (ipp->mOverlayCallerScript != lastSpeakerSeen && ipp->mOverlayCallerScript != -1) {
                lastSpeakerSeen = ipp->mOverlayCallerScript;
                ++r.speakers;
                if (verbose)
                    std::printf("  step %d speaker %d: %.60s\n", i, ipp->mOverlayCallerScript,
                                ipp->mOverlayText.c_str());
            }
            // drawOverlayText's typewriter (100 characters per second, the
            // text and the line under it)
            const float chars = ipp->mOverlayRuntime * 100.0f;
            if (chars >= static_cast<float>(ipp->mOverlayText.size() + 40)) ipp->mOverlayFullyDisplayed = true;
            if (ipp->mOverlayFullyDisplayed && ipp->mOverlayWaitsForTouch)
                ipp->processTouch(CVector2(640.0f, 360.0f), false, ~0u);
        }
        // one trigger at a time, a script tick (0.21 s) apart, never while a
        // text that still has to finish is shown: [!] two speakers started in
        // the same tick share the one overlay, and the first never completes
        bool textPending = ipp->mOverlayDisplayed && ipp->mOverlayTTL != -1.0f;
        for (const auto& kv : sm->scripts()) {   // a speaker that is active or about to be
            const CScript* s = kv.second.get();
            if (s->type() != SCRIPT_SPEAKER || s->mDidRun || s->mFinished) continue;
            bool ready = true;
            for (int r : s->mReqIds) {
                const CScript* q = sm->getScript(r);
                if (!q || !q->mDidRun) ready = false;
            }
            for (int x : s->mXorIds) {
                const CScript* q = sm->getScript(x);
                if (q && q->mDidRun) ready = false;
            }
            if (ready) textPending = true;
        }
        if (force && !textPending && !(e->mGameTime < lastForce + 0.3f)) {
            for (const auto& kv : sm->scripts()) {
                CScript* s = kv.second.get();
                if (!forceSet.count(kv.first)) continue;
                if (!s->mReadyToRun || s->mDidRun || s->mFinished || !(s->mRunAt < e->mGameTime)) continue;
                s->CScript::run();
                s->mDidRun = true;
                ++r.forced;
                if (verbose) std::printf("  step %d forced %d:%s\n", i, kv.first, sm->getScriptName(s->type()).c_str());
                lastForce = e->mGameTime;
                break;
            }
        }
        now += dt * 1000.0f;
        e->updateFrameTime(now, e->mCurrentInterface->getType() == CInterface::TYPE_INGAME);
        e->mCurrentInterface->timeUpdate(e->mFrameDt);
        ++r.steps;
    }
    if (e->mGame) {
        for (const auto& kv : sm->scripts()) {
            ++r.total;
            if (kv.second->mDidRun) ++r.done;
            else if (kv.second->mReadyToRun && !kv.second->mFinished && isTrigger(kv.second->type())) {
                if (!r.waiting.empty()) r.waiting += ",";
                r.waiting += std::to_string(kv.first) + ":" + sm->getScriptName(kv.second->type());
            }
        }
        if (CPlayer* p = e->mGame->getPlayer(e->mLocalPlayer)) r.result = p->mWon ? "won" : p->mLost ? "lost" : "running";
    }
    r.aftermath = e->mCurrentInterface == m && m->mCurrentMenu == MENU_AFTERMATH;
    if (CCampaign* c = e->getCurCampaign()) {
        r.unlockedC = c->mMaxChapter;
        r.unlockedE = c->mMaxEpisode;
    }
    e->resetGame();
    e->mMenuInterface.reset();
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data";
    std::string saveDir;
    std::string maps = "all";
    int steps = 3000;
    float dt = 0.1f;
    bool verbose = false;
    bool force = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data") data = next();
        else if (a == "--steps") steps = std::atoi(next().c_str());
        else if (a == "--maps") maps = next();
        else if (a == "--save-dir") saveDir = next();
        else if (a == "--dt") dt = static_cast<float>(std::atof(next().c_str()));
        else if (a == "--verbose") verbose = true;
        else if (a == "--force-triggers") force = true;
    }
    CUtils::setQuiet(true);
    CUtils::setDataDir(data);
    CMath math(4096);
    CMeshManager::setHeadless(true);
    CConfig::get().load(CUtils::dataPath("config.txt"));
    CTxtLoc::get().load("English");
    if (saveDir.empty()) {
        saveDir = (std::filesystem::temp_directory_path() / "campaign_smoke_save").string();
    }
    std::error_code ec;
    std::filesystem::create_directories(saveDir, ec);
    std::vector<unsigned> ids;
    if (maps == "all") {
        ids.push_back(201);
        for (unsigned id = 301; id <= 318; ++id) ids.push_back(id);
    } else {
        std::stringstream ss(maps);
        std::string tok;
        while (std::getline(ss, tok, ',')) ids.push_back(static_cast<unsigned>(std::atoi(tok.c_str())));
    }
    int failed = 0;
    for (unsigned id : ids) {
        // each mission from a fresh progress file
        std::filesystem::remove(std::filesystem::path(saveDir) / "cmp1prog.txt", ec);
        const Result r = runMission(id, steps, dt, saveDir, verbose, force);
        std::string name = CConfig::get().getString("MapName" + std::to_string(id), "");
        if (id >= 300) {
            CEngine* e = CEngine::getSingletonPtr();
            CCampaign c(1);
            (void)e;
            if (c.parseDef())
                if (const CCampaignMapInfo* info = c.getMapInfo(id)) name = info->name;
        }
        while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
        std::printf("map %u %s: loaded %d steps %d speakers %d scripts %d/%d result %s aftermath %d unlocked %u.%u "
                    "forced %d waiting %s\n",
                    id, name.c_str(), r.loaded ? 1 : 0, r.steps, r.speakers, r.done, r.total, r.result.c_str(),
                    r.aftermath ? 1 : 0, r.unlockedC, r.unlockedE, r.forced, r.waiting.empty() ? "-" : r.waiting.c_str());
        std::fflush(stdout);
        if (!r.loaded) ++failed;
    }
    return failed == 0 ? 0 : 1;
}
