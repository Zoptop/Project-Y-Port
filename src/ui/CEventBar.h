// CEventBar.h - port of ProjectY::CEventBar (Ghidra 0x17208c..0x17559c,
// 0x118 bytes): the event icons down the left edge (IPP +0x98).
//
// addEvent queues an icon (sector taken/lost/under attack, unit produced,
// special operation ready, tech researched, ...) with a time to live of
// 300 s (30 s for "sector lost"), sorted by priority (+0x38, higher first)
// and filtered by the EventVerboseness option. Events of the same type are
// stacked (one icon with a count). timeUpdate counts the TTL down and drops
// events that are no longer valid; tapping an icon selects the unit and/or
// scrolls to it (processEvent) and shortens it to 0.5 s.
#pragma once

#include <string>
#include <vector>

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CEngine;
class CInterfaceIngamePerPlayer;

// EEventTypes (0..4 special operation ready, 5 sector under attack, 6 sector
// lost, 7..0x1c unit produced, 0xf level up, 0x10 sector taken, 0x1e + tech
// researched)
enum EEventTypes : int {
    EVENT_AIRSTRIKE_READY = 0, EVENT_SOLDIERDROP_READY = 1, EVENT_CRATEDROP_READY = 2, EVENT_MINEDROP_READY = 3,
    EVENT_OP4_READY = 4, EVENT_SECTOR_UNDER_ATTACK = 5, EVENT_SECTOR_LOST = 6,
    EVENT_PRODUCED_ROBOBRAIN = 7, EVENT_PRODUCED_BOMBSUIT = 8, EVENT_PRODUCED_SOLDIER = 9,
    EVENT_PRODUCED_TANK = 10, EVENT_PRODUCED_HEAVYTANK = 0xb, EVENT_PRODUCED_REPAIR = 0xc,
    EVENT_PRODUCED_SPIDER = 0xd, EVENT_PRODUCED_SCORPION = 0xe, EVENT_LEVEL_UP = 0xf, EVENT_SECTOR_TAKEN = 0x10,
    EVENT_TECH_FIRST = 0x1e,
    kNumEventTypes = 0x3f,
};

// CEvent (0x44 bytes)
struct CEvent {
    int type = 0;            // +0x00
    unsigned objId = 0;      // +0x04
    CVector2 position;       // +0x08
    CVector2 drawPos;        // +0x10 top-left (pixels, set by draw)
    float ttl = 300.0f;      // +0x18
    bool active = true;      // +0x1c clickable
    Vec3 color;              // +0x20
    CVector2 uv;             // +0x2c icon
    std::string label;       // +0x34 (beginner mode / status text)
    int priority = 0;        // +0x38
    bool drawn = false;      // +0x3c (first of its type: has an icon this frame)
    std::string shortText;   // +0x40
};

class CEventBar {
public:
    CEventBar();    // Ghidra 0x17208c
    ~CEventBar();
    void init(CInterfaceIngamePerPlayer* ipp) { mIPP = ipp; }   // Ghidra 0x1720e8
    void notifyOfSelection(unsigned objId);       // Ghidra 0x1720f0
    bool isEventStillValid(CEvent& e);            // Ghidra 0x172238
    void timeUpdate();                            // Ghidra 0x1725fc
    void processEvent(CEvent& e);                 // Ghidra 0x1726d8
    bool processActionUp(const CVector2& pos, bool longTouch);   // Ghidra 0x172c44
    void drawEvent(CEvent& e);                    // Ghidra 0x172d64
    void draw();                                  // Ghidra 0x173270
    // Ghidra 0x17343c: unique = drop it if an event of this type and object exists
    void addEvent(int type, unsigned objId, const CVector2& position, bool unique, const std::string& shortText);
    void addEvent(int type, unsigned objId, bool unique, const std::string& shortText);         // Ghidra 0x175504
    void addEvent(int type, const CVector2& position, bool unique, const std::string& shortText);   // Ghidra 0x17559c

    CInterfaceIngamePerPlayer* mIPP = nullptr;   // +0x00
    CEngine* mEngine = nullptr;                  // +0x04
    std::vector<CEvent*> mEvents;                // +0x08 (owned)
    int mTypeCount[kNumEventTypes] = {};         // +0x14 per-type count while drawing
    CVector2 mFirstEventPos;                     // +0x110 (marker type 5)
};

}  // namespace py
