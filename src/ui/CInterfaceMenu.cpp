// CInterfaceMenu.cpp - the menu scene, input dispatch, quad buffers and the
// game start (see CInterfaceMenu.h; processAction is in
// CInterfaceMenuActions.cpp, the screens in CInterfaceMenuScreens.cpp).
#include "CInterfaceMenu.h"

#include <cstdio>

#include "core/CConfig.h"
#include "core/FloatBits.h"
#include "core/CRng.h"
#include "game/CCampaign.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CParticleManager.h"
#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTextRender.h"
#include "render/CTexture.h"
#include "render/Colors.h"
#include "ui/CInterfaceElement.h"
#include "ui/CInterfaceElementImageSlider.h"
#include "ui/CInterfaceElementTextInput2.h"
#include "ui/CInterfaceElementTextSlider2.h"

#include "ui/CInterfaceIngame.h"

namespace py {

namespace {
constexpr unsigned GL_DEPTH_TEST_ = 0x0B71, GL_CULL_FACE_ = 0x0B44, GL_BLEND_ = 0x0BE2, GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_COLOR_DEPTH_ = 0x4100, GL_ONE_ = 1, GL_SRC_ALPHA_ = 0x0302, GL_TRIANGLES_ = 4;
constexpr int kMaxColoredQuads = 1999;   // addColoredQuad: count < 1999
constexpr int kMaxPixelQuads = 999;      // addStandardQuadPixel / addColored4QuadPixel: count < 999
}  // namespace

// Ghidra 0x1b07dc
CInterfaceMenu::CInterfaceMenu() {
    mType = TYPE_MENU;   // +0x34 = 0
    mEngine = CEngine::getSingletonPtr();
    mTop10Option = 2;
    mDownloadPollTime = 0.0f;
    mBackAction = 0;
    mForceSetup = false;
    mDownloadInProgress = false;
    mModalState = 0;
    mTouchedElement = nullptr;
}

// Ghidra 0x1b0b9c
CInterfaceMenu::~CInterfaceMenu() = default;

void CInterfaceMenu::clearElements() {
    mTouchedElement = nullptr;
    mActiveTextInput = nullptr;
    mElements.clear();
}

// Ghidra 0x1cd118
void CInterfaceMenu::init(int menuType) {
    mEngine = CEngine::getSingletonPtr();
    mCamera = std::make_unique<CCameraMenu>();
    mNextMenu = menuType;
    mStartGameMode = 0;   // +0x7c
    mDrawMode = 0;
    mBackground = CTextureManager::get().getTexture("background.png", 1, 2, 2, true);
    mCreditsBlack = CTextureManager::get().getTexture("credits_black.png", 1, 2, 2, true);
    mMesh = CMeshManager::get().getMesh("menu.3DS.mesh", true, false);
    mProgramPlayerColorMenu = CGLSLProgramManager::get().getGLSLProgram("playercolormenu.gfx");
    mProgramSimple = CGLSLProgramManager::get().getGLSLProgram("simple.gfx");
    if (mProgramSimple) mProgramSimple->setupUniformTextureUnit("DIFFUSE", 0);
    mProgramSimpleAlpha = CGLSLProgramManager::get().getGLSLProgram("simplealpha.gfx");
    if (mProgramSimpleAlpha) mProgramSimpleAlpha->setupUniformTextureUnit("DIFFUSE", 0);
    mColoredQuads.clear();
    mColoredQuads.reserve(kMaxColoredQuads);   // 0x5dc00 bytes = 2000 quads
    mProgramInterfaceColored = CGLSLProgramManager::get().getGLSLProgram("interfacecolored.gfx");
    if (mProgramInterfaceColored) mProgramInterfaceColored->setupUniformTextureUnit("DIFFUSE", 0);
    mInterfaceTexture = CTextureManager::get().getTexture("interface.png", 1, 2, 2, false);
    mProgramSelect = CGLSLProgramManager::get().getGLSLProgram("select.gfx");
    if (mProgramSelect) mProgramSelect->setupUniformTextureUnit("DIFFUSE", 0);
    mStandardQuads.clear();
    mStandardQuads.reserve(kMaxPixelQuads);
    mProgramInterfaceColoredAlpha = CGLSLProgramManager::get().getGLSLProgram("interfacecoloredalpha.gfx");
    if (mProgramInterfaceColoredAlpha) mProgramInterfaceColoredAlpha->setupUniformTextureUnit("DIFFUSE", 0);
    mColor4Quads.clear();
    mColor4Quads.reserve(kMaxPixelQuads);
    setupElements(mNextMenu);
}

CCamera* CInterfaceMenu::getCamera() { return mCamera.get(); }

// Ghidra 0x1ad488
void CInterfaceMenu::setResolution(int width, int height) {
    mHeight = height;
    mWidth = width;
    mCamera->initGLviewport(width, height, 0, 0, 0);
    mCamera->setZoomForAspect();
}

// Ghidra 0x1ad558: the download poll of screen 13 is not ported (no downloads on PC)
void CInterfaceMenu::timeUpdate(float dt) {
    mCamera->timeUpdate(dt);
    if (CParticleManager* pm = mEngine->getParticleManager()) pm->timeUpdate(dt);
}

// Ghidra 0x1ad5f4
void CInterfaceMenu::processEventActionDown(CInterfaceEvent* e) {
    if (mModalState != 0) return;
    for (auto& el : mElements) {
        if (el->isHit(e)) {
            el->processEventActionDown(e);
            mTouchedElement = el.get();
            return;
        }
    }
}

// Ghidra 0x1ad670
void CInterfaceMenu::processEventActionMove(CInterfaceEvent* e) {
    if (mModalState != 0) return;
    for (size_t i = 0; i < mElements.size(); ++i)
        if (mElements[i].get() == mTouchedElement) mElements[i]->processEventActionMove(e);
}

// Ghidra 0x1d5a88. The corner button (64 x 64 GUI-scaled pixels at (2, 2))
// opens the lobby chat (not connected on PC: nothing). A tap that no element
// takes makes a "clickfail" (subtype 5) select particle, else "click" (4).
void CInterfaceMenu::processEventActionUp(CInterfaceEvent* e) {
    if (mModalState != 0) return;
    if (mDrawMode == 3) {
        mDrawMode = 0;
        processAction(0x17, nullptr);
        mForceSetup = false;
        setupElements(mNextMenu, 0);
        mTouchedElement = nullptr;
        return;
    }
    const CVector2 world = mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    const float g = mEngine->mGUIScale;
    const float lo = g + g;
    const float hi = g * 64.0f + lo;
    const bool corner = e->x > lo && e->x < hi && e->y > lo && e->y < hi;
    int subtype = 5;
    if (corner) {
        // CEngine::isLobbyClientConnected: the lobby is not ported (offline)
        subtype = 4;
    } else {
        for (size_t i = 0; i < mElements.size(); ++i) {
            CInterfaceElement* el = mElements[i].get();
            if (el == mTouchedElement && el->processEventActionUp(e) != 0) {
                subtype = 4;
                break;
            }
            if (i >= mElements.size()) break;   // [PC] processAction may have rebuilt the list
        }
    }
    if (CParticleManager* pm = mEngine->getParticleManager())
        pm->createParticleSystem(2, world, 0.0f, subtype, true, nullptr, true);
    // TODO(sound): CSoundManager::playSound(subtype == 4 ? "click" : "clickfail")
    if (mNextMenu != mCurrentMenu || mForceSetup) {
        mForceSetup = false;
        setupElements(mNextMenu, 0);
    }
    mTouchedElement = nullptr;
}

// Ghidra 0x1d59e8
void CInterfaceMenu::processEventBackKeyPressed(CInterfaceEvent*) {
    if (mModalState != 0) return;
    processBackButton();
    if (mNextMenu != mCurrentMenu || mForceSetup) {
        mForceSetup = false;
        setupElements(mNextMenu, 0);
    }
}

// Ghidra 0x1d5a38 (same as Back)
void CInterfaceMenu::processEventMenuKeyPressed(CInterfaceEvent* e) { processEventBackKeyPressed(e); }

// Ghidra 0x1d3b04
void CInterfaceMenu::processEventMenuAction(CInterfaceEvent* e) { processAction(static_cast<int>(e->pointer), nullptr); }

// Ghidra 0x1d3ae8: the screen's back action (+0x5c0, set by setupElements;
// the main menu's is 6 = Exit, so Back there quits, as on Android)
void CInterfaceMenu::processBackButton() {
    if (mModalState != 0) return;
    if (mBackAction == 0) return;
    processAction(mBackAction, nullptr);
}

// Ghidra 0x1ad9b4: the answer of Android's text dialog. Types 3/4 (chat) go
// to the lobby (not ported); 5 is the active CInterfaceElementTextInput2.
void CInterfaceMenu::processTextInput(std::string text, bool done) {
    if (mModalState != 0) return;
    if (mTextInput) mTextInput->processTextInput(text, done);
}

// [PC] keyboard for the text field being edited; Escape there cancels the
// edit instead of acting as Back.
void CInterfaceMenu::processEventKeyDown(CInterfaceEvent* e) {
    mKeyConsumed = false;
    if (!mActiveTextInput) return;
    mKeyConsumed = true;
    if (e->keycode == KEY_RETURN || e->keycode == 0x40000058 /* SDLK_KP_ENTER */) mActiveTextInput->endEdit(true);
    else if (e->keycode == KEY_ESCAPE || e->keycode == KEY_AC_BACK) mActiveTextInput->endEdit(false);
    else if (e->keycode == KEY_BACKSPACE) mActiveTextInput->editBackspace();
    if (mNextMenu != mCurrentMenu || mForceSetup) {
        mForceSetup = false;
        setupElements(mNextMenu, 0);
    }
}

void CInterfaceMenu::processEventTextInput(CInterfaceEvent* e) {
    if (mActiveTextInput) mActiveTextInput->editText(e->text);
}

// [PC] the wheel over a slider: TextSlider2 -> next/previous value (no wrap),
// image sliders -> next entry (as a tap)
void CInterfaceMenu::processEventMouseWheel(CInterfaceEvent* e) {
    if (mModalState != 0 || e->wheelY == 0.0f) return;
    for (size_t i = 0; i < mElements.size(); ++i) {
        CInterfaceElement* el = mElements[i].get();
        if (el->mDisabled || !el->isHit(e)) continue;
        if (auto* ts = dynamic_cast<CInterfaceElementTextSlider2*>(el)) {
            if (e->wheelY > 0.0f) ts->elementInc(false);
            else ts->elementDec();
        } else if (auto* is = dynamic_cast<CInterfaceElementImageSlider*>(el)) {
            if (e->wheelY > 0.0f) is->processEventActionUp(e);
        } else {
            continue;
        }
        break;
    }
    if (mNextMenu != mCurrentMenu || mForceSetup) {
        mForceSetup = false;
        setupElements(mNextMenu, 0);
    }
}

// ---------------------------------------------------------------------------
// Quads

// Ghidra 0x1ae0ec (yaw = the camera's +0x19c, which the menu camera leaves 0)
void CInterfaceMenu::addColoredQuad(const Vec3& center, const CVector2& uv, const CVector2& uvSize,
                                    const CVector2& half, const Vec3& color) {
    if (static_cast<int>(mColoredQuads.size()) >= kMaxColoredQuads) return;
    mColoredQuads.emplace_back(CVector3(center.x, center.y, center.z), half, uv, uvSize,
                                CVector3(color.x, color.y, color.z), 0.0f);
}

// Ghidra 0x1ae464
void CInterfaceMenu::drawColoredQuads() {
    if (mColoredQuads.empty()) return;
    if (mProgramInterfaceColored && gl::isLoaded()) {
        gl::Disable(GL_CULL_FACE_);
        mProgramInterfaceColored->startDrawLocalPosUVColorBlendNoDepth(&mColoredQuads[0].v[0]);
        if (mInterfaceTexture) mInterfaceTexture->bind(GL_TEXTURE0_);
        mProgramInterfaceColored->setUniformMVPMatrix(mCamera->getViewProjMatrix());
        CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, static_cast<int>(mColoredQuads.size()) * 6);
        mProgramInterfaceColored->endDrawLocalPosUVColorBlendNoDepth();
        gl::Enable(GL_CULL_FACE_);
    }
    mColoredQuads.clear();
}

namespace {
// The pixel -> screen conversion of addStandardQuadPixel/addColored4QuadPixel
// at orientation 0: x' = aspect ((x + x) / W - 1), y' = -2y / H + 1, size
// ((w + w) / W * aspect, (h + h) / H). The quad's corner is its top-left.
void pixelToScreen(const CCamera& cam, const CVector2& pos, const CVector2& size, CVector2& outPos,
                   CVector2& outSize) {
    const float w = static_cast<float>(cam.getWidth());
    const float h = static_cast<float>(cam.getHeight());
    const float aspect = cam.getAspect();
    outSize = CVector2((size.x + size.x) / w * aspect, (size.y + size.y) / h);
    outPos = CVector2(aspect * ((pos.x + pos.x) / w - 1.0f), (pos.y * -2.0f) / h + 1.0f);
}
}  // namespace

// Ghidra 0x1ae17c
void CInterfaceMenu::addStandardQuadPixel(const CVector2& pos, const CVector2& uv, const CVector2& uvSize,
                                          const CVector2& size) {
    if (static_cast<int>(mStandardQuads.size()) >= kMaxPixelQuads) return;
    CVector2 p, s;
    pixelToScreen(*mCamera, pos, size, p, s);
    mStandardQuads.emplace_back(p, s, uv, uvSize);
}

// Ghidra 0x1b017c
void CInterfaceMenu::addColored4QuadPixel(const CVector2& pos, const CVector2& uv, const CVector2& uvSize,
                                          const CVector2& size, const Vec3& color, float alpha) {
    if (static_cast<int>(mColor4Quads.size()) >= kMaxPixelQuads) return;
    CVector2 p, s;
    pixelToScreen(*mCamera, pos, size, p, s);
    mColor4Quads.emplace_back(p, s, uv, uvSize, CVector3(color.x, color.y, color.z), alpha, 0.0f);
}

// Ghidra 0x1ae4e0: select.gfx, depth off, the camera's text matrix (x / aspect)
void CInterfaceMenu::drawScreenSpaceQuads() {
    if (mStandardQuads.empty()) return;
    if (mProgramSelect && gl::isLoaded()) {
        gl::DepthMask(0);
        gl::Disable(GL_DEPTH_TEST_);
        mProgramSelect->startDrawLocalPosUVBlend(&mStandardQuads[0].v[0]);
        mProgramSelect->setUniformMVPMatrix(mCamera->textOrthoMatrix());
        CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, static_cast<int>(mStandardQuads.size()) * 6);
        mProgramSelect->endDrawLocalPosUVBlend();
        gl::DepthMask(1);
        gl::Enable(GL_DEPTH_TEST_);
    }
    mStandardQuads.clear();
}

// Ghidra 0x1b047c
void CInterfaceMenu::drawScreenSpaceQuadsColored4() {
    if (mColor4Quads.empty()) return;
    if (mProgramInterfaceColoredAlpha && gl::isLoaded()) {
        mProgramInterfaceColoredAlpha->startDrawLocalPosUVColor4BlendNoDepth(&mColor4Quads[0].v[0]);
        if (mInterfaceTexture) mInterfaceTexture->bind(GL_TEXTURE0_);
        mProgramInterfaceColoredAlpha->setUniformMVPMatrix(mCamera->textOrthoMatrix());
        CGLSLProgram::drawLocal(GL_TRIANGLES_, 0, static_cast<int>(mColor4Quads.size()) * 6);
        mProgramInterfaceColoredAlpha->endDrawLocalPosUVColor4BlendNoDepth();
    }
    mColor4Quads.clear();
}

// ---------------------------------------------------------------------------
// Draw (Ghidra 0x1d3b10)

namespace {
void drawBackground(CInterfaceMenu& m) {
    m.mCamera->activateGLviewport();
    m.mMesh->bindBuffer();
    if (m.mBackground) m.mBackground->bind(GL_TEXTURE0_);
    if (m.mProgramSimple) {
        m.mProgramSimple->startDraw();
        gl::Disable(GL_DEPTH_TEST_);
        gl::DepthMask(0);
        gl::Disable(GL_CULL_FACE_);
        m.mProgramSimple->setUniformMVPMatrix(m.mCamera->getViewProjMatrix());
        drawSubMesh(m.mMesh, "background");
        m.mProgramSimple->endDraw();
    }
    gl::Enable(GL_DEPTH_TEST_);
    gl::Enable(GL_CULL_FACE_);
    gl::DepthMask(1);
}
}  // namespace

void CInterfaceMenu::draw() {
    if (!gl::isLoaded() || !mMesh) return;
    CTextRender* tr = mEngine->getTextRender();
    const float g = mEngine->mGUIScale;
    const float w = static_cast<float>(mCamera->getWidth());
    const float h = static_cast<float>(mCamera->getHeight());
    const CTxtLoc& t = CTxtLoc::get();
    switch (mDrawMode) {
        case 0: {
            gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl::Clear(GL_COLOR_DEPTH_);
            drawBackground(*this);
            for (size_t i = 0; i < mElements.size(); ++i) mElements[i]->draw();
            drawColoredQuads();
            mMesh->bindBuffer();
            if (mProgramPlayerColorMenu) {
                mProgramPlayerColorMenu->startDraw();
                mProgramPlayerColorMenu->setUniformMVPMatrix(mCamera->getViewProjMatrix());
                gl::Enable(GL_BLEND_);
                gl::BlendFunc(GL_ONE_, GL_ONE_);
                mProgramPlayerColorMenu->setUniformAlpha(0.8f);
                mProgramPlayerColorMenu->setUniformPlayerColor(mEngine->mMenuColorR);
                drawSubMesh(mMesh, "menur");
                mProgramPlayerColorMenu->setUniformPlayerColor(mEngine->mMenuColorG);
                drawSubMesh(mMesh, "menug");
                mProgramPlayerColorMenu->setUniformPlayerColor(mEngine->mMenuColorB);
                drawSubMesh(mMesh, "menub");
                gl::BlendFunc(GL_SRC_ALPHA_, GL_ONE_);
                for (size_t i = 0; i < mElements.size(); ++i) mElements[i]->drawMesh();
                mProgramPlayerColorMenu->endDraw();
                gl::Disable(GL_BLEND_);
            }
            if (tr) tr->drawDirect(mCamera->getViewProjMatrix());
            // TODO(particles): CParticleManager::draw(camera, true) - the particle renderer is not ported yet
            if (mCurrentMenu != MENU_LOBBYCHAT) {
                // the lobby chat corner button; the lobby is never connected on PC: the grey icon
                const CVector2 uvSize(bitsf(0x3d800000), bitsf(0x3d800000));
                const float cell = 64.0f * bitsf(0x3a800000);
                const float icon = g * 64.0f;
                const float frame = g * 105.0f;
                const CVector2 pos(g + g, g + g);
                const float inset = (frame - icon) * 0.5f;
                const CVector2 framePos(pos.x - inset + g * 5.0f, pos.y - inset + g * 5.0f);
                const Vec3 grey(bitsf(0x3e4ccccd), bitsf(0x3e4ccccd), bitsf(0x3e4ccccd));
                addColored4QuadPixel(framePos, CVector2(10.0f * cell, 4.0f * cell), uvSize, CVector2(frame, frame),
                                     colors::COL_BLACK, 1.0f);
                addColored4QuadPixel(pos, CVector2(6.0f * cell, 4.0f * cell), uvSize, CVector2(icon, icon), grey, 1.0f);
                addColored4QuadPixel(pos, CVector2(10.0f * cell, 1.0f * cell), uvSize, CVector2(icon, icon), grey,
                                     1.0f);
                drawScreenSpaceQuadsColored4();
            }
            if (tr) tr->draw();
            if (mModalState != 0 && tr) {
                if (mModalState == 1) {
                    // a campaign check (CCampaign::startInitAsync) - [PC] done synchronously: completes at once
                    tr->drawStringPixel(w * 0.5f, h - g * 32.0f, g * 32.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1, t.getC(0x1c5),
                                        100.0);
                    tr->draw();
                    mModalState = 0;
                    CUtils::log("long op complete: mWaitForLongOperation: %i", 0);
                    processAction(mActionAfterLongOp, nullptr);
                    CUtils::log("long op complete: mActionAfterLongOperation: %i", mActionAfterLongOp);
                    mForceSetup = false;
                    setupElements(mNextMenu, 0);
                } else {
                    tr->drawStringPixel(w * 0.5f, h - g * 32.0f, g * 32.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1, "%s",
                                        t.getC(0x256));
                    tr->draw();
                }
            }
            break;
        }
        case 1: {
            gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl::Clear(GL_COLOR_DEPTH_);
            drawBackground(*this);
            if (tr) {
                const char* starting = t.getC(mStartGameMode == 3 ? 0x72 : 0x71);
                const float big = g * 32.0f;
                switch (mEngine->mGameType) {
                    case 0:
                    case 2:
                    case 3: {
                        static const int kDesc[4] = {0x287, 0, 0x286, 0x285};
                        static const int kName[4] = {0xd3, 0, 0x26f, 0x271};
                        const int gt = mEngine->mGameType;
                        tr->drawStringPixel(w * 0.5f, h - big, big, 1.0f, 1.0f, 0.0f, 1.0f, 1, "%s", starting);
                        tr->drawStringPixel(w * 0.5f, h - g * 144.0f, g * 48.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1, "%s: %s",
                                            t.getC(0xd2), t.getC(kName[gt]));
                        tr->drawStringPixel(w * 0.5f, h - g * bitsf(0x42b33333), g * 32.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1,
                                            "%s", t.getC(kDesc[gt]));
                        break;
                    }
                    case 1: {
                        // the campaign loading screen: the episode's chapter and
                        // name over its picture, cmp1_cut_<n>.jpg [!] (always
                        // "cmp1", whatever the campaign), drawn as a square that
                        // covers the screen
                        const int iw = mCamera->getWidth(), ih = mCamera->getHeight();
                        const float cx = static_cast<float>(iw / 2);
                        if (CCampaign* cmp = mEngine->getCurCampaign()) {
                            char chapter[1024], episode[1024];
                            const unsigned c = cmp->mCurChapter;
                            std::snprintf(chapter, sizeof chapter, "%s %i: %s", t.getC(0x1b3), static_cast<int>(c),
                                          cmp->mChapterNames[c].c_str());
                            std::snprintf(episode, sizeof episode, "%s %i: %s", t.getC(0x1b6),
                                          static_cast<int>(cmp->mCurEpisode), cmp->getCurrentMapInfo()->name.c_str());
                            tr->drawStringPixel(cx, h - g * 144.0f, g * 48.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1, "%s",
                                                formatNoArgs(chapter).c_str());
                            tr->drawStringPixel(cx, h - g * bitsf(0x42b33333), g * bitsf(0x42266666), 1.0f, 1.0f,
                                                1.0f, 1.0f, 1, "%s", formatNoArgs(episode).c_str());
                            char cut[64];
                            std::snprintf(cut, sizeof cut, "cmp1_cut_%u.jpg", cmp->getCurrentMapInfo()->cutImage);
                            if (CTexture* tex = CTextureManager::get().getTexture(cut, 1, 2, 2, true)) {
                                tex->bind(GL_TEXTURE0_);
                                CVector2 pos, size;
                                if (ih < iw) {
                                    pos = CVector2(0.0f, static_cast<float>((ih - iw) / 2));
                                    size = CVector2(static_cast<float>(iw), static_cast<float>(iw));
                                } else {
                                    pos = CVector2(static_cast<float>((iw - ih) / 2), 0.0f);
                                    size = CVector2(h, h);
                                }
                                addStandardQuadPixel(pos, CVector2(0.0f, 0.0f), CVector2(1.0f, 1.0f), size);
                                drawScreenSpaceQuads();
                            }
                        }
                        tr->drawStringPixel(cx, h - big, big, 1.0f, 1.0f, 0.0f, 1.0f, 1, "%s", starting);
                        break;
                    }
                    default: break;
                }
                tr->draw();
            }
            mDrawMode = 2;
            break;
        }
        case 2: {
            gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl::Clear(GL_COLOR_DEPTH_);
            drawBackground(*this);
            // the menu camera waits at the pause screen for the return from the game
            mCamera->mTargetX = bitsf(0xc415c000);
            mCamera->mTargetY = bitsf(0xc30f0000);
            mCamera->refresh();
            switch (mStartGameMode) {
                case 1: initSingleplayerGame(); break;
                case 2: initMultiplayerSplitGame(); break;
                case 3: CUtils::log("CInterfaceMenu: loadGame %s notPorted (savegames: later phase)", mLoadFile.c_str());
                    mDrawMode = 0;
                    break;
                case 4: initMultiplayerNetworkGame(); break;
                default: CUtils::log("Error: unknown mStartGameMode %i", mStartGameMode);
            }
            break;
        }
        case 3: {
            // the campaign's credits roll (processAction 0x54): the final
            // picture cmp1_cut_<finalcut>.jpg, the "final" text scrolling up
            // at 25 px/s (32 px x GUI scale per line), then the end titles
            // (CTxtLoc 0x1c6 large in the middle, 0x1c7 yellow at the
            // bottom). A tap ends it (processEventActionUp: action 0x17).
            gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            gl::Clear(GL_COLOR_DEPTH_);
            mCamera->activateGLviewport();
            CCampaign* cmp = mEngine->mGameType == 1 ? mEngine->getCurCampaign() : nullptr;
            if (cmp && tr) {
                const int iw = mCamera->getWidth(), ih = mCamera->getHeight();
                char cut[64];
                std::snprintf(cut, sizeof cut, "cmp1_cut_%u.jpg", cmp->mFinalCut);
                CTexture* tex = CTextureManager::get().getTexture(cut, 1, 2, 2, true);
                if (tex) tex->bind(GL_TEXTURE0_);
                CVector2 pos;
                int side;
                if (ih < iw) {
                    pos = CVector2(0.0f, static_cast<float>((ih - iw) / 2));
                    side = iw;
                } else {
                    pos = CVector2(static_cast<float>((iw - ih) / 2), 0.0f);
                    side = ih;
                }
                const float fs = static_cast<float>(side);
                addStandardQuadPixel(pos, CVector2(0.0f, 0.0f), CVector2(1.0f, 1.0f), CVector2(fs, fs));
                drawScreenSpaceQuads();
                mCreditsScroll = mCreditsScroll + mEngine->mFrameDt * 25.0f;
                const float fh = static_cast<float>(ih);
                const float base = fh - mCreditsScroll;
                const float unit = g * 32.0f;
                int shown = 0;
                for (const CTextListing& l : mCreditsText) {
                    const float y = l.y * unit + base;
                    if (fh > y) {
                        const float size = l.scale * unit;
                        if (y + size > 0.0f) {
                            tr->drawStringPixel(static_cast<float>(iw / 2), y, size, 1.0f, 1.0f, 1.0f, 1.0f, 1, "%s",
                                                formatNoArgs(l.text).c_str());
                            ++shown;
                        }
                    }
                }
                if (shown == 0 && mCreditsScroll > 5.0f) {
                    const float big = unit * 3.0f;
                    tr->drawStringPixel(static_cast<float>(iw / 2), fh * 0.5f - big * 0.5f, big, 1.0f, 1.0f, 1.0f,
                                        1.0f, 1, "%s", t.getC(0x1c6));
                    tr->drawStringPixel(static_cast<float>(iw / 2), fh - unit, unit / bitsf(0x3fb33333), 1.0f, 1.0f,
                                        0.0f, 1.0f, 1, "%s", t.getC(0x1c7));
                }
            }
            if (tr) tr->draw();
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Starting a game

// Ghidra 0x1af6cc
unsigned CInterfaceMenu::localPlayerFromSetup(const CPlayerSetup* s, unsigned n) {
    unsigned local = 1;
    if (n != 0) {
        if (s[1].type == 0) local = 1;
        if (n > 1) {
            if (s[2].type == 0) local = 2;
            if (n > 2) {
                if (s[3].type == 0) local = 3;
                if (n > 3 && s[4].type == 0) local = 4;
            }
        }
    }
    return local;
}

void CInterfaceMenu::initSingleplayerGame() {
    CEngine* e = mEngine;
    // [!] the local player is the LAST human slot among players 1..n
    e->mLocalPlayer = localPlayerFromSetup(e->mPlayerSetup, e->mNumPlayers);
    e->pauseGame();
    CRng::SetSeedFromSystemTime();
    CRngSafe::SetSeedFromSystemTime();
    e->mNetworkMode = 0;
    e->setGame(std::make_unique<CGame>());
    e->mGame->init(false, !gl::isLoaded());   // [PC] headless without a GL context (tools, tests)
    auto ingame = std::make_unique<CInterfaceIngame>();
    ingame->initSingleplayer(mWidth, mHeight, e->mLocalPlayer);
    e->mIngameInterface = std::move(ingame);
    e->mCurrentInterface = e->mIngameInterface.get();
    e->resumeGame(e->mFrameTime);
}

// Ghidra 0x1af614: split screen needs CInterfaceIngame::initSplitscreen (the
// HUD package). [PC] Until then it starts a single player game with the
// same setup.
void CInterfaceMenu::initMultiplayerSplitGame() {
    CUtils::log("CInterfaceMenu::initMultiplayerSplitGame notPorted: starting a single player game instead");
    initSingleplayerGame();
}

// Ghidra 0x1af7fc
void CInterfaceMenu::initMultiplayerNetworkGame() {
    CUtils::log("CInterfaceMenu::initMultiplayerNetworkGame notPorted (networking: later phase)");
    mDrawMode = 0;
}

}  // namespace py
