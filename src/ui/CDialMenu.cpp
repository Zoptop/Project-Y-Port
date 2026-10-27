// CDialMenu.cpp - see CDialMenu.h.
#include "ui/CDialMenu.h"

#include <cstring>

#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CGameObject.h"
#include "game/CTechManager.h"
#include "render/CCamera.h"
#include "render/CTextRender.h"
#include "render/Colors.h"
#include "ui/CIngameWorldDraw.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CUIUtils.h"

namespace py {

namespace {

// The label and icon table of CDialMenu::drawButton (Ghidra 0x1627d8):
// label = S(prefix) + " " + S(name) [+ " : " + S(desc)], or S(name) alone
// when prefix is 0; icon cell (col, row) of interface.png; selColor: the
// colour of the selected button when it is not the menu colour.
struct DialButtonInfo {
    int prefix, name, desc;
    int col, row;
    const float* selColor;
};
const float kRed[3] = {bitsf(0x3f4ccccd), bitsf(0x3e4ccccd), bitsf(0x3e4ccccd)};
const float kGreen[3] = {bitsf(0x3e4ccccd), bitsf(0x3f4ccccd), bitsf(0x3e4ccccd)};
const float kYellow[3] = {bitsf(0x3f4ccccd), bitsf(0x3f4ccccd), bitsf(0x3e4ccccd)};

bool dialButtonInfo(int type, DialButtonInfo& out) {
    static const DialButtonInfo k[] = {
        /* 0x00 */ {0, 0x114, 0, 5, 7, kRed},
        /* 0x01 */ {0x8c, 0x40, 0x59, 0, 6, nullptr},
        /* 0x02 */ {0x8c, 0x41, 0x5a, 1, 6, nullptr},
        /* 0x03 */ {0x8c, 0x43, 0x5c, 0, 5, nullptr},
        /* 0x04 */ {0x8c, 0x47, 0x60, 7, 1, nullptr},
        /* 0x05 */ {0x8c, 0x42, 0x5b, 3, 6, nullptr},
        /* 0x06 */ {0x8c, 0x45, 0x5e, 2, 6, nullptr},
        /* 0x07 */ {0x8c, 0x46, 0x5f, 7, 2, nullptr},
        /* 0x08 */ {0x8c, 0xa6, 0xa7, 7, 0, nullptr},
        /* 0x09 */ {0x8c, 0x3f, 0x58, 6, 2, nullptr},
        /* 0x0a */ {0x8c, 0xaa, 0xab, 0, 1, nullptr},
        /* 0x0b */ {0x8c, 0xa8, 0xa9, 0, 8, nullptr},
        /* 0x0c */ {0x8c, 300, 0x12d, 1, 0, nullptr},
        /* 0x0d */ {0x8d, 0x77, 0, 2, 7, nullptr},
        /* 0x0e */ {0x8d, 0x90, 0, 3, 7, nullptr},
        /* 0x0f */ {0x8d, 0x78, 0, 1, 7, nullptr},
        /* 0x10 */ {0x8d, 0x74, 0, 4, 7, nullptr},
        /* 0x11 */ {0x8d, 0x8f, 0, 0, 7, nullptr},
        /* 0x12 */ {0x8e, 0x80, 0, 6, 6, nullptr},
        /* 0x13 */ {0x8e, 0x82, 0, 1, 5, nullptr},
        /* 0x14 */ {0x8e, 0x81, 0, 2, 6, nullptr},
        /* 0x15 */ {0, 0x112, 0, 4, 1, kRed},
        /* 0x16 */ {0, 0x115, 0, 8, 3, kYellow},
        /* 0x17 */ {0, 0x113, 0, 4, 1, kGreen},
        /* 0x18 */ {0, 0x116, 0, 0, 2, kGreen},
        /* 0x19 */ {0, 0x117, 0, 0, 2, kRed},
        /* 0x1a */ {0x8e, 0x11f, 0, 1, 1, nullptr},
        /* 0x1b */ {0x8e, 0x120, 0, 6, 6, nullptr},
        /* 0x1c */ {0x8e, 0x55, 0, 8, 0, nullptr},
        /* 0x1d */ {0x8e, 0x54, 0, 8, 2, nullptr},
        /* 0x1e */ {0x8e, 0x57, 0, 8, 1, nullptr},
        /* 0x1f */ {0x8e, 0x288, 0, 10, 5, nullptr},
        /* 0x20 */ {0x8c, 0x136, 0x137, 5, 8, nullptr},
        /* 0x21 */ {0x8c, 0x13a, 0x13b, 3, 8, nullptr},
        /* 0x22 */ {0x8c, 0x138, 0x139, 4, 8, nullptr},
        /* 0x23 */ {0x8c, 0x132, 0x133, 2, 8, nullptr},
        /* 0x24 */ {0x8c, 0x134, 0x135, 1, 8, nullptr},
        /* 0x25 */ {0x8c, 0x1a4, 0x1a5, 9, 1, nullptr},
        /* 0x26 */ {0x8c, 0x1a6, 0x1a7, 9, 0, nullptr},
    };
    if (type >= 0 && type <= 0x26) {
        out = k[type];
        return true;
    }
    if (type == 0x49) {
        out = {0, 0x20f, 0, 6, 8, kGreen};
        return true;
    }
    if (type == 0x4a) {
        out = {0, 0x210, 0, 6, 8, kRed};
        return true;
    }
    if (type == 0x4b) {
        out = {0x8c, 0x278, 0x27a, 10, 2, nullptr};
        return true;
    }
    return false;
}

const std::string& S(int id) { return CTxtLoc::get().getS(id); }

}  // namespace

// Ghidra 0x161490
CDialMenu::CDialMenu() {
    mEngine = CEngine::getSingletonPtr();
    mColor = colors::COL_MAGENTA;
}

// Ghidra 0x16151c
CDialMenu::~CDialMenu() = default;

// Ghidra 0x161594
void CDialMenu::init(CInterfaceIngamePerPlayer* ipp, CGameObject* object) {
    mIPP = ipp;
    ipp->getCamera()->mSlowScroll = false;
    mObjectId = object->mId;
}

// Ghidra 0x165c70
void CDialMenu::addButton(int type, int value, const std::string& label) {
    CGame* game = mEngine->mGame;
    const bool f8 = game && game->features().isFeatureEnabled(8);
    const bool f25 = game && game->features().isFeatureEnabled(0x19);
    if (!(f8 || static_cast<unsigned>(type - 0x18) > 1u)) return;
    if (!(f25 || (type & ~2) != 0x15)) return;
    for (const CDialButton& b : mButtons)
        if (b.type == type) return;
    CDialButton b;
    b.type = type;
    b.value = value;
    b.selected = type == 0;
    b.label = label;
    mButtons.push_back(b);
}

// Ghidra 0x1615b0
int CDialMenu::getSelectedButton() const {
    for (const CDialButton& b : mButtons)
        if (b.selected) return b.type;
    return 0;
}

float CDialMenu::layoutRadius(int n, float guiScale) {
    float r = static_cast<float>(static_cast<unsigned>(n)) * 10.0f;
    r = r + 70.0f;   // 0x428c0000
    return guiScale * r;
}

CVector2 CDialMenu::buttonOffset(int k, int n, float r) {
    const float step = CMath::TWO_PI / static_cast<float>(static_cast<unsigned>(n));
    float a = 0.0f;
    for (int i = 0; i < k; ++i) a = a - step;   // the layout loop subtracts step by step
    const float c = CMath::SinTable(CMath::HALF_PI + a), s = CMath::SinTable(a);
    return CVector2(r * c - s * 0.0f, r * s + c * 0.0f);
}

// Ghidra 0x161600
void CDialMenu::timeUpdate() {
    CGame* game = mEngine->mGame;
    if (game)
        if (CGameObject* o = game->getGameObject(mObjectId))
            mCenter = mIPP->worldToPixel(Vec3(o->mPosition.x, o->mPosition.y, o->mPosition.z));
    float offX = 0.0f, offY = 0.0f;
    const float g = mEngine->mGUIScale;
    const float dt = mEngine->mFrameDt;
    const float g64 = g * 64.0f;
    const float marginTop = g64 + 50.0f, marginBottom = g64 + 40.0f, marginLeft = g64 + 80.0f,
                marginRight = g64 + 10.0f;
    const float r = layoutRadius(static_cast<int>(mButtons.size()), g);
    CCameraLevel* cam = mIPP->getCamera();
    float angle = 0.0f;
    for (CDialButton& b : mButtons) {
        const float c = CMath::SinTable(CMath::HALF_PI + angle), s = CMath::SinTable(angle);
        const float bx = mCenter.x + (r * c - s * 0.0f);
        const float by = mCenter.y + (r * s + c * 0.0f);
        b.pos = CVector2(bx, by);
        b.angle = angle;
        angle = angle - CMath::TWO_PI / static_cast<float>(static_cast<unsigned>(mButtons.size()));
        // keep the buttons on screen: nudge the camera (scaled by the frame dt)
        const float w = static_cast<float>(cam->mWidth), h = static_cast<float>(cam->mHeight);
        if (w - marginRight < bx) {
            float v = (bx - w + marginRight) * dt;
            if (offX > v) v = offX;
            offX = v;
        }
        if (marginLeft > bx) {
            const float v = (bx - marginLeft) * dt;
            if (!(v > offX)) offX = v;
        }
        if (h - marginBottom < by) {
            const float v = -((by - h) + marginBottom) * dt;
            if (!(offY < v)) offY = v;
        }
        if (marginTop > by) {
            const float v = -(by - marginTop) * dt;
            if (!(offY > v)) offY = v;
        }
    }
    const float yaw = cam->mYaw;
    const float c = CMath::SinTable(yaw + CMath::HALF_PI), s = CMath::SinTable(yaw);
    cam->mTarget.x = cam->mTarget.x + (offX * c - offY * s);
    cam->mTarget.y = cam->mTarget.y + (offX * s + offY * c);
    cam->refresh();
}

int CDialMenu::buttonIndexForDelta(float dx, float dy, int n) {
    if (n <= 0) return -1;
    const float len = CMath::Sqrt(dx * dx + dy * dy);
    if (len > bitsf(0x322bcc77)) {   // 1e-8
        const float inv = 1.0f / len;
        dx = dx * inv;
        dy = dy * inv;
    }
    float a = CMath::Pi2Pi(CMath::ATan2(-dy, dx));
    if (a < 0.0f) a = a + CMath::TWO_PI;
    const float nf = static_cast<float>(static_cast<unsigned>(n));
    const float half = CMath::TWO_PI / (nf + nf);
    const float k = (half + a) / (CMath::TWO_PI / nf);
    return static_cast<int>(static_cast<unsigned>(f2i(k)) % static_cast<unsigned>(n));
}

// Ghidra 0x1622e0
void CDialMenu::processTouchMove(const CVector2& screenPos) {
    if (mButtons.empty()) return;
    const CVector2 p = mIPP->screenPixelPosToPixelPos(screenPos);
    const int k = buttonIndexForDelta(p.x - mCenter.x, p.y - mCenter.y, static_cast<int>(mButtons.size()));
    for (CDialButton& b : mButtons) b.selected = false;
    mButtons[static_cast<size_t>(k)].selected = true;
}

bool CDialMenu::selectIndex(int i) {
    if (i < 0 || i >= static_cast<int>(mButtons.size())) return false;
    for (CDialButton& b : mButtons) b.selected = false;
    mButtons[static_cast<size_t>(i)].selected = true;
    return true;
}

int CDialMenu::hitTestButton(const CVector2& pixel) const {
    const float half = mEngine->mGUIScale * 128.0f * 0.5f;
    for (size_t i = 0; i < mButtons.size(); ++i) {
        const CVector2& c = mButtons[i].pos;
        if (pixel.x > c.x - half && pixel.x < c.x + half && pixel.y > c.y - half && pixel.y < c.y + half)
            return static_cast<int>(i);
    }
    return -1;
}

// Ghidra 0x165c2c
void CDialMenu::draw() {
    std::memset(mButtonVisible, 0, sizeof(mButtonVisible));
    for (CDialButton& b : mButtons) drawButton(b);
}

// Ghidra 0x1627d8
void CDialMenu::drawButton(CDialButton& b) {
    const float g = mEngine->mGUIScale;
    Vec3 color = b.selected ? mColor : colors::COL_GRAY50;
    std::string text;
    int col = 0, row = 0;
    DialButtonInfo info{};
    if (dialButtonInfo(b.type, info)) {
        if (info.prefix == 0) {
            text = S(info.name);
        } else {
            text = S(info.prefix) + " " + S(info.name);
            if (info.desc) text = text + " : " + S(info.desc);
        }
        if (info.selColor) color = b.selected ? Vec3(info.selColor[0], info.selColor[1], info.selColor[2])
                                              : colors::COL_GRAY50;
        col = info.col;
        row = info.row;
    } else if (b.type >= DIAL_TECH_FIRST && b.type < DIAL_TECH_FIRST + CTechManager::kNumTechs) {
        // research: tech = type - 0x28; the icons are columns 13.. of the atlas
        const int t = b.type - DIAL_TECH_FIRST;
        CTechManager& tm = CTechManager::get();
        text = S(0x20e) + " " + tm.getTechName(t) + ": " + tm.getTechDesc(t);
        col = (t >> 4) + 0xd;
        row = t % 16;
    }
    const CVector2 icon = uiutil::btnUVpos(col, row);
    const CVector2 cell = uiutil::btnUVdefsize();
    const float size = g * 128.0f;
    if (b.selected && mIPP) mIPP->mStatusText = text;
    const CVector2 tl(b.pos.x - size * 0.5f, b.pos.y - size * 0.5f);
    const CVector2 bg = uiutil::btnUVpos(b.value, 3);
    const float shadow = g * 180.0f;
    const CVector2 shadowTL(tl.x - (shadow - size) * 0.5f + g * 7.0f, tl.y - (shadow - size) * 0.5f + g * 7.0f);
    CInterfaceIngame* parent = mIPP->getParent();
    parent->addColored4QuadPixel(shadowTL, uiutil::btnUVpos(9, 4), cell, CVector2(shadow, shadow),
                                 colors::COL_BLACK, 0.8f);
    parent->addColored4QuadPixel(tl, bg, cell, CVector2(size, size), color, 1.0f);
    parent->addColored4QuadPixel(tl, icon, cell, CVector2(size, size), color, 1.0f);
    CTextRender* tr = mEngine->getTextRender();
    auto drawLabel = [&](float along, float across, const std::string& s) {
        const float c = CMath::SinTable(b.angle + CMath::HALF_PI), sn = CMath::SinTable(b.angle);
        const float x = b.pos.x + (along * c - across * sn);
        const float y = b.pos.y + (along * sn + across * c);
        const float a = CMath::Pi2Pi(b.angle);
        if (!tr) return;
        if (a < CMath::HALF_PI && a > -CMath::HALF_PI)
            tr->drawStringPixelBoundedYaw(x, y, g * 22.0f, 1.0f, 1.0f, 0.0f, 1.0f, g * 150.0f, g * -5.0f, 10000, 0,
                                          -a, "%s", s.c_str());
        else
            tr->drawStringPixelBoundedYaw(x, y, g * 22.0f, 1.0f, 1.0f, 0.0f, 1.0f, g * 150.0f, g * -5.0f, 10000, 2,
                                          CMath::PI - a, "%s", s.c_str());
    };
    if (!b.label.empty()) {
        // the label is a format string in the original ("%.0f%%  >>  ..." of the rank buttons)
        char buf[256];
        std::snprintf(buf, sizeof(buf), b.label.c_str());   // NOLINT: the original passes it as the format
        drawLabel(g * 110.0f, mEngine->mBeginnerMode ? g * 40.0f : g * 5.0f, buf);
    }
    if (b.type >= 0 && b.type < kNumDialButtonTypes) {
        mButtonCenters[b.type] = CVector2(size * 0.5f + tl.x, size * 0.5f + tl.y);
        mButtonVisible[b.type] = true;
    }
    if (mEngine->mBeginnerMode) drawLabel(g * 90.0f, g * 5.0f, text);
}

// Ghidra 0x162500: the selected unit, turning, at the centre (production buttons)
void CDialMenu::drawCenterMesh(int selectedButton) {
    unsigned unit = 0;
    switch (selectedButton) {
        case 1: unit = 2; break;
        case 2: unit = 3; break;
        case 3: unit = 5; break;
        case 4: unit = 10; break;
        case 5: unit = 1; break;
        case 6: unit = 6; break;
        case 7: unit = 9; break;
        case 8: unit = 0xc; break;
        case 9: unit = 8; break;
        case 10: unit = 0xe; break;
        case 0xb: unit = 0xd; break;
        case 0xc: unit = 0x10; break;
        case 0x20: unit = 0x15; break;
        case 0x21: unit = 0x13; break;
        case 0x22: unit = 0x14; break;
        case 0x23: unit = 0x12; break;
        case 0x24: unit = 0x11; break;
        case 0x25: unit = 0x16; break;
        case 0x26: unit = 0x17; break;
        case 0x4b: unit = 0x18; break;
        default: return;
    }
    ingamedraw::drawUnitAtScreen(unit, mCenter.x, mCenter.y, mEngine->mRealGameTime, mIPP);
}

}  // namespace py
