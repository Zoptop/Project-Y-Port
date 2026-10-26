#include "CScriptManager.h"

#include <cstdio>
#include <stdexcept>

#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "core/Real.h"
#include "game/CEngine.h"

namespace py {

CScriptManager* CScriptManager::sCurrent = nullptr;
IScriptUI* CScriptManager::sUI = nullptr;

CScriptManager* CScriptManager::current() { return sCurrent; }

// Ghidra 0x38e4c0. registerScriptType order of the original constructor.
CScriptManager::CScriptManager(CEngine* engine) : mEngine(engine) {
    static const std::pair<const char*, int> kTypes[] = {
        {"unitdrop", SCRIPT_AIRDROP},        {"cratedrop", SCRIPT_CRATEDROP},
        {"areatrigger", SCRIPT_AREATRIGGER}, {"selecttrigger", SCRIPT_SELECTTRIGGER},
        {"flagtrigger", SCRIPT_FLAGTRIGGER}, {"unittrigger", SCRIPT_UNITTRIGGER},
        {"speaker", SCRIPT_SPEAKER},         {"gamestate", SCRIPT_GAMESTATE},
        {"beacon", SCRIPT_BEACON},           {"airstrike", SCRIPT_AIRSTRIKE},
        {"feature", SCRIPT_FEATURE},         {"block", SCRIPT_BLOCK},
        {"bldtrigger", SCRIPT_BUILDINGTRIGGER}, {"marker", SCRIPT_MARKER},
        {"mod", SCRIPT_MOD},                 {"timetrigger", SCRIPT_TIMETRIGGER},
        {"aioption", SCRIPT_AIOPTION},       {"addach", SCRIPT_ADDACH},
        {"counttrigger", SCRIPT_COUNTTRIGGER},
    };
    for (const auto& t : kTypes) {
        mTypeByName[t.first] = t.second;
        mNameByType[t.second] = t.first;
    }
    sCurrent = this;
}

CScriptManager::~CScriptManager() {
    if (sCurrent == this) sCurrent = nullptr;
}

// Ghidra 0x38e874
CScript* CScriptManager::addScript(const std::string& line) {
    char typeName[32] = {0};
    char params[1000] = {0};
    // [!] the original's buffers are uninitialised stack; a line without
    // parameters would reuse stale bytes. Shipped maps always have parameters.
    std::sscanf(line.c_str(), "%*s %31s %999[^\n]", typeName, params);

    auto it = mTypeByName.find(typeName);
    if (it == mTypeByName.end())   // the original crashes (uninitialised pointer)
        throw std::runtime_error(std::string("CScriptManager::addScript: unknown script type ") + typeName);

    std::unique_ptr<CScript> s;
    switch (it->second) {
        case SCRIPT_AIRDROP: s = std::make_unique<CScriptAirdrop>(params); break;
        case SCRIPT_CRATEDROP: s = std::make_unique<CScriptCratedrop>(params); break;
        case SCRIPT_AREATRIGGER: s = std::make_unique<CScriptAreaTrigger>(params); break;
        case SCRIPT_SPEAKER: s = std::make_unique<CScriptSpeaker>(params); break;
        case SCRIPT_FLAGTRIGGER: s = std::make_unique<CScriptSectorFlagTrigger>(params); break;
        case SCRIPT_SELECTTRIGGER: s = std::make_unique<CScriptSelectTrigger>(params); break;
        case SCRIPT_UNITTRIGGER: s = std::make_unique<CScriptUnitTrigger>(params); break;
        case SCRIPT_GAMESTATE: s = std::make_unique<CScriptGamestate>(params); break;
        case SCRIPT_BEACON: s = std::make_unique<CScriptBeacon>(params); break;
        case SCRIPT_AIRSTRIKE: s = std::make_unique<CScriptAirstrike>(params); break;
        case SCRIPT_FEATURE: s = std::make_unique<CScriptFeature>(params); break;
        case SCRIPT_BLOCK: s = std::make_unique<CScriptBlock>(params); break;
        case SCRIPT_BUILDINGTRIGGER: s = std::make_unique<CScriptBuildingTrigger>(params); break;
        case SCRIPT_MARKER: s = std::make_unique<CScriptMarker>(params); break;
        case SCRIPT_MOD: s = std::make_unique<CScriptMod>(params); break;
        case SCRIPT_TIMETRIGGER: s = std::make_unique<CScriptTimeTrigger>(params); break;
        case SCRIPT_AIOPTION: s = std::make_unique<CScriptAIOption>(params); break;
        case SCRIPT_ADDACH: s = std::make_unique<CScriptAddAch>(params); break;
        case SCRIPT_COUNTTRIGGER: s = std::make_unique<CScriptCountTrigger>(params); break;
        default: break;
    }
    s->parseParams();
    CScript* raw = s.get();
    mScripts[raw->mId] = std::move(s);
    return raw;
}

CScript* CScriptManager::getScript(int id) const {
    auto it = mScripts.find(id);
    return it == mScripts.end() ? nullptr : it->second.get();
}

// Ghidra 0x38dcb8
void CScriptManager::update() {
    const Real now = mEngine->mGameTime;
    if (now < mNextTick) return;
    mNextTick = now + bitsf(0x3e570a3du);   // 0.21f
    for (auto& entry : mScripts) {
        CScript* s = entry.second.get();
        if (s->mFinished) continue;
        if (!s->mReadyToRun) {
            s->mReadyToRun = true;
            bool blocked = false;
            for (int r : s->mReqIds)
                if (!mScripts.at(r)->mDidRun) { blocked = true; break; }
            if (!blocked)
                for (int x : s->mXorIds)
                    if (mScripts.at(x)->mDidRun) { blocked = true; break; }
            if (blocked) {
                s->mReadyToRun = false;
                continue;
            }
            s->enableRun();
            if (!s->mReadyToRun) continue;
        } else {
            bool cancelled = false;
            for (int x : s->mXorIds)
                if (mScripts.at(x)->mDidRun) { cancelled = true; break; }
            if (cancelled) {
                s->mReadyToRun = false;
                continue;
            }
        }
        if (s->mRunAt < mEngine->mGameTime) s->run();
    }
}

// Ghidra 0x38dfec
void CScriptManager::setScriptDone(int id) {
    if (id == -2) {
        CUtils::log("CScriptManager::setScriptDone: End game scripting outdated!");
        return;
    }
    mScripts.at(id)->mDidRun = true;
}

// Ghidra 0x38ec88
CScript* CScriptManager::addEndGameScript(int player, int result) {
    CUtils::log("End game!");
    char line[256];
    std::snprintf(line, sizeof(line), "script gamestate player(%i) result(%i)", player, result);
    CScript* s = addScript(line);
    s->mReadyToRun = true;
    s->mRunAt = 0.0f;
    s->mFinished = false;
    return s;
}

std::string CScriptManager::getScriptName(int type) const {
    auto it = mNameByType.find(type);
    return it == mNameByType.end() ? std::string() : it->second;
}

}  // namespace py
