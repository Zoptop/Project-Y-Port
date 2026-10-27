// CInterfaceElementImageSlider.h - ports of ProjectY::CInterfaceElementImageSlider
// (Ghidra 0x18ea14..0x18fce0) and CInterfaceElementMapImageSlider
// (0x19004c..0x1903a4): a picker that shows one entry at a time (preview
// image on a submesh of menu.3DS.mesh, name, two info lines, revision) and
// steps to the next entry (wrapping) on a tap. The custom game's map slider
// is a MapImageSlider; the campaign slider uses CInterfaceElementCampaignImageSlider.
//
// Entries are a std::map<unsigned id, CEntry*> (+0x4c), so they are ordered
// by id; +0x48 is the current id.
//
// Draw (Ghidra 0x18f17c), all text with drawStringDirect on the menu plane,
// colour c = +0x2c and size s = +0x38:
//   title (+0x28)           at (+0x6c, +0x70)      size s,        c,        align 2
//   entry name (+4)         at (+0x74, +0x78 + 2)  0.75 s,        0.75 c,   align 0
//   revision line           at (+0x74, +0x78 - 5)  0.4 s:  outdated (file < config revision):
//                           "Revision: %i (outdated, get %i)" in (0.75, 0.25, 0.25) c,
//                           else "Revision: %i (up to date)" in 0.75 c
//   entry line 1 (+8)       at (+0x7c, +0x80)      0.75 s,        0.75 c
//   entry line 2 (+0xc)     at (+0x84, +0x88)      0.75 s,        0.75 c
//   entry +0x10 / +0x14 if not empty at (+0x8c, +0x90) / (+0x94, +0x98), 0.6 s
//   "Touch to change" if enabled at (+0x9c, +0xa0 - 2 s), 0.6 s, centred
//   "Need Download" if purchased but not downloaded, at (+0x9c, +0xa0), 0.75 s, centred
//   entry line 2 again [!] (the original queues +0xc twice)
//   the preview texture on submesh +0xa4 with simplealpha.gfx, BLENDCOLOR
//   (1,0,0,0.7) not purchased, (0,0,1,0.7) not downloaded, else (0,0,0,1).
// Hit box (isHit 0x18ea1c): x from +0x18 to +0x18 + w, y from +0x1c - h to +0x1c.
#pragma once

#include <map>
#include <memory>
#include <string>

#include "render/Math3D.h"
#include "ui/CInterfaceElement.h"

namespace py {

class CTexture;

class CInterfaceElementImageSlider : public CInterfaceElement {
public:
    // CInterfaceElementImageSlider::CEntry (0x2c bytes, addEntry 0x18fce0)
    struct CEntry {
        unsigned id = 0;              // +0x00
        std::string name;             // +0x04
        std::string line1;            // +0x08 (maps: "<n> player")
        std::string line2;            // +0x0c (maps: "<size> km²")
        std::string line3;            // +0x10
        std::string line4;            // +0x14
        CTexture* texture = nullptr;  // +0x18 (unused: the slider loads <id>icon.png itself)
        bool purchased = false;       // +0x1c
        bool downloaded = false;      // +0x1d
        unsigned numPlayers = 0;      // +0x20
        unsigned revision = 0;        // +0x24 of the installed file ("set revision")
        unsigned currentRevision = 0; // +0x28 config CurrentMapRevision<id>
    };

    // Ghidra 0x18ecc8: the short form (menu, action, x, y, w, h, title, size, colour)
    CInterfaceElementImageSlider(CInterfaceMenu* menu, int action, float x, float y, float w, float h,
                                 const std::string& title, float size, const Vec3& color);
    // Ghidra 0x18ee1c: (menu, action, x, y, titleX, titleY, nameX, nameY,
    // line1X, line1Y, line2X, line2Y, touchX, touchY, w [+0x3c], h [+0x40],
    // title, submesh, size, colour, hitW, hitH, hitOffX, hitOffY)
    CInterfaceElementImageSlider(CInterfaceMenu* menu, int action, float x, float y, float titleX, float titleY,
                                 float nameX, float nameY, float line1X, float line1Y, float line2X, float line2Y,
                                 float touchX, float touchY, float w, float h, const std::string& title,
                                 const std::string& subMesh, float size, const Vec3& color, float hitW, float hitH,
                                 float hitOffX, float hitOffY);
    ~CInterfaceElementImageSlider() override;   // Ghidra 0x18eff8

    void draw() override;                                      // Ghidra 0x18f17c
    bool isHit(CInterfaceEvent* e) override;                   // Ghidra 0x18ea1c
    void processEventActionDown(CInterfaceEvent*) override {}  // Ghidra 0x18ea18
    void processEventActionMove(CInterfaceEvent*) override {}  // Ghidra 0x18ea14
    int processEventActionUp(CInterfaceEvent* e) override;     // Ghidra 0x18eab4

    // Ghidra 0x18fce0 (std::map::insert: an existing id is kept)
    void addEntry(unsigned id, const std::string& name, const std::string& line1, const std::string& line2,
                  CTexture* texture, bool purchased, bool downloaded, unsigned numPlayers, unsigned revision,
                  unsigned currentRevision, const std::string& line3, const std::string& line4);
    // Ghidra 0x18ec68: drops the previous preview texture from the texture manager, loads the new one
    void changeTexture(const std::string& file);
    // map::operator[] of the current id (creates an empty entry for an unknown id, like the original)
    CEntry& current();
    // The id after the current one, wrapping to the first (the tap's step)
    unsigned nextId() const;

    float mHitX = 0.0f, mHitY = 0.0f;   // +0x18
    float mHitW = 0.0f, mHitH = 0.0f;   // +0x20
    std::string mTitle;                 // +0x28
    Vec3 mColor;                        // +0x2c
    float mSize = 0.0f;                 // +0x38
    float mW = 0.0f, mH = 0.0f;         // +0x3c
    unsigned mCurrentId = 0;            // +0x48
    std::map<unsigned, std::unique_ptr<CEntry>> mEntries;   // +0x4c
    int mAction = 0;                    // +0x64
    CTexture* mTexture = nullptr;       // +0x68
    float mTitleX = 0.0f, mTitleY = 0.0f;     // +0x6c
    float mNameX = 0.0f, mNameY = 0.0f;       // +0x74
    float mLine1X = 0.0f, mLine1Y = 0.0f;     // +0x7c
    float mLine2X = 0.0f, mLine2Y = 0.0f;     // +0x84
    float mLine3X = 0.0f, mLine3Y = 0.0f;     // +0x8c (never set by a constructor)
    float mLine4X = 0.0f, mLine4Y = 0.0f;     // +0x94 (never set by a constructor)
    float mTouchX = 0.0f, mTouchY = 0.0f;     // +0x9c
    std::string mSubMesh;               // +0xa4
};

class CInterfaceElementMapImageSlider : public CInterfaceElementImageSlider {
public:
    using CInterfaceElementImageSlider::CInterfaceElementImageSlider;   // Ghidra 0x19004c
    // Ghidra 0x1901c8: ignored while disabled; after the step it loads
    // map<id>icon.png, sets the menu's "outdated" flag (+0x5fc) and the
    // entry's purchased/downloaded flags, then sends the action.
    int processEventActionUp(CInterfaceEvent* e) override;
};

}  // namespace py
