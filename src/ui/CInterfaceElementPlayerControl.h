// CInterfaceElementPlayerControl.h - port of ProjectY::CInterfaceElementPlayerControl
// (Ghidra 0x194cc0..0x196b4c): the player table of the custom game, split
// screen and lobby screens. It edits the engine's player slots directly
// (CEngine::mPlayerSetup, engine+0x4c + i*0x2c) for players 1..n.
//
// Layout (drawStringDirect on the menu plane, size s = +0x38, colour c):
//   header row at y - 1, 0.75 s: "Player" (x, left), "Type" (x+40), "Team"
//   (x+60), "Color" (x+80) centred; networked also "Rank" (x+165),
//   "Score" (x+150), "Ping" (x+100).
//   row i (i = 1..n) at y - 9 - 8 (i-1), size 2.5, colour 0.5 white + 0.5
//   player colour: name (x, left), "Human"/"AI"/"-" (x+40), team "%i" (x+60),
//   a colour swatch quad at (x+80, row-2) (half 8 x 3.5, interface.png
//   uv (0.015, 0.26) size (0.06, 0.01)); networked: "Ready" (x+125), the
//   three stats columns, and a highlight bar behind the local player's row.
//
// Taps (processEventActionUp), per row, boxes 20 wide and 8 high (top at
// the row y): Type (x+40) toggles human <-> AI (a human only while fewer than
// maxHumans slots are human; [!] the count treats a neutral slot 1 as 0 and
// slot 1 type 3 as not human), Team (x+60) cycles 1..n, Color (x+80) cycles
// through the colour table skipping the other players' colours ([!] the
// skip rules depend on n, see the .cpp). In game type 2 (Shared Teams)
// team mates share their colour. Then `action` (0x32) goes to the menu.
#pragma once

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CInterfaceElementPlayerControl : public CInterfaceElement {
public:
    // Ghidra 0x1966f4: (menu, action, x, y, maxHumans, networked, size,
    // colour). Resets all five slots (names from config Player<i>Name).
    CInterfaceElementPlayerControl(CInterfaceMenu* menu, int action, float x, float y, unsigned maxHumans,
                                   bool networked, float size, const Vec3& color);

    void draw() override;                                    // Ghidra 0x194de8
    bool isHit(CInterfaceEvent* e) override;                 // Ghidra 0x194cc0
    int processEventActionUp(CInterfaceEvent* e) override;   // Ghidra 0x195e7c

    // Ghidra 0x1969e4: n players; resets types/colours (not teams or names)
    void setNumPlayers(unsigned n);

    // The colour that follows `color` in the cycle used by the Color column
    // and the Shared-Teams team change (Ghidra 0x195e7c, both copies).
    unsigned nextFreeColor(unsigned color) const;

    float mSize = 0.0f;           // +0x38
    Vec3 mColor;                  // +0x2c
    unsigned mNumPlayers = 4;     // +0x3c
    unsigned mMaxHumans = 0;      // +0x40
    int mAction = 0;              // +0x44
    bool mNetworked = false;      // +0x48
};

}  // namespace py
