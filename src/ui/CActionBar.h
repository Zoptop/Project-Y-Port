// CActionBar.h - port of ProjectY::CActionBar (Ghidra 0x11fc00..0x123fc0,
// 0x17c bytes): the row of buttons at the bottom centre (IPP +0xa0).
//
// timeUpdate (= refresh, rebuilt every frame) looks at the selection: the
// last selected object `sel` decides the object buttons, and every unit type
// in the selection gets a select-by-type button. Buttons are 64 * guiScale
// px, spaced 70 * guiScale, the row centred on the screen and 70 * guiScale
// above the bottom. processActionUp handles a tap: some buttons act in the
// interface (eject = cmdUnitOrderRemoveCommander, info screen, deselect,
// select by type, follow, call commander, jump to the closest unit), the
// others send cmdProcessActionBarButton for every selected object.
#pragma once

#include <string>
#include <vector>

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CEngine;
class CInterfaceIngamePerPlayer;

// EActionBarButtonTypes
enum EActionBarButtonTypes : int {
    AB_EJECT = 0, AB_INFO = 1, AB_HOLD_ON = 2, AB_HOLD_OFF = 3, AB_DESELECT = 4,
    AB_SELECT_TYPE_FIRST = 5, AB_SELECT_TYPE_LAST = 0x19,
    AB_FOLLOW = 0x1a, AB_MINES = 0x1b, AB_AUTOLEVEL = 0x1c, AB_AUTOLEVEL_1 = 0x1d, AB_AUTOLEVEL_2 = 0x1e,
    AB_CALL_COMMANDER = 0x1f, AB_CLOSEST_UNIT = 0x20,
    AB_SHIELD_ON = 0x22, AB_SHIELD_OFF = 0x23, AB_STOP_ATTACK = 0x24, AB_DROP_RESOURCES = 0x27,
    kNumActionBarButtonTypes = 0x28,
};

// CActionBarButton (0x24 bytes)
struct CActionBarButton {
    int type = 0;            // +0x00
    CVector2 pos;            // +0x04 top-left (pixels)
    Vec3 color;              // +0x0c
    CVector2 uv;             // +0x18 icon
    std::string label;       // +0x20 (beginner mode)
};

class CActionBar {
public:
    CActionBar();    // Ghidra 0x11fc00
    void init(CInterfaceIngamePerPlayer* ipp) { mIPP = ipp; }   // Ghidra 0x11fc5c
    void timeUpdate() { refresh(); }   // Ghidra 0x123fc0 (tail call)
    void refresh();                    // Ghidra 0x120984
    void draw();                       // Ghidra 0x1201b8
    void drawButton(const CActionBarButton& b);   // Ghidra 0x11fdb8
    // Ghidra 0x120204: true if a button was hit (the touch is consumed)
    bool processActionUp(const CVector2& pos, bool longTouch);
    // [PC] the action of a button, as processActionUp runs it
    void processButton(const CActionBarButton& b);
    // the unit type behind a select-by-type button (5..0x19), 0 otherwise
    static unsigned unitTypeForButton(int buttonType);

    CInterfaceIngamePerPlayer* mIPP = nullptr;   // +0x00
    CEngine* mEngine = nullptr;                  // +0x04
    std::vector<CActionBarButton> mButtons;      // +0x08 (draw order: last added first)
    CVector2 mButtonCenters[kNumActionBarButtonTypes];   // +0x14 (type + 2) * 8 - 4: label points (markers)
    bool mButtonVisible[kNumActionBarButtonTypes] = {};  // +0x154
};

}  // namespace py
