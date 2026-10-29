// CScript.h - port of ProjectY::CScript and its 19 subclasses (map scripting).
//
// A "script <type> <params>" line of a map file becomes one CScript object
// (CScriptManager::addScript). The base class owns the generic parameter
// parsing (CScript::parseParams, Ghidra 0x38963c) and the scheduling state
// that CScriptManager::update drives; each subclass reads its own parameters.
//
// Parameter grammar (parseParams): the parameter string is split on " \n\r"
// (boost::char_separator, empty tokens dropped). A token containing '"' starts
// a quoted value: following tokens are appended with a single ' ' until one
// contains a '"' (so runs of spaces inside quotes collapse to one). Then
//   quoted   name("text")  -> key = up to '(', value = substr(p+2, len-p-4)
//   name(value)            -> key = up to '(', value = substr(p+1, len-p-2)
//   name                   -> key = name, value = ""      (a flag)
// The last character before the value is assumed to be ')' and is dropped
// unchecked. Later keys overwrite earlier ones (std::map::operator[]).
//
// Value conversion (getParam*):
//   int/float: std::istringstream >> v. Floats go through strtod and are then
//     narrowed to float (libstdc++ __convert_to_v<float>). A failed conversion
//     gives 0. [!] An *empty* value ("param2()") leaves the original's local
//     uninitialised; the port returns 0. In the shipped maps this only occurs
//     for aioption param2 with types that never read it.
//   bool: true if the key is present at all, whatever its value.
//   vec2: "x;y" split on ';' (the original asserts if there is no ';').
//   int list: "a;b;c" split on ';'.
//
// Common parameters: id (default: CScriptManager's running counter, which is
// incremented for *every* script, value before the increment), loopable,
// fixedspeed, reqid, xorid, delay (ms; stored as ms*0.001*speedFactor unless
// fixedspeed). The script's first run time is set to the delay.
//
// Ghidra refs: CScript::CScript 0x3884ac, run 0x3881e4, enableRun 0x38802c,
// getParamInt 0x3889cc, getParamFloat 0x388808, getParamVec2 0x389188,
// getParamVarIntList 0x388c38, getParamBool 0x388b7c, getParamString 0x3886e0.
#pragma once

#include <cstdio>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/CVector2.h"

namespace py {

class CEngine;
class CSector;
class CPlayer;

// Script type ids (the switch in CScriptManager::addScript).
enum EScriptType {
    SCRIPT_AIRDROP = 1,          // "unitdrop"
    SCRIPT_CRATEDROP = 2,        // "cratedrop"
    SCRIPT_AREATRIGGER = 3,      // "areatrigger"
    SCRIPT_SPEAKER = 4,          // "speaker"
    SCRIPT_FLAGTRIGGER = 5,      // "flagtrigger"
    SCRIPT_SELECTTRIGGER = 6,    // "selecttrigger"
    SCRIPT_UNITTRIGGER = 7,      // "unittrigger"
    SCRIPT_GAMESTATE = 8,        // "gamestate"
    SCRIPT_BEACON = 9,           // "beacon"
    SCRIPT_AIRSTRIKE = 10,       // "airstrike"
    SCRIPT_FEATURE = 11,         // "feature"
    SCRIPT_BLOCK = 12,           // "block"
    SCRIPT_BUILDINGTRIGGER = 13, // "bldtrigger"
    SCRIPT_MARKER = 14,          // "marker"
    SCRIPT_MOD = 15,             // "mod"
    SCRIPT_TIMETRIGGER = 16,     // "timetrigger"
    SCRIPT_AIOPTION = 17,        // "aioption"
    SCRIPT_ADDACH = 18,          // "addach"
    SCRIPT_COUNTTRIGGER = 19,    // "counttrigger"
};

class CScript {
public:
    virtual void save(FILE* f);              // +0x00 Ghidra 0x38806c (CGameSave.cpp)
    virtual void load(FILE* f);              // +0x04 Ghidra 0x388110
    explicit CScript(const std::string& params);
    virtual ~CScript() = default;

    // vtable order in the original: save, load, run, parseParams, enableRun
    virtual void run();            // base: repeat handling (Finished / next run time)
    virtual void parseParams();
    virtual void enableRun();      // runAt = gameTime + delay

    int type() const { return mType; }
    int id() const { return mId; }

    // ---- parameter access (protected in spirit; public for tests/tools)
    const std::map<std::string, std::string>& params() const { return mParams; }
    int getParamInt(const std::string& key, int def) const;
    float getParamFloat(const std::string& key, float def) const;
    bool getParamBool(const std::string& key) const;
    std::string getParamString(const std::string& key) const;
    CVector2 getParamVec2(const std::string& key, const CVector2& def) const;
    void getParamVarIntList(const std::string& key, std::vector<int>& out) const;

    // ---- state (field offsets of the original object)
    CEngine* mEngine = nullptr;          // +0x04
    int mType = 0;                       // +0x08
    bool mReadyToRun = false;            // +0x0c
    bool mDidRun = false;                // +0x0d
    bool mFinished = false;              // +0x0e
    int mId = 0;                         // +0x10
    float mDelay = 0.0f;                 // +0x14  seconds (already scaled)
    float mRunAt = 0.0f;                 // +0x18  absolute game time of the next run
    bool mLoopable = false;              // +0x1c
    bool mFixedSpeed = false;            // +0x1d
    std::string mParamString;            // +0x20
    std::map<std::string, std::string> mParams;   // +0x24
    std::vector<int> mReqIds;            // +0x3c
    std::vector<int> mXorIds;            // +0x48
};

// ---------------------------------------------------------------- subclasses
// Field comments give the original offset and the parameter default.

class CScriptAirdrop : public CScript {           // "unitdrop", 0x70 bytes
public:
    explicit CScriptAirdrop(const std::string& p) : CScript(p) { mType = SCRIPT_AIRDROP; }
    void parseParams() override;
    void run() override;
    int mUnitType = 0;           // +0x54 unittype(0)
    int mPlayer = 0;             // +0x58 player(0)
    int mCount = 1;              // +0x5c count(1)
    CVector2 mPosition;          // +0x60 position(0;0)
    bool mScrollTo = false;      // +0x68 scrollto
    int mUnitId = -1;            // +0x6c unitid(-1)
};

class CScriptCratedrop : public CScript {         // "cratedrop", 0x64 bytes
public:
    explicit CScriptCratedrop(const std::string& p) : CScript(p) { mType = SCRIPT_CRATEDROP; }
    void parseParams() override;
    void run() override;
    int mMaxCrates = 1;          // +0x54 maxcrates(1)
    bool mPosRandom = false;     // +0x58 posrandom
    CVector2 mPosition;          // +0x5c position(0;0)
};

class CScriptAreaTrigger : public CScript {       // "areatrigger", 0x68 bytes
public:
    explicit CScriptAreaTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_AREATRIGGER; }
    void parseParams() override;
    void run() override;
    CVector2 mCenter;            // +0x54 center(0;0)
    float mRadius = 1.0f;        // +0x5c radius(1)
    int mPlayer = -1;            // +0x60 player(-1)
    int mUnitType = -1;          // +0x64 unittype(-1)
};

class CScriptSpeaker : public CScript {           // "speaker", 0x7c bytes
public:
    explicit CScriptSpeaker(const std::string& p) : CScript(p) { mType = SCRIPT_SPEAKER; }
    void parseParams() override;
    void run() override;
    std::string mText;           // +0x54 text("")
    int mPlayer = 1;             // +0x58 player(1)
    bool mHasScrollTo = false;   // +0x5c scrollto != (0,0)
    CVector2 mScrollTo;          // +0x60 scrollto(0;0)
    float mShowTime = 0.0f;      // +0x68 showtime(0)
    bool mNoPause = false;       // +0x6c nopause
    int mIconPlayer = 0;         // +0x70 iconplayer(0)
    std::string mIconName;       // +0x74 iconname("")
    std::string mCmdText;        // +0x78 cmdtext("")
};

class CScriptSectorFlagTrigger : public CScript { // "flagtrigger", 0x70 bytes
public:
    explicit CScriptSectorFlagTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_FLAGTRIGGER; }
    void parseParams() override;
    void run() override;
    int mPlayerNew = -1;         // +0x54 playernew(-1)
    int mPlayerOld = -1;         // +0x58 playerold(-1)
    int mTriggerType = 2;        // +0x5c type(2)
    int mSectorId = 1;           // +0x60 sector(1)
    CSector* mSector = nullptr;  // +0x64 resolved at parse time (CLevel::getSector)
    CPlayer* mOldPlayer = nullptr;   // +0x68 resolved at parse time, or null for -1
    CPlayer* mNewPlayer = nullptr;   // +0x6c
};

class CScriptSelectTrigger : public CScript {     // "selecttrigger", 0x5c bytes
public:
    explicit CScriptSelectTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_SELECTTRIGGER; }
    void parseParams() override;
    void run() override;
    int mUnitId = -1;            // +0x54 unitid(-1)
    int mPlayer = -1;            // +0x58 player(-1)
};

class CScriptUnitTrigger : public CScript {       // "unittrigger", 0x64 bytes
public:
    void save(FILE* f) override;   // +0x00 Ghidra 0x391888
    void load(FILE* f) override;   // +0x04 Ghidra 0x3918b4
    explicit CScriptUnitTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_UNITTRIGGER; }
    void parseParams() override;
    void run() override;
    void enableRun() override {}   // [!] empty override: the run time is never re-based
    int mUnitId = -1;            // +0x54 unitid(-1)
    int mPlayer = -1;            // +0x58 player(-1)
    int mTriggerType = -1;       // +0x5c type(-1)
    bool mUnitWasActiveBefore = false;   // +0x60
};

class CScriptGamestate : public CScript {         // "gamestate", 0x60 bytes
public:
    explicit CScriptGamestate(const std::string& p) : CScript(p) { mType = SCRIPT_GAMESTATE; }
    void parseParams() override;
    void run() override;
    int mResult = 1;             // +0x54 result(1)  1 = won, else lost
    int mPlayer = 1;             // +0x58 player(1)
    bool mDontEnd = false;       // +0x5c dontend
};

class CScriptBeacon : public CScript {            // "beacon", 0x68 bytes
public:
    explicit CScriptBeacon(const std::string& p) : CScript(p) { mType = SCRIPT_BEACON; }
    void parseParams() override;
    void run() override;
    int mPlayer = 1;             // +0x54 player(1)
    CVector2 mPosition;          // +0x58 position(0;0)
    int mUnitId = -1;            // +0x60 unitid(-1)
    bool mScrollTo = false;      // +0x64 scrollto
};

class CScriptAirstrike : public CScript {         // "airstrike", 0x68 bytes
public:
    explicit CScriptAirstrike(const std::string& p) : CScript(p) { mType = SCRIPT_AIRSTRIKE; }
    void parseParams() override;
    void run() override;
    int mLevel = 1;              // +0x54 level(1)
    int mPlayer = 0;             // +0x58 player(0)
    bool mScrollTo = false;      // +0x5c scrollto
    CVector2 mPosition;          // +0x60 position(0;0)
};

class CScriptFeature : public CScript {           // "feature", 0x64 bytes
public:
    explicit CScriptFeature(const std::string& p) : CScript(p) { mType = SCRIPT_FEATURE; }
    void parseParams() override;
    void run() override;
    int mFeature = 0;            // +0x54 feature(0)
    bool mDisable = false;       // +0x58 disable
    int mParam1 = -1;            // +0x5c param1(-1)
    int mParam2 = -1;            // +0x60 param2(-1)
};

class CScriptBlock : public CScript {             // "block", 0x68 bytes
public:
    explicit CScriptBlock(const std::string& p) : CScript(p) { mType = SCRIPT_BLOCK; }
    void parseParams() override;
    void run() override;
    CVector2 mCenter;            // +0x54 center(0;0)
    float mRadius = 10.0f;       // +0x5c radius(10)
    bool mDisable = false;       // +0x60 disable
    int mBlockId = 0;            // +0x64 blockid(0)
};

class CScriptBuildingTrigger : public CScript {   // "bldtrigger", 0x64 bytes
public:
    explicit CScriptBuildingTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_BUILDINGTRIGGER; }
    void parseParams() override;
    void run() override;
    void enableRun() override {}   // [!] empty override, like CScriptUnitTrigger
    int mBuildingId = -1;        // +0x54 buildingid(-1)
    int mTriggerType = -1;       // +0x58 type(-1)
    int mOccupantsAtStart = -1;  // +0x5c snapshot taken on the first run
    int mProductionAtStart = -1; // +0x60 snapshot taken on the first run
};

class CScriptMarker : public CScript {            // "marker", 0x68 bytes
public:
    explicit CScriptMarker(const std::string& p) : CScript(p) { mType = SCRIPT_MARKER; }
    void parseParams() override;
    void run() override;
    int mPlayer = 1;             // +0x54 player(1)
    bool mDisable = false;       // +0x58 disable
    int mMarkerType = 0;         // +0x5c type(0)
    int mObjId = 0;              // +0x60 objid(0)
    bool mScrollTo = false;      // +0x64 scrollto
};

class CScriptMod : public CScript {               // "mod", 0x64 bytes
public:
    explicit CScriptMod(const std::string& p) : CScript(p) { mType = SCRIPT_MOD; }
    void parseParams() override;
    void run() override;
    int mObjId = 0;              // +0x54 objid(0)
    int mModType = 1;            // +0x58 type(1)
    float mParam1 = 0.0f;        // +0x5c param1(0)
    float mParam2 = 0.0f;        // +0x60 param2(0)
};

class CScriptTimeTrigger : public CScript {       // "timetrigger", 0x68 bytes
public:
    void save(FILE* f) override;   // +0x00 Ghidra 0x390de8 [!] StartTime only, not CScript's lines
    void load(FILE* f) override;   // +0x04 Ghidra 0x390e28
    explicit CScriptTimeTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_TIMETRIGGER; }
    void parseParams() override;
    void run() override;
    float mStartTime = -1.0f;    // +0x54 set on the first run
    float mTime = 0.0f;          // +0x58 time(0) seconds
    std::string mText;           // +0x5c text("")
    bool mShow = false;          // +0x60 show
    int mPlayer = 0;             // +0x64 player(0) 0 = all
};

class CScriptAIOption : public CScript {          // "aioption", 0x64 bytes
public:
    explicit CScriptAIOption(const std::string& p) : CScript(p) { mType = SCRIPT_AIOPTION; }
    void parseParams() override;
    void run() override;
    int mPlayer = 1;             // +0x54 player(1)
    int mOptionType = 1;         // +0x58 type(1)
    float mParam1 = 0.0f;        // +0x5c param1(0)
    float mParam2 = 0.0f;        // +0x60 param2(0)
};

class CScriptAddAch : public CScript {            // "addach", 0x5c bytes
public:
    explicit CScriptAddAch(const std::string& p) : CScript(p) { mType = SCRIPT_ADDACH; }
    void parseParams() override;
    void run() override;
    int mPlayer = 1;             // +0x54 player(1)
    int mAchType = 1;            // +0x58 type(1)
};

class CScriptCountTrigger : public CScript {      // "counttrigger", 0x68 bytes
public:
    void save(FILE*) override {}   // +0x00 Ghidra 0x38c860 [!] empty: nothing of the script is saved
    void load(FILE*) override {}   // +0x04 Ghidra 0x38c864
    explicit CScriptCountTrigger(const std::string& p) : CScript(p) { mType = SCRIPT_COUNTTRIGGER; }
    void parseParams() override;
    void run() override;
    bool mShow = false;          // +0x54 show
    int mPlayer = 0;             // +0x58 player(0)
    int mCount = 1;              // +0x5c count(1)
    int mObjType = 1;            // +0x60 type(1)  1 = units, 2 = buildings
    int mSubType = 1;            // +0x64 subtype(1)
};

// boost::char_separator<char>(sep) with the default drop_empty_tokens.
std::vector<std::string> tokenize(const std::string& s, const char* separators);

}  // namespace py
