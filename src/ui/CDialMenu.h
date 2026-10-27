// CDialMenu.h - port of ProjectY::CDialMenu (Ghidra 0x161490..0x165c70,
// 0x2d8 bytes), the radial menu around a building or unit (IPP +0x38).
//
// The object's addDialButtons vfunc (+0x80) fills it (CDialMenuButtons.cpp
// ports the overrides); init stores the object id. Each frame timeUpdate
// recomputes the centre (the object projected to pixels) and lays the N
// buttons out on a circle of radius guiScale * (70 + 10 N) px:
//   button k at angle a_k = -k * 2 pi / N, position = centre + R (cos a_k, sin a_k)
// (pixels, y down: button 0 to the right, then counter-clockwise on screen).
// processTouchMove selects the button under the finger's direction:
//   angle = atan2(-dy, dx) in [0, 2 pi), k = trunc((angle + pi/N) / (2 pi/N)) mod N.
// The touch up sends getSelectedButton() as cmdProcessDialButton (IPP).
// [PC] the same angle maths drives the mouse hover; buttons are clicked
// directly (CInterfaceIngamePerPlayerPC.cpp).
#pragma once

#include <string>
#include <vector>

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CEngine;
class CGameObject;
class CInterfaceIngamePerPlayer;

// EDialButtons (the ids the objects' processDialButton understand); the
// names follow the labels/icons of CDialMenu::drawButton.
enum EDialButtons : int {
    DIAL_CANCEL = 0,
    DIAL_PRODUCE_FIRST = 1,     // 1..12 unit types, see drawCenterMesh
    DIAL_RANK_HEALTH = 0xd, DIAL_RANK_ARMOR = 0xe, DIAL_RANK_RANGE = 0xf, DIAL_RANK_SPEED = 0x10,
    DIAL_RANK_DAMAGE = 0x11,
    // 0x1b MedEvac; turret weapons: 0x1c rocket launcher (weapon 10), 0x1d double
    // blaster (1), 0x1e single laser (8), 0x1f radar tower (0xe)
    DIAL_TURRET_ROCKET = 0x1c, DIAL_TURRET_DOUBLEBLASTER = 0x1d, DIAL_TURRET_LASER = 0x1e, DIAL_TURRET_RADAR = 0x1f,
    DIAL_SPECOP_AIRSTRIKE = 0x12, DIAL_SPECOP_CRATE = 0x13, DIAL_SPECOP_SOLDIER = 0x14,
    DIAL_STOP_PRODUCTION = 0x15, DIAL_APPLY = 0x16, DIAL_START_PRODUCTION = 0x17,
    DIAL_AUTOCALL_ON = 0x18, DIAL_AUTOCALL_OFF = 0x19, DIAL_SPECOP_MINE = 0x1a,
    DIAL_TECH_FIRST = 0x28,     // 0x28 + tech (0..32)
    DIAL_RANDOM_RESEARCH_ON = 0x49, DIAL_RANDOM_RESEARCH_OFF = 0x4a,
    DIAL_HARVESTER = 0x4b,
    kNumDialButtonTypes = 0x4c,
};

// CDialButton (0x1c bytes)
struct CDialButton {
    int type = 0;            // +0x00 EDialButtons
    CVector2 pos;            // +0x04 centre in pixels (timeUpdate)
    bool selected = false;   // +0x0c
    int value = 5;           // +0x10 background icon column (row 3 of the atlas)
    float angle = 0.0f;      // +0x14 radians (<= 0)
    std::string label;       // +0x18 (cost / rank text)
};

class CDialMenu {
public:
    CDialMenu();    // Ghidra 0x161490
    ~CDialMenu();   // Ghidra 0x16151c

    void init(CInterfaceIngamePerPlayer* ipp, CGameObject* object);   // Ghidra 0x161594
    // Ghidra 0x165c70: gated by features 8 (rank buttons 0x18/0x19) and 0x19
    // (0x15/0x17); a type already present is not added again.
    void addButton(int type, int value, const std::string& label);
    int getSelectedButton() const;     // Ghidra 0x1615b0 (0 if none)
    void timeUpdate();                 // Ghidra 0x161600
    void processTouchMove(const CVector2& screenPos);   // Ghidra 0x1622e0
    void draw();                       // Ghidra 0x165c2c
    void drawButton(CDialButton& b);   // Ghidra 0x1627d8
    void drawCenterMesh(int selectedButton);   // Ghidra 0x162500

    // The button index processTouchMove picks for a direction (pixel delta
    // from the centre, y down) among n buttons. Pure maths, for the tests.
    static int buttonIndexForDelta(float dx, float dy, int n);
    // The pixel offset of button k of n from the centre (radius r).
    static CVector2 buttonOffset(int k, int n, float r);
    // The layout radius for n buttons: guiScale * (10 n + 70)
    static float layoutRadius(int n, float guiScale);
    // [PC] select button i (in layout order); false if out of range
    bool selectIndex(int i);
    // [PC] the button index under a pixel (inside its 128 * guiScale square), -1 if none
    int hitTestButton(const CVector2& pixel) const;

    CInterfaceIngamePerPlayer* mIPP = nullptr;   // +0x00
    CEngine* mEngine = nullptr;                  // +0x04
    std::vector<CDialButton> mButtons;           // +0x08 (pointers in the original)
    unsigned mObjectId = 0;                      // +0x14
    Vec3 mColor;                                 // +0x18 selected button colour (default magenta)
    CVector2 mCenter;                            // +0x24 pixels
    CVector2 mButtonCenters[kNumDialButtonTypes];   // +0x2c (type + 5) * 8: centres by type (markers)
    bool mButtonVisible[kNumDialButtonTypes] = {};  // +0x28c by type
};

// CGameObject::addDialButtons (vtable +0x80) and its overrides (CUnit,
// CUnitRoboBrain, CUnitTurret, CBuildingProduction and its four factories,
// CBuildingHQ, CBuildingSpecop, CBuildingScienceLab), dispatched on the
// object's category and type (CDialMenuButtons.cpp): they only read the
// object and fill the menu.
void addDialButtons(CGameObject* object, CDialMenu* menu);

}  // namespace py
