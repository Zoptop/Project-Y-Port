// CGroupBar.h - port of ProjectY::CGroupBar (Ghidra 0x185414..0x187f18,
// 0x20 bytes): the control-group slots down the right edge (IPP +0x9c).
//
// init creates, in network games, an "Ingame Chat" slot (type 1) and with
// the global chat a "Lobby Chat" slot (type 2), then nine group slots
// (type 0, ids 2..10). A long press on a group assigns the selected units
// (and sets the group as rally target of the selected buildings,
// cmdBuildingOrderRallyGroup); if none of them was new, it removes them
// instead. A tap selects the group's units, and scrolls to the group (its
// under-attack unit first) when some of them were already selected.
// Slots are 64 * guiScale px, 70 * guiScale apart, from y = 46 * guiScale.
#pragma once

#include <cstdio>

#include <list>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CEngine;
class CInterfaceIngamePerPlayer;

// CGroupInfo (0x40 bytes)
struct CGroupInfo {
    unsigned id = 0;                 // +0x00
    bool active = false;             // +0x04 has units
    CVector2 pos;                    // +0x10 top-left (pixels, set by draw)
    Vec3 color{0.4f, 0.4f, 0.4f};    // +0x18
    CVector2 uv;                     // +0x24 icon
    std::list<unsigned> units;       // +0x2c
    int type = 0;                    // +0x34 0 group, 1 ingame chat, 2 lobby chat, 3 spacer
    float underAttackTimeout = 0;    // +0x38
    unsigned underAttackUnit = 0;    // +0x3c
};

class CGroupBar {
public:
    void save(FILE* f);   // Ghidra 0x186d34 (CInterfaceIngameSave.cpp)
    void load(FILE* f);   // Ghidra 0x187f18
    CGroupBar();    // Ghidra 0x186eb4
    void init(CInterfaceIngamePerPlayer* ipp);   // Ghidra 0x187718
    void timeUpdate();                           // Ghidra 0x1856c4
    void draw();                                 // Ghidra 0x186a60
    void drawgGroupInfo(CGroupInfo& g);          // Ghidra 0x185920
    // Ghidra 0x187484: true when a slot was hit
    bool processActionUp(const CVector2& pos, bool longTouch);
    void processGI(CGroupInfo& g, bool assign, bool allowScroll);   // Ghidra 0x1871ac

    CGroupInfo* getGroupInfo(unsigned id);           // Ghidra 0x18548c (type 0 only)
    unsigned getFirstGroupUnit(unsigned id);         // Ghidra 0x1854d0
    CGroupInfo* getGroupOfUnit(unsigned unit);       // Ghidra 0x1854fc
    bool isUnitInGroup(unsigned unit, const CGroupInfo& g) const;   // Ghidra 0x18557c
    bool isUnitInGroup(unsigned unit) const;         // Ghidra 0x1855b4 (0xffffffff: any group with units)
    bool isGroupStillValid(CGroupInfo& g);           // Ghidra 0x185640 (drops lost units)
    void enableUnderAttackWarning(unsigned unit);    // Ghidra 0x186cb8
    void removeUnitFromGroups(unsigned unit);        // Ghidra 0x186ef0
    void removeSelectedUnitsFromGroups();            // Ghidra 0x186f68
    void addSelectedUnitsToGroup(CGroupInfo& g);     // Ghidra 0x186fa4
    void addUnitToGroup(CGroupInfo& g, unsigned unit);   // Ghidra 0x187098
    void copyGroupsFromUnit(unsigned from, unsigned to); // Ghidra 0x18710c
    // [PC] the i-th type-0 group (0..8), null if none
    CGroupInfo* groupByIndex(int i);

    CInterfaceIngamePerPlayer* mIPP = nullptr;   // +0x00
    CEngine* mEngine = nullptr;                  // +0x04
    std::vector<CGroupInfo> mGroups;             // +0x08
    int mHighlightGroup = -1;                    // +0x14 (yellow frame for one frame)
    CVector2 mFirstGroupPos;                     // +0x18 (marker type 4)
};

}  // namespace py
