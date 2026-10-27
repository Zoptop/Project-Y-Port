// CInterfaceMenu.cpp - PLACEHOLDER implementation (phase 5a).
//
// =========================================================================
// TO BE REPLACED by the menus package. Ported faithfully here: the
// constructor/init resources, setResolution, timeUpdate (camera only), the
// touch dispatch to elements (Down/Move/Up minus particles, sounds and the
// lobby-chat corner button), the Back/Menu keys and the "draw mode 0" scene
// (background, elements, menu colour meshes, element meshes, text).
// Placeholder parts, marked [PLACEHOLDER]: setupElements builds screen 0
// (the main menu, from the decompiled setupElements case 0) and a stub page
// for every other screen; processAction only handles 0x71 (sent by the
// intro: offline path), Exit (6), Back (7) and the main menu's screen
// changes with their camera scrolls.
// =========================================================================
#include "CInterfaceMenu.h"

#include <cstdio>

#include "core/CConfig.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTextRender.h"
#include "render/CTexture.h"
#include "render/Colors.h"
#include "ui/CInterfaceElement.h"
#include "ui/CInterfaceElementButton2.h"
#include "ui/CInterfaceElementImageButton.h"
#include "ui/CInterfaceElementText.h"

namespace py {

namespace {
constexpr unsigned GL_DEPTH_TEST_ = 0x0B71, GL_CULL_FACE_ = 0x0B44, GL_BLEND_ = 0x0BE2, GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_COLOR_DEPTH_ = 0x4100, GL_ONE_ = 1, GL_SRC_ALPHA_ = 0x0302;
}  // namespace

// Ghidra 0x1b07dc
CInterfaceMenu::CInterfaceMenu() {
    mType = TYPE_MENU;   // +0x34 = 0
    mModalState = 0;
    mTouchedElement = nullptr;
}

// Ghidra 0x1b0b9c
CInterfaceMenu::~CInterfaceMenu() = default;

void CInterfaceMenu::clearElements() {
    mTouchedElement = nullptr;
    mElements.clear();
}

// Ghidra 0x1cd118 (the CPU quad buffers +0x5cc/+0x650/+0x694 are the menus
// package's: they belong to addColoredQuad & co.)
void CInterfaceMenu::init(int menuType) {
    mEngine = CEngine::getSingletonPtr();
    mCamera = std::make_unique<CCameraMenu>();
    mNextMenu = menuType;
    mDrawMode = 0;
    mBackground = CTextureManager::get().getTexture("background.png", 1, 2, 2, true);
    mCreditsBlack = CTextureManager::get().getTexture("credits_black.png", 1, 2, 2, true);
    mMesh = CMeshManager::get().getMesh("menu.3DS.mesh", true, false);
    mProgramPlayerColorMenu = CGLSLProgramManager::get().getGLSLProgram("playercolormenu.gfx");
    mProgramSimple = CGLSLProgramManager::get().getGLSLProgram("simple.gfx");
    if (mProgramSimple) mProgramSimple->setupUniformTextureUnit("DIFFUSE", 0);
    mProgramSimpleAlpha = CGLSLProgramManager::get().getGLSLProgram("simplealpha.gfx");
    if (mProgramSimpleAlpha) mProgramSimpleAlpha->setupUniformTextureUnit("DIFFUSE", 0);
    mProgramInterfaceColored = CGLSLProgramManager::get().getGLSLProgram("interfacecolored.gfx");
    if (mProgramInterfaceColored) mProgramInterfaceColored->setupUniformTextureUnit("DIFFUSE", 0);
    mInterfaceTexture = CTextureManager::get().getTexture("interface.png", 1, 2, 2, false);
    mProgramSelect = CGLSLProgramManager::get().getGLSLProgram("select.gfx");
    if (mProgramSelect) mProgramSelect->setupUniformTextureUnit("DIFFUSE", 0);
    mProgramInterfaceColoredAlpha = CGLSLProgramManager::get().getGLSLProgram("interfacecoloredalpha.gfx");
    if (mProgramInterfaceColoredAlpha) mProgramInterfaceColoredAlpha->setupUniformTextureUnit("DIFFUSE", 0);
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

// Ghidra 0x1ad558 (the particle update and the download poll of screen 13
// are the menus package's)
void CInterfaceMenu::timeUpdate(float dt) { mCamera->timeUpdate(dt); }

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
    for (auto& el : mElements)
        if (el.get() == mTouchedElement) el->processEventActionMove(e);
}

// Ghidra 0x1d5a88 (minus the click particles/sounds and the lobby-chat
// corner button: menus package)
void CInterfaceMenu::processEventActionUp(CInterfaceEvent* e) {
    if (mModalState != 0) return;
    if (mDrawMode == 3) {
        mDrawMode = 0;
        processAction(0x17, nullptr);
        mForceSetup = false;
        setupElements(mNextMenu);
        mTouchedElement = nullptr;
        return;
    }
    for (size_t i = 0; i < mElements.size(); ++i) {
        CInterfaceElement* el = mElements[i].get();
        if (el == mTouchedElement && el->processEventActionUp(e) != 0) break;
    }
    if (mNextMenu != mCurrentMenu || mForceSetup) {
        mForceSetup = false;
        setupElements(mNextMenu);
    }
    mTouchedElement = nullptr;
}

// Ghidra 0x1d59e8
void CInterfaceMenu::processEventBackKeyPressed(CInterfaceEvent*) {
    if (mModalState != 0) return;
    processBackButton();
    if (mNextMenu != mCurrentMenu || mForceSetup) {
        mForceSetup = false;
        setupElements(mNextMenu);
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

namespace {
// [PLACEHOLDER] where processAction scrolls the menu camera for a screen
// (from the decompiled processAction cases); screens not listed: (0, 0)
CVector2 screenCenter(int screen) {
    switch (screen) {
        case 1: return CVector2(bitsf(0x43050000), bitsf(0x43870000));   // (133, 270)
        case 2: return CVector2(bitsf(0x43a02666), bitsf(0xc17ccccd));   // (320.3, -15.8)
        case 3: return CVector2(bitsf(0xc4600000), bitsf(0x44124000));   // (-896, 585)
        case 5:
        case 6: return CVector2(bitsf(0xc3988000), bitsf(0xc32a0000));   // (-305, -170)
        default: return CVector2(0.0f, 0.0f);
    }
}
}  // namespace

// [PLACEHOLDER] Ghidra 0x1cd3f8: the camera scrolls (k = 10) are the original's
void CInterfaceMenu::processAction(int action, CInterfaceElement*) {
    auto go = [&](int screen) {
        mNextMenu = screen;
        mCamera->slowScrollTo(screenCenter(screen), 10.0f);
    };
    switch (action) {
        case 0x71: mCamera->slowScrollTo(CVector2(0.0f, 0.0f), 10.0f); break;   // after the intro (offline path)
        case 6: mEngine->mQuitRequested = true; break;   // main menu "Exit": CEngine::quit  // [PC] ends the frame loop
        case 7: go(0); break;                            // Back
        case 1: mEngine->mGameType = 0; go(1); break;    // Singleplayer
        case 2: mEngine->mGameSpeedFactor = 1.0f; go(2); break;   // Multiplayer
        case 3: go(3); break;                            // Profile
        case 4: go(5); break;                            // Options
        case 5: go(6); break;                            // Options 2
        default: CUtils::log("CInterfaceMenu placeholder: action 0x%x not handled", action); break;
    }
}

// [PLACEHOLDER] Ghidra 0x1bbcb8: screen 0 as decompiled (setupElements case 0)
void CInterfaceMenu::setupElements(int menuType, int /*action*/) {
    clearElements();
    mCurrentMenu = menuType;
    mBackAction = 0;
    const CTxtLoc& t = CTxtLoc::get();
    const Vec3& white = colors::COL_WHITE;
    const Vec3 colorG = mEngine->mMenuColorG;   // engine+0x230
    if (menuType == 0) {
        addElement(new CInterfaceElementText(this, bitsf(0xc1840000), 80.0f, t.getS(0xf), 1, 5.5f, colorG));
        addElement(new CInterfaceElementText(this, 24.0f, 79.0f, "-full version-", 0, bitsf(0x40840000),
                                             colors::COL_GOLD));
        struct B {
            int action;
            const char* mesh;
            uint32_t x, y;
            int text, align;
            uint32_t w, offX, offY;
        };
        static const B kButtons[] = {
            {1, "main_singl", 0x41ca6666, 0x423e0000, 7, 2, 0x42580000, 0x40c00000, 0x40800000},
            {2, "main_multi", 0x41380000, 0x41d26666, 8, 0, 0x42500000, 0xc0c9999a, 0x40733333},
            {4, "main_optio", 0xbfd9999a, 0xc1680000, 9, 2, 0x42480000, 0x40b00000, 0x4089999a},
            {6, "main_exit", 0x3f800000, 0xc2493333, 10, 2, 0x420c0000, 0x3fe66666, 0x4089999a},
            {3, "main_profi", 0xc21a0000, 0xc2080000, 0x24c, 0, 0x41f00000, 0xc0b9999a, 0x4089999a},
            {0x47, "main_hof", 0xc0a33333, 0x41d40000, 599, 2, 0x42400000, 0x40b9999a, 0x4089999a},
            {0x23, "main_credi", 0xbf800000, 0x40b9999a, 0x91, 2, 0x42400000, 0x40e00000, 0x40733333},
            {0x25, "main_websi", 0x41900000, 0xc0cccccd, 0x9f, 0, 0x42480000, 0xc099999a, 0x4089999a},
            {0x26, "main_gameg", 0x41926666, 0xc1c26666, 0xa1, 0, 0x42480000, 0xc0e9999a, 0x40533333},
            {0x27, "main_forum", 0x4134cccd, 0xc2273333, 0xa0, 0, 0x42200000, 0xc089999a, 0x40200000},
        };
        for (const B& b : kButtons) {
            auto* btn = addElement(new CInterfaceElementButton2(this, b.action, b.mesh, bitsf(b.x), bitsf(b.y),
                                                                t.getS(b.text), b.align, false, 5.0f, white,
                                                                bitsf(b.w), 12.0f, bitsf(b.offX), bitsf(b.offY)));
            if (b.action == 0x47) btn->mDisabled = true;   // Hall of fame: needs the (gone) online lobby
        }
        addElement(new CInterfaceElementImageButton(this, 0x57, "btn_twitter.png", -65.0f, -2.0f, 15.0f, 15.0f));
        addElement(new CInterfaceElementImageButton(this, 0x59, "btn_facebook.png", -65.0f, -19.0f, 15.0f, 15.0f));
        char ver[300];
        std::snprintf(ver, sizeof ver, "v%s Rev %s", CConfig::get().getString("Version", "").c_str(),
                      CConfig::get().getString("Revision", "").c_str());
        const Vec3 verColor(colorG.x * 0.75f, colorG.y * 0.75f, colorG.z * 0.75f);
        addElement(new CInterfaceElementButton2(this, 0x42, "main_upd", bitsf(0x4144cccd), bitsf(0x427ecccd), ver, 1,
                                                false, 3.25f, verColor, 60.0f, 12.0f, bitsf(0x40533333), 2.5f));
        mBackAction = 6;
        return;
    }
    // [PLACEHOLDER] any other screen: its number and a way back
    const CVector2 c = screenCenter(menuType);
    char title[64];
    std::snprintf(title, sizeof title, "SCREEN %d", menuType);
    addElement(new CInterfaceElementText(this, c.x, c.y + 40.0f, title, 1, 5.5f, colorG));
    addElement(new CInterfaceElementText(this, c.x, c.y + 20.0f, "(menus not ported yet)", 1, 4.0f, white));
    addElement(new CInterfaceElementButton2(this, 7, "", c.x, c.y - 40.0f, t.getS(6), 1, true, 5.0f, white, 0.0f,
                                            0.0f, 0.0f, -5.0f));
    mBackAction = 7;
}

// Ghidra 0x1d3b10, draw mode 0 (the other modes - credits, game end, ... -
// and the corner chat button, coloured quads and particles are the menus
// package's)
void CInterfaceMenu::draw() {
    if (!gl::isLoaded() || !mMesh) return;
    gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl::Clear(GL_COLOR_DEPTH_);
    mCamera->activateGLviewport();
    mMesh->bindBuffer();
    if (mBackground) mBackground->bind(GL_TEXTURE0_);
    if (mProgramSimple) {
        mProgramSimple->startDraw();
        gl::Disable(GL_DEPTH_TEST_);
        gl::DepthMask(0);
        gl::Disable(GL_CULL_FACE_);
        mProgramSimple->setUniformMVPMatrix(mCamera->getViewProjMatrix());
        drawSubMesh(mMesh, "background");
        mProgramSimple->endDraw();
    }
    gl::Enable(GL_DEPTH_TEST_);
    gl::Enable(GL_CULL_FACE_);
    gl::DepthMask(1);
    for (auto& el : mElements) el->draw();
    // drawColoredQuads (menus package)
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
        for (auto& el : mElements) el->drawMesh();
        mProgramPlayerColorMenu->endDraw();
        gl::Disable(GL_BLEND_);
    }
    if (CTextRender* tr = mEngine->getTextRender()) tr->drawDirect(mCamera->getViewProjMatrix());
}

}  // namespace py
