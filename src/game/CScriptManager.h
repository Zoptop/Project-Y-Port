// CScriptManager.h - port of ProjectY::CScriptManager (owned by CLevel, +0x15c).
//
// update() (Ghidra 0x38dcb8), called from the game loop:
//   if gameTime < nextTick: return;  nextTick = gameTime + 0.21
//   for each script in ascending id order, unless Finished:
//     not ReadyToRun: ReadyToRun = 1; any reqid not DidRun, or any xorid
//        DidRun -> ReadyToRun = 0, next script; else enableRun()
//     ReadyToRun:     any xorid DidRun -> ReadyToRun = 0, next script
//     if runAt < gameTime: run()
//   (gameTime is re-read for every script: a run() never changes it.)
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
#include "game/CScriptedObstacle.h"

namespace py {

class CEngine;
class CPlayer;

// The in-game interface as the scripts see it (CInterfaceIngame /
// CInterfaceIngamePerPlayer, phase 5). The original reaches the interface of
// a player through CInterfaceIngame::getInterfaceForPlayer, which exists only
// for the local human player; without one the queries fail and nothing is
// shown. That is also the port's behaviour while no IScriptUI is installed
// (headless runs, tests, tools).
class IScriptUI {
public:
    virtual ~IScriptUI() = default;
    // Queries that decide whether a trigger fires.
    virtual bool isGameObjectSelected(CPlayer* player, unsigned id) = 0;   // selecttrigger
    virtual bool isUnitInGroup(CPlayer* player, unsigned id) = 0;          // unittrigger type 5
    // speaker: setOverlayText(text, cmdtext, showtime, script id, nopause,
    // icon). Returns false if the player has no interface. [!] The speaker
    // script itself never sets DidRun: the overlay calls
    // CScriptManager::setScriptDone(id) when the text is dismissed or timed
    // out (CInterfaceIngamePerPlayer::timeUpdateOverlayText, Ghidra 0x1a8600;
    // it also pauses/resumes the game for texts without nopause), so scripts waiting
    // on a speaker stall without an interface.
    virtual bool showSpeakerText(CPlayer* player, const CScriptSpeaker& speaker) = 0;
};

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

    static void setUI(IScriptUI* ui) { sUI = ui; }
    static IScriptUI* ui() { return sUI; }
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
    // +0x54 "block" obstacles by block id (CScriptBlock::run). The original
    // never deletes them (and a re-used id leaks the old one, still active
    // in the navmesh); destroying a CScriptedObstacle does not touch the navmesh.
    std::map<unsigned, std::unique_ptr<CScriptedObstacle>> mObstacles;

private:
    static CScriptManager* sCurrent;
    static IScriptUI* sUI;
};

}  // namespace py
