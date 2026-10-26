// CScriptManager.h - port of ProjectY::CScriptManager (owned by CLevel, +0x15c).
//
// update() (Ghidra 0x38dcb8), called from the game loop:
//   if gameTime < nextTick: return;  nextTick = gameTime + 0.21
//   for each script in ascending id order, unless Finished:
//     not ReadyToRun: ReadyToRun = 1; any reqid not DidRun, or any xorid
//        DidRun -> ReadyToRun = 0, next script; else enableRun()
//     ReadyToRun:     any xorid DidRun -> ReadyToRun = 0, next script
//     if runAt < gameTime: run()
//   A reqid/xorid that names no script throws std::out_of_range ("map::at"),
//   which ends the original program; shipped maps never do this.
// addScript (0x38e874): "%*s %s %[^\n]" -> type name + parameter string,
//   construct, parseParams, then scripts[id] = script (a duplicate id
//   replaces the earlier script).
#pragma once

#include <map>
#include <memory>
#include <string>

#include "game/CScript.h"

namespace py {

class CEngine;

class CScriptManager {
public:
    explicit CScriptManager(CEngine* engine);
    ~CScriptManager();

    CScript* addScript(const std::string& line);
    void update();
    void setScriptDone(int id);
    CScript* addEndGameScript(int player, int result);
    std::string getScriptName(int type) const;

    CScript* getScript(int id) const;
    const std::map<int, std::unique_ptr<CScript>>& scripts() const { return mScripts; }

    // The script manager of the current level, i.e. what the original reaches
    // through engine->game->level->scriptManager (the newest one constructed).
    static CScriptManager* current();

    CEngine* mEngine;                                   // +0x00
    std::map<int, std::unique_ptr<CScript>> mScripts;   // +0x04
    std::map<std::string, int> mTypeByName;             // +0x1c (registerScriptType)
    std::map<int, std::string> mNameByType;             // +0x34
    int mIdCounter = 10000;                             // +0x4c auto id source
    float mNextTick = 0.0f;                             // +0x50

private:
    static CScriptManager* sCurrent;
};

}  // namespace py
