// CInterfaceIngamePerPlayerDraw.cpp - the drawing half of the IPP (see the
// header): IPP::draw in the original order (docs/re/scripts_rendering_ui.md
// B.1), the HUD texts and the quad flushes. Render code: plain float.
#include <cstdio>
#include <cstring>

#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/FloatBits.h"
#include "game/CDrawContext.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CGameFeatures.h"
#include "game/CGameObject.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTextRender.h"
#include "render/CTexture.h"
#include "render/Colors.h"
#include "ui/CActionBar.h"
#include "ui/CDialMenu.h"
#include "ui/CEventBar.h"
#include "ui/CGroupBar.h"
#include "ui/CIngameWorldDraw.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CUIUtils.h"

namespace py {

namespace {
constexpr unsigned kTriangles = 4;        // GL_TRIANGLES
constexpr unsigned kTexture0 = 0x84c0;    // GL_TEXTURE0
const float k02 = bitsf(0x3e4ccccd);

bool featureOn(CEngine* e, int f) { return e->mGame && e->mGame->features().isFeatureEnabled(f); }
}  // namespace

// Ghidra 0x1a6e3c
void CInterfaceIngamePerPlayer::draw() {
    CGame* game = mEngine->mGame;
    CTextRender* text = mEngine->getTextRender();
    ingamedraw::setupDrawContext(this);
    mCamera->activateGLviewport();
    ingamedraw::updateVisibility(game);
    if (!mInfoScreenActive) {
        drawSelection();
        if (mDialMenu) mDialMenu->draw();
        if (mEventBar) mEventBar->draw();
        if (mGroupBar) mGroupBar->draw();
        if (mEngine->mBeginnerMode && text) {
            const float g = mEngine->mGUIScale;
            text->drawStringPixel(static_cast<float>(mCamera->mWidth) * 0.5f, g * 45.0f, g * 22.0f, 1.0f, 1.0f, 0.0f,
                                  1.0f, 1, "%s", CTxtLoc::get().getC(0xc2));
        }
    }
    ingamedraw::drawLevel(game);
    drawCircleQuads();
    ingamedraw::drawGame(game);
    if (text) text->draw();
    if (!mCamera->isStategyView()) ingamedraw::drawWeapons();
    ingamedraw::drawParticles(*mCamera);
    drawStandardQuads();
    drawColoredQuads();
    drawScreenSpaceQuadsColored4();
    drawScreenSpaceQuadsColored();
    drawScreenSpaceQuads(false);
    if (text) text->draw();
    if (mEngine->mMinimapSize != 0 && featureOn(mEngine, 7)) {
        ingamedraw::drawMinimap(game);
        mCamera->activateGLviewport();   // [PC] the 5b minimap leaves its viewport set
        if (mEngine->mShowCompass && mPlayer) {
            // the compass rose over the minimap: interface.png (0, 12), 4 x 4 cells
            const float g = mEngine->mGUIScale;
            const CVector2 cell = uiutil::btnUVdefsize();
            const float s = g * 96.0f;
            const Vec3 c(mPlayer->mColor.x * 0.25f, mPlayer->mColor.y * 0.25f, mPlayer->mColor.z * 0.25f);
            mParent->addColored4QuadPixel(
                CVector2(static_cast<float>(mCameraMinimap->mWidth), static_cast<float>(mCamera->mHeight) - s),
                uiutil::btnUVpos(0, 0xc), CVector2(cell.x * 4.0f, cell.y * 4.0f), CVector2(s, s), c, 0.75f);
            drawScreenSpaceQuadsColored4();
        }
    }
    if (mActionBar) mActionBar->draw();
    drawScreenSpaceQuadsColored4();
    // (CAchievementManager::draw: achievements are not ported)
    drawTitleBar();
    drawChatText();
    drawTimer();
    drawOverlayText();
    if (mInfoScreenActive) {
        drawInfoScreen();
        if (text) text->draw();
    }
    if (mRectVisible) {
        drawSelectionRectangle(mRectAlpha * bitsf(0x3ecccccd));
        mRectAlpha = mRectAlpha - mEngine->mFrameDt;
        if (mRectAlpha < 0.0f) mRectVisible = false;
    }
    // (commandManager->drawInfo(cam), vtable +0x54: the network desync info, not ported)
    drawMarker();
    if (mDialMenu) mDialMenu->drawCenterMesh(mDialMenu->getSelectedButton());
    if (text) text->draw();   // [PC] the dial labels queued after the last flush
}

// Ghidra 0x1a3494: the selected objects' drawSelection (vtable +0x48)
void CInterfaceIngamePerPlayer::drawSelection() {
    CGame* game = mEngine->mGame;
    if (!game) return;
    for (unsigned id : mSelection)
        if (CGameObject* o = game->getGameObject(id)) o->drawSelection();
}

// Ghidra 0x1a34e0: the multi-select rectangle (red)
void CInterfaceIngamePerPlayer::drawSelectionRectangle(float alpha) {
    const CVector2 uv = uiutil::uvFromPixel(5.0f, 264.0f), uvs = uiutil::uvFromPixel(1.0f, 1.0f);
    const CVector2 size(mRectMax.x - mRectMin.x, mRectMax.y - mRectMin.y);
    mParent->addColored4QuadPixel(mRectMin, uv, uvs, size, colors::COL_RED, alpha);
    drawScreenSpaceQuadsColored4();
}

// Ghidra 0x1a3848
void CInterfaceIngamePerPlayer::drawTitleBar() {
    const float g = mEngine->mGUIScale;
    CGame* game = mEngine->mGame;
    CTextRender* tr = mEngine->getTextRender();
    const CVector2 uv = uiutil::uvFromPixel(5.0f, 264.0f), uvs = uiutil::uvFromPixel(1.0f, 1.0f);
    const float w = static_cast<float>(mParent->getCamera()->mWidth);
    const float h = g * 40.0f;
    Vec3 c(0.0f, 0.0f, 0.0f);
    if (mPlayer) c = Vec3(mPlayer->mColor.x * 0.25f, mPlayer->mColor.y * 0.25f, mPlayer->mColor.z * 0.25f);
    mParent->addColored4QuadPixel(CVector2(0.0f, 0.0f), uv, uvs, CVector2(w, h), c, 0.5f);
    drawScreenSpaceQuadsColored4();
    if (!tr || !game) return;
    const Vec3 white(1.0f, 1.0f, 1.0f);
    float x = 12.0f;
    if (featureOn(mEngine, 2)) {
        tr->drawStringPixelScaled(2.0f, -1.0f, 23.0f, white, 1.0f, 0, "Sectors:");
        x = 92.0f;
        mSectorsTextPos = CVector2(92.0f, 15.0f);
        float sx = 82.0f;
        if (mEngine->mGameType == 2) {
            // team game: the sectors per team, in the colour of the team's last player
            for (unsigned team = 0; team < mEngine->mNumPlayers + 1; ++team) {
                int n = 0;
                bool any = false;
                Vec3 col;
                for (const auto& kv : game->players()) {
                    const CPlayer* p = kv.second.get();
                    if (p->mTeam != team) continue;
                    any = true;
                    n += p->mOwnedSectors;
                    col = p->mColor;
                }
                if (!any) continue;
                tr->drawStringPixelScaled(sx, -1.0f, 23.0f, col, 1.0f, 0, "%i", n);
                sx = sx + 20.0f;
            }
            x = sx + 10.0f;
        } else if (!game->players().empty()) {
            for (const auto& kv : game->players()) {
                const CPlayer* p = kv.second.get();
                tr->drawStringPixelScaled(sx, -1.0f, 23.0f, p->mColor, 1.0f, 0, "%i", p->mOwnedSectors);
                sx = sx + 20.0f;
            }
            x = sx + 10.0f;
        }
    }
    if (featureOn(mEngine, 1) && mPlayer) {
        mBrainsTextPos = CVector2(x + 60.0f, 15.0f);
        tr->drawStringPixelScaled(x, -1.0f, 23.0f, white, 1.0f, 0, "Brains: %i / %i", mPlayer->getCurrentBrains(),
                                  mPlayer->getBrainCap());
        x = x + 160.0f;
    }
    if (mEngine->mGameType == 3 && mPlayer) {
        tr->drawStringPixelScaled(x, -1.0f, 23.0f, white, 1.0f, 0, "Resources: %.0f",
                                  static_cast<double>(mPlayer->getCurrentResources()));
        x = x + 180.0f;
    }
    tr->drawStringPixelScaled(x, -1.0f, 23.0f, white, 1.0f, 0, "FPS %0.0f", static_cast<double>(mEngine->mFps));
    tr->drawStringPixelScaled(2.0f, 18.0f, 23.0f, white, 1.0f, 0, "%s", mStatusText.c_str());
    tr->draw();
    // the menu button at the right end (no lobby connection: always on the PC)
    const CVector2 cell = uiutil::btnUVdefsize();
    const Vec3 grey(k02, k02, k02);
    mParent->addColored4QuadPixel(CVector2(w - h, 0.0f), uiutil::btnUVpos(6, 4), cell, CVector2(h, h), grey, 1.0f);
    mParent->addColored4QuadPixel(CVector2(w - h, 0.0f), uiutil::btnUVpos(10, 1), cell, CVector2(h, h), grey, 1.0f);
    drawScreenSpaceQuadsColored4();
}

// Ghidra 0x1a12f8: the chat lines (newest at the bottom, fading out after 20 s)
void CInterfaceIngamePerPlayer::drawChatText() {
    const float g = mEngine->mGUIScale;
    const float size = g * 20.0f;
    float y = static_cast<float>(mParent->getCamera()->mHeight) - g * 80.0f;
    CTextRender* tr = mEngine->getTextRender();
    unsigned i = 0;
    for (auto it = mChatLines.rbegin(); it != mChatLines.rend(); ++it, ++i) {
        float a = 2.0f - static_cast<float>(i) * k02;
        if (a > 1.0f) a = 1.0f;
        else if (a < 0.0f) a = 0.0f;
        float f = CMath::Pow(it->age / 20.0f, 4.0f);   // (the exponent is not visible in the decompile)
        f = f > 1.0f ? 0.0f : 1.0f - f;
        if (f < 0.0f) f = 0.0f;
        float alpha = a * f;
        if (alpha < 0.0f) alpha = 0.0f;
        if (i < 10 && tr)
            tr->drawStringPixel(g * 80.0f, y, size, it->color.x, it->color.y, it->color.z, alpha, 0, "%s",
                                it->text.c_str());
        y = y - size;
    }
    for (auto it = mChatLines.begin(); it != mChatLines.end();) {
        it->age = it->age + mEngine->mFrameDt;
        if (it->age > 20.0f) it = mChatLines.erase(it);
        else ++it;
    }
    if (tr) tr->draw();
}

// Ghidra 0x1a35d8: "m:ss" (or "text : m:ss") at the top right
void CInterfaceIngamePerPlayer::drawTimer() {
    if (!(mTimerRuntime > 0.0f)) return;
    char buf[256];
    const float m = CMath::Floor(mTimerRuntime / 60.0f);
    const float s = CMath::Floor(CMath::FMod(mTimerRuntime, 60.0f));
    if (mTimerText.empty()) std::snprintf(buf, sizeof(buf), "%.0f:%02.0f", static_cast<double>(m), static_cast<double>(s));
    else
        std::snprintf(buf, sizeof(buf), "%s : %.0f:%02.0f", mTimerText.c_str(), static_cast<double>(m),
                      static_cast<double>(s));
    const float g = mEngine->mGUIScale;
    if (CTextRender* tr = mEngine->getTextRender())
        tr->drawStringPixel(static_cast<float>(mParent->getCamera()->mWidth) - g * 100.0f, g * 55.0f, g * 26.0f, 1.0f,
                            1.0f, 1.0f, 1.0f, 2, 0.0f, "%s", buf);
}

namespace {
std::string counterName(unsigned type, unsigned subtype) {
    // CUnitFactory::getUnitTypeName / CBuildingFactory::getBuildingTypeName
    // (CBuildingFactory::getBuildingTypeName is not ported: the building type number is shown)
    if (type == 1) return CUnitFactory::getUnitTypeName(subtype);
    if (type == 2) return "Building " + std::to_string(subtype);
    return "";
}
}  // namespace

// Ghidra 0x1a18d0. [!] Never called in the original (IPP::draw does not
// call it); the overlay text shows the counter instead.
void CInterfaceIngamePerPlayer::drawCounter() {
    if (mCounterType == 0) return;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s: %i / %i", counterName(mCounterType, mCounterSubtype).c_str(),
                  static_cast<int>(mCounterCount), static_cast<int>(mCounterRequired));
    const float g = mEngine->mGUIScale;
    if (CTextRender* tr = mEngine->getTextRender())
        tr->drawStringPixel(g * 100.0f, g * 55.0f, g * 26.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0.0f, "%s", buf);
}

// Ghidra 0x1a2484: the speaker text, typed out at 100 characters per second
void CInterfaceIngamePerPlayer::drawOverlayText() {
    if (mInfoScreenActive) return;
    std::string cmd;
    if (mOverlayTTL != -1.0f) {
        cmd = mOverlayNoPause ? "Touch anywhere." : CTxtLoc::get().getC(0x2c);
    } else {
        cmd = mOverlayCmdText;
        if (mCounterType != 0) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), ": %s: %i / %i", counterName(mCounterType, mCounterSubtype).c_str(),
                          static_cast<int>(mCounterCount), static_cast<int>(mCounterRequired));
            cmd += buf;
        }
        if (mOverlayNoPause && !mOverlayMinimized) {
            cmd += "  ";
            cmd += CTxtLoc::get().getC(0x1b8);
        }
    }
    if (!mOverlayDisplayed) return;
    const float g = mEngine->mGUIScale;
    const float iconOffset = 0.0f;   // (avatar icons need CCampaign: not ported)
    const int total = static_cast<int>(mOverlayText.size() + cmd.size());
    int chars = f2i(mOverlayRuntime * 100.0f);
    if (chars > total) chars = total;
    mOverlayFullyDisplayed = chars == total;
    CTextRender* tr = mEngine->getTextRender();
    const float w = static_cast<float>(mCamera->mWidth);
    const float h = static_cast<float>(mCamera->mHeight);
    float y = g * 50.0f;
    if (tr) {
        if (!mOverlayMinimized)
            y = tr->drawStringPixelBounded(g * 100.0f + iconOffset, y, g * 30.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                           w - g * 200.0f - iconOffset, g * 4.0f, mOverlayText, chars);
        int rest = chars - static_cast<int>(mOverlayText.size());
        if (rest < 0) rest = 0;
        y = tr->drawStringPixelBounded(g * 100.0f + iconOffset, y, g * 30.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                                       w - g * 200.0f - iconOffset, g * 4.0f, cmd, rest);
    }
    const CVector2 uv = uiutil::uvFromPixel(5.0f, 264.0f), uvs = uiutil::uvFromPixel(1.0f, 1.0f);
    if (!mOverlayNoPause || !mOverlayMinimized) {
        // the screen below the title bar dims
        const float a = (!mOverlayNoPause || mOverlayMinimized) ? bitsf(0x3f333333) : bitsf(0x3ecccccd);
        mParent->addColored4QuadPixel(CVector2(0.0f, g * 40.0f), uv, uvs, CVector2(w, h - g * 40.0f),
                                      colors::COL_BLACK, a);
        drawScreenSpaceQuadsColored4();
    }
    if (mOverlayNoPause) {
        // the box behind the text
        const float a = mOverlayMinimized ? bitsf(0x3f333333) : 0.5f;
        mParent->addColored4QuadPixel(CVector2(g * 80.0f, g * 45.0f), uv, uvs,
                                      CVector2(w - g * 160.0f, y - g * 45.0f), colors::COL_BLACK, a);
        drawScreenSpaceQuadsColored4();
    }
    if (tr) tr->draw();
}

// Ghidra 0x1a21c4: the object-info screen. The texts so far are flushed, the
// screen below the title bar is darkened (black, alpha 0.65), then the
// object is drawn again in front of it (gameobjects.3DS.mesh with the object
// shader bound, its draw (vtable +0x10) with (false, 1)) and its
// drawInfoscreen (vtable +0x94) writes its texts from (10, 50) x GUI scale,
// flushed at once. The camera has zoomed onto the object (startObjectInfo
// and timeUpdate), and the world drawing skipped it (CDrawContext).
void CInterfaceIngamePerPlayer::drawInfoScreen() {
    CTextRender* tr = mEngine->getTextRender();
    if (tr) tr->draw();
    const float g = mEngine->mGUIScale;
    const float w = static_cast<float>(mParent->getCamera()->mWidth);
    const float h = static_cast<float>(mParent->getCamera()->mHeight);
    mParent->addColored4QuadPixel(CVector2(0.0f, g * 40.0f), uiutil::uvFromPixel(5.0f, 264.0f),
                                  uiutil::uvFromPixel(1.0f, 1.0f), CVector2(w, h - g * 40.0f), colors::COL_BLACK,
                                  bitsf(0x3f266666));
    drawScreenSpaceQuads(false);
    drawScreenSpaceQuadsColored4();
    CGame* game = mEngine->mGame;
    CGameObject* o = game ? game->getGameObject(mInfoScreenObject) : nullptr;
    if (!o) return;
    if (game->mGameObjectsMesh && game->mObjectShader) {
        game->mGameObjectsMesh->bindBuffer();
        game->mObjectShader->startDraw();
        if (game->mObjectTexture) game->mObjectTexture->bind(kTexture0);
        o->draw(false, 1.0f);
        game->mObjectShader->endDraw();
    }
    CVector2 pos(g * 10.0f, g * 50.0f);
    o->drawInfoscreen(pos);
    if (tr) tr->draw();
}

// Ghidra 0x1a5a58: the tutorial arrow pointing at a HUD element or object
void CInterfaceIngamePerPlayer::drawMarker() {
    if (!mMarkerEnabled) return;
    CVector2 target(0.0f, 0.0f);
    auto ab = [&](std::initializer_list<int> types) {
        // the last visible one of the action bar buttons
        bool found = false;
        if (!mActionBar) return false;
        for (int t : types)
            if (mActionBar->mButtonVisible[t]) {
                target = mActionBar->mButtonCenters[t];
                found = true;
            }
        return found;
    };
    auto abFirst = [&](std::initializer_list<int> types) {
        // the first visible one, in order
        if (!mActionBar) return false;
        for (int t : types)
            if (mActionBar->mButtonVisible[t]) {
                target = mActionBar->mButtonCenters[t];
                return true;
            }
        return false;
    };
    switch (mMarkerType) {
        case 1: target = mBrainsTextPos; break;
        case 2: target = mSectorsTextPos; break;
        case 3: if (!ab({0})) return; break;
        case 4: if (!mGroupBar) return; target = mGroupBar->mFirstGroupPos; break;
        case 5: if (!mEventBar) return; target = mEventBar->mFirstEventPos; break;
        case 6: {
            CGame* game = mEngine->mGame;
            CGameObject* o = game ? game->getGameObject(mMarkerObject) : nullptr;
            if (!o) return;
            target = worldToPixel(Vec3(o->mPosition.x, o->mPosition.y, 0.0f));
            break;
        }
        case 7:
            if (mEngine->mMinimapSize == 0 || !featureOn(mEngine, 7)) return;
            target = mMinimapCenter;
            break;
        case 8: if (!abFirst({0x1d, 0x1c, 0x1e})) return; break;
        case 9: if (!ab({0x20})) return; break;
        case 10:
            if (!ab({0x15, 7, 0x13, 0x16, 0xf, 0x14, 0xb, 0x10, 0xc, 5, 8, 0x11, 0x12, 6, 9, 0xe, 10, 0xd, 0x17, 0x18,
                     0x19}))
                return;
            break;
        case 0xb: if (!ab({1})) return; break;
        case 0xc: if (!ab({0x1f})) return; break;
        case 0xd: if (!ab({0x1a})) return; break;
        case 0xe: if (!abFirst({0x24, 2, 3})) return; break;
        case 0xf: if (!abFirst({0x23, 0x22})) return; break;
        case 0x10: if (!ab({4})) return; break;
        case 0x11: if (!ab({0x1b})) return; break;
        case 0x13:
            if (!mDialMenu) return;
            if (mDialMenu->mButtonVisible[0x15]) target = mDialMenu->mButtonCenters[0x15];
            else if (mDialMenu->mButtonVisible[0x17]) target = mDialMenu->mButtonCenters[0x17];
            else return;
            break;
        default: return;
    }
    const float g = mEngine->mGUIScale;
    const CVector2 center(static_cast<float>(mCamera->mWidth) * 0.5f, static_cast<float>(mCamera->mHeight) * 0.5f);
    const CVector2 cell = uiutil::btnUVdefsize();
    const CVector2 icon = uiutil::btnUVpos(4, 2);
    const float angle = CMath::ATan2(target.y - center.y, target.x - center.x);
    CVector2 hit;
    const bool offScreen = uiutil::checkSegmentRectangleHit(
        center, target, CVector2(0.0f, -2.0f),
        CVector2(static_cast<float>(mCamera->mWidth), static_cast<float>(mCamera->mHeight)), hit);
    float s;
    CVector2 at;
    if (!offScreen) {
        s = g * 100.0f;
        at = CVector2(target.x - s * 0.5f, target.y - s * 0.5f);
    } else {
        s = g * 0.5f * 128.0f;
        at = CVector2(hit.x - s * 0.5f, hit.y - s * 0.5f);
    }
    const float a = CMath::PI * 0.75f + angle;
    const float c = CMath::SinTable(a + CMath::HALF_PI), sn = CMath::SinTable(a);
    const CVector2 p(at.x + (s * c - s * sn) * 0.25f, at.y + (s * sn + s * c) * 0.25f);
    const float pulse = CMath::SinTable(mEngine->mRealGameTime * 5.0f) * 0.5f + 0.5f;
    mParent->addColored4QuadPixel(CVector2(p.x + g * 5.0f, p.y + g * 5.0f), icon, cell, CVector2(s, s),
                                  colors::COL_BLACK, pulse * 0.25f, a);
    const Vec3 m(colors::COL_MAGENTA.x * 1.5f, colors::COL_MAGENTA.y * 1.5f, colors::COL_MAGENTA.z * 1.5f);
    mParent->addColored4QuadPixel(p, icon, cell, CVector2(s, s), m, pulse + pulse, a);
    drawScreenSpaceQuadsColored4();
}

// ---------------------------------------------------------------------------
// quad flushes

// Ghidra 0x1a16cc
void CInterfaceIngamePerPlayer::drawCircleQuads() {
    std::vector<CInterfaceIngame::CCircleInfo>& circles = mParent->mWorldQuads.circles();
    if (gl::isLoaded() && !circles.empty()) {
        mParent->startDrawCircle();
        for (const auto& c : circles) mParent->drawCircle(c, mCamera);
        mParent->endDrawCircle();
    }
    circles.clear();
}

// Ghidra 0x1a405c
void CInterfaceIngamePerPlayer::drawStandardQuads() {
    mParent->mWorldQuads.drawStandardQuads(mCamera->viewProjMatrix());
    mParent->mSelectionQuadZ = 0.0f;
}

// Ghidra 0x1a17d0
void CInterfaceIngamePerPlayer::drawColoredQuads() { mParent->mWorldQuads.drawColoredQuads(mCamera->viewProjMatrix()); }

// Ghidra 0x1a1750
void CInterfaceIngamePerPlayer::drawColored4Quads() {
    mParent->mWorldQuads.drawColored4Quads(mCamera->viewProjMatrix());
}

// Ghidra 0x1a1850 (CGame::drawMinimap calls the CIngameQuads version)
void CInterfaceIngamePerPlayer::drawColoredQuadsMinimap(CCamera*) {
    if (mMinimapDraw) mParent->mWorldQuads.drawColoredQuadsMinimap(mMinimapDraw->viewProjMatrix());
}

// the ortho matrix of the screen-space flushes (rotation 0): x / aspect, y, z by near/far
Mat4 CInterfaceIngamePerPlayer::screenSpaceMatrix() const { return mCamera->textOrthoMatrix(); }

// Ghidra 0x1a1aa0
void CInterfaceIngamePerPlayer::drawScreenSpaceQuads(bool keepTexture) {
    CInterfaceIngame* p = mParent;
    if (p->mNumScreenQuads <= 0) return;
    if (gl::isLoaded() && p->mSelectShader) {
        gl::DepthMask(0);
        gl::Disable(0x0B71);
        p->mSelectShader->startDrawLocalPosUVBlend(&p->mScreenQuads[0].v[0]);
        if (!keepTexture && p->mInterfaceTexture) p->mInterfaceTexture->bind(kTexture0);
        p->mSelectShader->setUniformMVPMatrix(screenSpaceMatrix());
        CGLSLProgram::drawLocal(kTriangles, 0, p->mNumScreenQuads * 6);
        p->mSelectShader->endDrawLocalPosUVBlend();
        gl::DepthMask(1);
        gl::Enable(0x0B71);
    }
    p->mNumScreenQuads = 0;
}

// Ghidra 0x1a30f8
void CInterfaceIngamePerPlayer::drawScreenSpaceQuadsColored() {
    CInterfaceIngame* p = mParent;
    if (p->mNumColoredScreenQuads <= 0) return;
    if (gl::isLoaded() && p->mColoredShader) {
        gl::DepthMask(0);
        gl::Disable(0x0B71);
        p->mColoredShader->startDrawLocalPosUVColorBlendNoDepth(&p->mColoredScreenQuads[0].v[0]);
        if (p->mInterfaceTexture) p->mInterfaceTexture->bind(kTexture0);
        p->mColoredShader->setUniformMVPMatrix(screenSpaceMatrix());
        CGLSLProgram::drawLocal(kTriangles, 0, p->mNumColoredScreenQuads * 6);
        p->mColoredShader->endDrawLocalPosUVColorBlendNoDepth();
        gl::DepthMask(1);
        gl::Enable(0x0B71);
    }
    p->mNumColoredScreenQuads = 0;
}

// Ghidra 0x1a1e4c
void CInterfaceIngamePerPlayer::drawScreenSpaceQuadsColored4() {
    CInterfaceIngame* p = mParent;
    if (p->mNumColored4ScreenQuads <= 0) return;
    if (gl::isLoaded() && p->mColoredAlphaShader) {
        p->mColoredAlphaShader->startDrawLocalPosUVColor4BlendNoDepth(&p->mColored4ScreenQuads[0].v[0]);
        if (p->mInterfaceTexture) p->mInterfaceTexture->bind(kTexture0);
        p->mColoredAlphaShader->setUniformMVPMatrix(screenSpaceMatrix());
        CGLSLProgram::drawLocal(kTriangles, 0, p->mNumColored4ScreenQuads * 6);
        p->mColoredAlphaShader->endDrawLocalPosUVColor4BlendNoDepth();
    }
    p->mNumColored4ScreenQuads = 0;
}

}  // namespace py
