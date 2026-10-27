// CInterfaceIngame.cpp - see CInterfaceIngame.h.
#include "ui/CInterfaceIngame.h"

#include "core/CUtils.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"
#include "game/CScript.h"
#include "ui/CEventBar.h"
#include "ui/CGroupBar.h"
#include "ui/CIngameWorldDraw.h"
#include "ui/CUISound.h"
#include "ui/CInterfaceIngamePerPlayer.h"

namespace py {

// Ghidra 0x19e154
CInterfaceIngame::CInterfaceIngame() {
    mType = TYPE_INGAME;   // +0x34 = 1
    mEngine = CEngine::getSingletonPtr();
}

// Ghidra 0x19e1d4
CInterfaceIngame::~CInterfaceIngame() {
    if (IIngameUI::get() == this) IIngameUI::set(nullptr);
    if (CScriptManager::ui() == this) CScriptManager::setUI(nullptr);
}

// Ghidra 0x19e27c
void CInterfaceIngame::init() {
    mEngine = CEngine::getSingletonPtr();
    mPointerCounter = 1;
    mSelectionQuadZ = 0.0f;
    mSelectionQuadYaw = 0.0f;
    mScreenQuads.assign(kMaxQuads + 1, CQuadPosUV());
    mColoredScreenQuads.assign(kMaxQuads + 1, CQuadPosUVColor());
    mColored4ScreenQuads.assign(kMaxQuads + 1, CQuadPosUVColor4());
    mNumScreenQuads = mNumColoredScreenQuads = mNumColored4ScreenQuads = 0;
    mWorldQuads = CIngameQuads();
    if (!gl::isLoaded()) return;   // [PC] headless (tests): no GL resources
    mWorldQuads.initGraphics();
    CGLSLProgramManager& pm = CGLSLProgramManager::get();
    mSelectShader = pm.getGLSLProgram("select.gfx");
    if (mSelectShader) mSelectShader->setupUniformTextureUnit("DIFFUSE", 0);
    mInterfaceTexture = CTextureManager::get().getTexture("interface.png", 1, 2, 2, false);
    mColoredShader = pm.getGLSLProgram("interfacecolored.gfx");
    if (mColoredShader) mColoredShader->setupUniformTextureUnit("DIFFUSE", 0);
    mColoredMMShader = pm.getGLSLProgram("interfacecolored_mm.gfx");
    if (mColoredMMShader) mColoredMMShader->setupUniformTextureUnit("DIFFUSE", 0);
    mColoredAlphaShader = pm.getGLSLProgram("interfacecoloredalpha.gfx");
    if (mColoredAlphaShader) mColoredAlphaShader->setupUniformTextureUnit("DIFFUSE", 0);
    mCircleShader = pm.getGLSLProgram("circle.gfx");
    mCircleUniformShader = pm.getGLSLProgram("circleuniform.gfx");
    mCircleMesh = CMeshManager::get().getMesh("circletest.3DS.mesh", false, false);
    static const char* kCircles[6] = {"circle10", "circle25", "circle50", "circle100", "circle150", "circle200"};
    if (mCircleMesh)
        for (int i = 0; i < 6; ++i) mCircleMesh->getSubMeshData(kCircles[i], mCircleSub[i][0], mCircleSub[i][1]);
}

// Ghidra 0x19e7fc
void CInterfaceIngame::initSingleplayer(int width, int height, unsigned player) {
    init();
    mSplitMode = 0;
    mOwned[0] = std::make_unique<CInterfaceIngamePerPlayer>();
    mIPP[0] = mOwned[0].get();
    mIPP[0]->init(this, mEngine->mGame ? mEngine->mGame->getPlayer(player) : nullptr);
    setResolution(width, height);
    mCurrentIPP = 0;
    IIngameUI::set(this);          // [PC] the game objects' and scripts' way to the HUD
    CScriptManager::setUI(this);
}

// Ghidra 0x19e734
void CInterfaceIngame::initSplitscreen(int width, int height, unsigned p1, unsigned p2) {
    init();
    mSplitMode = 1;
    for (int i = 0; i < 2; ++i) {
        mOwned[i] = std::make_unique<CInterfaceIngamePerPlayer>();
        mIPP[i] = mOwned[i].get();
    }
    mIPP[0]->init(this, mEngine->mGame->getPlayer(p1));
    mIPP[1]->init(this, mEngine->mGame->getPlayer(p2));
    setResolution(width, height);
    mCurrentIPP = 0;
    IIngameUI::set(this);
    CScriptManager::setUI(this);
}

CInterfaceIngame* CInterfaceIngame::startSkirmish(unsigned mapId, unsigned players, bool fow, int width,
                                                 int height) {
    CEngine* e = CEngine::getSingletonPtr();
    e->setupSkirmish(mapId, players);
    e->mUseFOW = fow;
    e->mGameType = 0;
    e->mLocalPlayer = 1;
    e->pauseGame();
    e->mNetworkMode = 0;
    e->setGame(std::make_unique<CGame>());
    if (!e->mGame->init(false, !gl::isLoaded())) {   // [PC] headless without a GL context (tests)
        CUtils::log("skirmish: map %u failed to load", mapId);
        e->setGame(nullptr);
        return nullptr;
    }
    auto ingame = std::make_unique<CInterfaceIngame>();
    ingame->initSingleplayer(width, height, e->mLocalPlayer);
    CInterfaceIngame* r = ingame.get();
    e->mIngameInterface = std::move(ingame);
    e->mCurrentInterface = e->mIngameInterface.get();
    e->resumeGame(e->mFrameTime);
    return r;
}

// Ghidra 0x19dc2c
void CInterfaceIngame::draw() {
    if (gl::isLoaded()) {
        gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);   // 0x4100
    }
    CGame* game = mEngine->mGame;
    if (game && game->mStarted) {
        if (mSplitMode == 0) {
            mCurrentIPP = 0;
            mIPP[0]->draw();
        } else if (mSplitMode == 1) {
            mCurrentIPP = 0;
            mIPP[0]->draw();
            mCurrentIPP = 1;
            mIPP[1]->draw();
        }
    }
    ingamedraw::drawReset();
}

// Ghidra 0x19dcc0
CCamera* CInterfaceIngame::getCamera() {
    CInterfaceIngamePerPlayer* p = mIPP[mCurrentIPP];
    return p ? p->getCamera() : nullptr;
}

// Ghidra 0x19dcd0
void CInterfaceIngame::timeUpdate(float dt) {
    if (mSplitMode == 0) {
        mCurrentIPP = 0;
        if (mIPP[0]) mIPP[0]->timeUpdate(dt);
    } else if (mSplitMode == 1) {
        mCurrentIPP = 0;
        mIPP[0]->timeUpdate(dt);
        mCurrentIPP = 1;
        mIPP[1]->timeUpdate(dt);
    }
    CGame* game = mEngine->mGame;
    if (game && game->getCommandManager()) game->getCommandManager()->timeUpdate();   // vtable +0x50
}

// Ghidra 0x19dc18
void CInterfaceIngame::processEventBackKeyPressed(CInterfaceEvent* e) { processEventMenuKeyPressed(e); }

// Ghidra 0x19dd4c
void CInterfaceIngame::processEventMenuKeyPressed(CInterfaceEvent*) {
    if (mEngine->mLoadingSavegame) return;
    CGame* game = mEngine->mGame;
    if (!game || !game->mStarted) return;
    mEngine->showMenuInterface();
}

// Ghidra 0x19dd78
void CInterfaceIngame::processTextInput(std::string text, bool cancelled) {
    if (mSplitMode == 0) {
        mIPP[0]->processTextInput(text, cancelled);
    } else if (mSplitMode == 1) {
        mIPP[0]->processTextInput(text, cancelled);
        mIPP[1]->processTextInput(text, cancelled);
    }
}

// Ghidra 0x19de48
void CInterfaceIngame::setResolution(int width, int height) {
    mWidth = width;
    mHeight = height;
    mLandscape = height < width;
    if (mSplitMode == 0) {
        if (mIPP[0]) mIPP[0]->setResolution(width, height, 0, 0, 0);
        return;
    }
    if (mSplitMode == 1) {
        if (height < width) {
            const int half = width / 2;
            mIPP[0]->setResolution(half, height, 0, 0, 0x5a);
            mIPP[1]->setResolution(half, height, half, 0, 0x10e);
            return;
        }
        const int half = height / 2;
        mIPP[0]->setResolution(width, half, 0, 0, 0);
        mIPP[1]->setResolution(width, half, 0, half, 0xb4);
    }
}

// ---------------------------------------------------------------------------
// pointer routing

// Ghidra 0x19fad8
bool CInterfaceIngame::updateSplitscreenCoords(CVector2& pos) const {
    if (!mLandscape) {
        const float half = static_cast<float>(mHeight) * 0.5f;
        if (pos.y > half) {
            pos.y = pos.y - half;
            return false;
        }
        return true;
    }
    const float half = static_cast<float>(mWidth) * 0.5f;
    if (!(pos.x < half)) {
        pos.x = pos.x - half;
        return true;
    }
    return false;
}

// Ghidra 0x19fbcc
bool CInterfaceIngame::isPointerAssignedSingle(unsigned id) const {
    const int i = static_cast<int>(id);
    return mIPP[0]->mPointer[0].id == i || mIPP[0]->mPointer[1].id == i;
}

// Ghidra 0x19fbf8
bool CInterfaceIngame::isPointerAssignedSplit(unsigned id) const {
    const int i = static_cast<int>(id);
    return mIPP[0]->mPointer[0].id == i || mIPP[0]->mPointer[1].id == i || mIPP[1]->mPointer[0].id == i ||
           mIPP[1]->mPointer[1].id == i;
}

// Ghidra 0x19fde8
int CInterfaceIngame::getInterfaceAssignedSplit(unsigned id) const {
    const int i = static_cast<int>(id);
    if (mIPP[0]->mPointer[0].id == i || mIPP[0]->mPointer[1].id == i) return 0;
    if (mIPP[1]->mPointer[0].id != i && mIPP[1]->mPointer[1].id != i) return -1;
    return 1;
}

// Ghidra 0x19fc40
void CInterfaceIngame::processEventActionDown(CInterfaceEvent* e) {
    CInterfaceIngamePerPlayer* p = nullptr;
    if (mSplitMode == 0) {
        if (isPointerAssignedSingle(e->pointer)) return;
        p = mIPP[0];
    } else if (mSplitMode == 1) {
        if (isPointerAssignedSplit(e->pointer)) {
            CUtils::log("processEventActionDown %i obwohl schon down!!", e->pointer);
            return;
        }
        CVector2 pos(e->x, e->y);
        mCurrentIPP = updateSplitscreenCoords(pos) ? 1 : 0;
        e->x = pos.x;
        e->y = pos.y;
        p = mIPP[mCurrentIPP];
    } else {
        return;
    }
    if (p->mPointer[0].id != -1) {
        if (p->mPointer[1].id != -1) {
            CUtils::log("processEventActionDown %i schon zwei pointer zugewiesen!!", e->pointer);
            return;
        }
        p->mPointer[1].id = static_cast<int>(e->pointer);
        p->mPointer[1].order = mPointerCounter++;
    } else {
        p->mPointer[0].id = static_cast<int>(e->pointer);
        p->mPointer[0].order = mPointerCounter++;
    }
    p->processEventActionDown(e);
}

// Ghidra 0x19fe38
void CInterfaceIngame::processEventActionMove(CInterfaceEvent* e) {
    CInterfaceIngamePerPlayer* p = nullptr;
    const int id = static_cast<int>(e->pointer);
    if (mSplitMode == 0) {
        if (!isPointerAssignedSingle(e->pointer)) return;   // (logged in the original)
        p = mIPP[0];
    } else if (mSplitMode == 1) {
        if (!isPointerAssignedSplit(e->pointer)) return;
        mCurrentIPP = getInterfaceAssignedSplit(e->pointer);
        if (mCurrentIPP == -1) return;
        CVector2 pos(e->x, e->y);
        const int vp = updateSplitscreenCoords(pos) ? 1 : 0;
        e->x = pos.x;
        e->y = pos.y;
        if (vp != mCurrentIPP) return;   // "nicht mehr im richtigen viewport"
        p = mIPP[vp];
    } else {
        return;
    }
    if (p->mPointer[0].id != id && p->mPointer[1].id != id) return;
    p->processEventActionMove(e);
}

// Ghidra 0x19ff84
void CInterfaceIngame::processEventActionUp(CInterfaceEvent* e) {
    CInterfaceIngamePerPlayer* p = nullptr;
    const int id = static_cast<int>(e->pointer);
    if (mSplitMode == 0) {
        if (!isPointerAssignedSingle(e->pointer)) return;
        p = mIPP[0];
    } else if (mSplitMode == 1) {
        if (!isPointerAssignedSplit(e->pointer)) return;
        mCurrentIPP = getInterfaceAssignedSplit(e->pointer);
        if (mCurrentIPP == -1) return;
        CVector2 pos(e->x, e->y);
        updateSplitscreenCoords(pos);
        e->x = pos.x;
        e->y = pos.y;
        p = mIPP[mCurrentIPP];
    } else {
        return;
    }
    if (p->mPointer[0].id == -1 && p->mPointer[1].id == -1) return;
    if (p->mPointer[0].id == id) p->mPointer[0].id = -1;
    else if (p->mPointer[1].id == id) p->mPointer[1].id = -1;
    else return;
    p->processEventActionUp(e);
}

// ---------------------------------------------------------------------------
// [PC] mouse and keyboard: single player routes everything to IPP 0

void CInterfaceIngame::processEventMouseButtonDown(CInterfaceEvent* e) {
    if (mIPP[0]) mIPP[0]->processPCMouseDown(e->button, CVector2(e->x, e->y), e->modifiers, e->clicks, e->timestamp);
}
void CInterfaceIngame::processEventMouseButtonUp(CInterfaceEvent* e) {
    if (mIPP[0]) mIPP[0]->processPCMouseUp(e->button, CVector2(e->x, e->y), e->modifiers, e->timestamp);
}
void CInterfaceIngame::processEventMouseMove(CInterfaceEvent* e) {
    if (mIPP[0]) mIPP[0]->processPCMouseMove(CVector2(e->x, e->y), e->buttons, e->modifiers);
}
void CInterfaceIngame::processEventMouseWheel(CInterfaceEvent* e) {
    if (mIPP[0]) mIPP[0]->processPCWheel(e->wheelY, CVector2(e->x, e->y));
}
void CInterfaceIngame::processEventKeyDown(CInterfaceEvent* e) {
    if (mIPP[0]) mIPP[0]->processPCKeyDown(e->keycode, e->modifiers, e->repeat);
}
void CInterfaceIngame::processEventKeyUp(CInterfaceEvent* e) {
    if (mIPP[0]) mIPP[0]->processPCKeyUp(e->keycode, e->modifiers);
}
int CInterfaceIngame::getCursorHint() const { return mIPP[0] ? mIPP[0]->getCursorHint() : 0; }

// ---------------------------------------------------------------------------
// players

// Ghidra 0x19fac8
CInterfaceIngamePerPlayer* CInterfaceIngame::getCurrentInterfacePerPlayer() const { return mIPP[mCurrentIPP]; }

// Ghidra 0x1a0154
CPlayer* CInterfaceIngame::getCurrentInterfacePlayer() const { return mIPP[mCurrentIPP]->getPlayer(); }

// Ghidra 0x1a0168
CInterfaceIngamePerPlayer* CInterfaceIngame::getInterfaceForPlayer(CPlayer* player) const {
    if (!player || player->isAI() || player->isNeutral()) return nullptr;
    if (mSplitMode == 0) {
        if (mIPP[0] && mIPP[0]->getPlayer() == player) return mIPP[0];
        return nullptr;
    }
    if (mSplitMode != 1) return nullptr;
    if (mIPP[0] && mIPP[0]->getPlayer() == player) return mIPP[0];
    if (mIPP[1] && mIPP[1]->getPlayer() == player) return mIPP[1];
    return nullptr;
}

// Ghidra 0x1a0da0 (compares player ids, +0x1c)
bool CInterfaceIngame::isLocalPlayer(CPlayer* player) const {
    if (!player) return false;
    for (CInterfaceIngamePerPlayer* p : mIPP)
        if (p && p->getPlayer() && p->getPlayer()->mId == player->mId) return true;
    return false;
}

// ---------------------------------------------------------------------------
// quad queues. Pixel -> screen space for display rotation 0:
//   x' = aspect * ((x + x) / W - 1), y' = (y * -2) / H + 1,
//   size' = ((sx + sx) / W * aspect, (sy + sy) / H)

namespace {
void pixelToScreen(const CCamera* cam, const CVector2& p, const CVector2& s, CVector2& outP, CVector2& outS) {
    const float w = static_cast<float>(cam->mWidth), h = static_cast<float>(cam->mHeight);
    const float aspect = cam->mAspect;
    outS = CVector2((s.x + s.x) / w * aspect, (s.y + s.y) / h);
    outP = CVector2(aspect * ((p.x + p.x) / w - 1.0f), (p.y * -2.0f) / h + 1.0f);
}
}  // namespace

namespace {
CVector3 cv3(const Vec3& v) { return CVector3(v.x, v.y, v.z); }
}  // namespace

// Ghidra 0x19e880: rotating selection quad (z rises by 0.02 per quad)
void CInterfaceIngame::addSelectionQuad(const Vec3& pos, float size) {
    mSelectionQuadZ = mSelectionQuadZ + 0.02f;   // 0x3ca3d70a
    mWorldQuads.addStandardQuad(CVector3(pos.x, pos.y, mSelectionQuadZ + 0.5f), CVector2(0.0f, 0.0f),
                                CVector2(0.25f, 0.25f), size);
}

// Ghidra 0x19e96c
void CInterfaceIngame::addStandardQuad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize, float half) {
    mWorldQuads.addStandardQuad(cv3(center), uvTL, uvSize, half);
}

// Ghidra 0x19e9dc
void CInterfaceIngame::addStandardQuadScreenSpace(const CVector2& pos, const CVector2& uvTL, const CVector2& uvSize,
                                                  const CVector2& size) {
    const CVector2 half(size.x * 0.5f, size.y * 0.5f);
    const CCamera* cam = getCamera();
    const CVector3 c(pos.x * cam->mAspect, pos.y, 0.0f);
    if (mNumScreenQuads < kMaxQuads) mScreenQuads[mNumScreenQuads++] = CQuadPosUV(c, half, uvTL, uvSize, 0.0f);
}

// Ghidra 0x19ec14
void CInterfaceIngame::addStandardQuadPixel(const CVector2& topLeft, const CVector2& uvTL, const CVector2& uvSize,
                                            const CVector2& size) {
    CVector2 p, s;
    pixelToScreen(getCamera(), topLeft, size, p, s);
    if (mNumScreenQuads < kMaxQuads) mScreenQuads[mNumScreenQuads++] = CQuadPosUV(p, s, uvTL, uvSize);
}

// Ghidra 0x19eefc (yaw = the camera's, +0x19c)
void CInterfaceIngame::addColored4Quad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize,
                                       const CVector2& half, const Vec3& color, float alpha) {
    mWorldQuads.addColored4Quad(cv3(center), uvTL, uvSize, half, cv3(color), alpha, getCamera()->mYaw);
}

// Ghidra 0x19ef9c
void CInterfaceIngame::addColored4QuadPixel(const CVector2& topLeft, const CVector2& uvTL, const CVector2& uvSize,
                                            const CVector2& size, const Vec3& color, float alpha, float yaw) {
    CVector2 p, s;
    pixelToScreen(getCamera(), topLeft, size, p, s);
    if (mNumColored4ScreenQuads < kMaxQuads)
        mColored4ScreenQuads[mNumColored4ScreenQuads++] = CQuadPosUVColor4(p, s, uvTL, uvSize, cv3(color), alpha, yaw);
}

// Ghidra 0x19f29c
void CInterfaceIngame::addColoredQuadPixel(const CVector2& topLeft, const CVector2& uvTL, const CVector2& uvSize,
                                           const CVector2& size, const Vec3& color) {
    CVector2 p, s;
    pixelToScreen(getCamera(), topLeft, size, p, s);
    if (mNumColoredScreenQuads < kMaxQuads)
        mColoredScreenQuads[mNumColoredScreenQuads++] = CQuadPosUVColor(p, s, uvTL, uvSize, cv3(color));
}

// Ghidra 0x19f58c
void CInterfaceIngame::addStandardQuadScreenSpaceAdjustOnlyAspect(const CVector2& pos, const CVector2& uvTL,
                                                                  const CVector2& uvSize, const CVector2& size) {
    const CVector2 half(size.x * 0.5f, size.y * 0.5f);
    const CVector3 c(pos.x * getCamera()->mAspect, pos.y, 0.0f);
    if (mNumScreenQuads < kMaxQuads) mScreenQuads[mNumScreenQuads++] = CQuadPosUV(c, half, uvTL, uvSize, 0.0f);
}

// Ghidra 0x19f790
void CInterfaceIngame::addColoredQuadScreenSpaceAdjustOnlyAspect(const CVector2& pos, const CVector2& uvTL,
                                                                 const CVector2& uvSize, const CVector2& size,
                                                                 const Vec3& color) {
    const CVector2 half(size.x * 0.5f, size.y * 0.5f);
    const CVector3 c(pos.x * getCamera()->mAspect, pos.y, 0.0f);
    if (mNumColoredScreenQuads < kMaxQuads)
        mColoredScreenQuads[mNumColoredScreenQuads++] = CQuadPosUVColor(c, half, uvTL, uvSize, cv3(color), 0.0f);
}

// Ghidra 0x19f99c
void CInterfaceIngame::addColoredQuad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize, float half,
                                      const Vec3& color) {
    mWorldQuads.addColoredQuad(cv3(center), uvTL, uvSize, half, cv3(color), getCamera()->mYaw);
}

// Ghidra 0x19fa38
void CInterfaceIngame::addColoredQuad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize,
                                      const CVector2& half, const Vec3& color) {
    mWorldQuads.addColoredQuad(cv3(center), uvTL, uvSize, half, cv3(color), getCamera()->mYaw);
}

// Ghidra 0x1a0df0
void CInterfaceIngame::addCircleQuad(const Vec3& pos, float radius, float width, const Vec3& color, float alpha) {
    mWorldQuads.addCircleQuad(cv3(pos), radius, width, cv3(color), alpha);
}

// Ghidra 0x1a0d38
void CInterfaceIngame::startDrawCircle() {
    gl::DepthMask(0);
    gl::Disable(0x0B71);                // GL_DEPTH_TEST
    gl::Enable(0x0BE2);                 // GL_BLEND
    gl::BlendFunc(0x0302, 0x0303);      // SRC_ALPHA, ONE_MINUS_SRC_ALPHA
    if (mCircleMesh) mCircleMesh->bindBuffer();
    if (mCircleUniformShader) mCircleUniformShader->startDraw();
}

// Ghidra 0x1a0d78
void CInterfaceIngame::endDrawCircle() {
    if (mCircleUniformShader) mCircleUniformShader->endDraw();
    gl::Disable(0x0BE2);
    gl::DepthMask(1);
    gl::Enable(0x0B71);
}

// Ghidra 0x1a028c: the circle submesh for the radius, scaled to it
void CInterfaceIngame::drawCircle(const CCircleInfo& c, CCamera* cam) {
    CGLSLProgram* sh = mCircleUniformShader;
    if (!sh || !mCircleMesh || !cam) return;
    sh->setUniformPlayerColor(Vec3(c.color.x, c.color.y, c.color.z));
    sh->setUniformAlpha(c.alpha);
    const float r = c.radius;
    int idx;
    float base;
    if (r < 17.5f) idx = 0, base = 10.0f;            // 0x418c0000
    else if (r < 37.5f) idx = 1, base = 25.0f;       // 0x42160000
    else if (r < 75.0f) idx = 2, base = 50.0f;       // 0x42960000
    else if (r < 125.0f) idx = 3, base = 100.0f;     // 0x42fa0000
    else if (r < 175.0f) idx = 4, base = 150.0f;     // 0x432f0000
    else idx = 5, base = 200.0f;
    sh->setUniformSize(r);
    const float inv = 1.0f / base;
    const Vec3 scale(r * 1.0f * inv, r * 1.0f * inv, r * 1.0f * inv);
    const Mat4 model = Mat4::transform(Vec3(c.position.x, c.position.y, c.position.z), Quat(), scale);
    sh->setUniformMVPMatrix(cam->mViewProj * model);
    mCircleMesh->draw(mCircleSub[idx][0], mCircleSub[idx][1]);
}


// ---------------------------------------------------------------------------
// IIngameUI / IScriptUI: what the game objects and the scripts reach through
// getInterfaceForPlayer in the original

bool CInterfaceIngame::hasInterface(CPlayer* player) { return getInterfaceForPlayer(player) != nullptr; }

void CInterfaceIngame::addEvent(CPlayer* player, int type, unsigned objId, const CVector2& position, bool unique,
                                const std::string& shortText) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    if (p && p->getEventBar()) p->getEventBar()->addEvent(type, objId, position, unique, shortText);
}

void CInterfaceIngame::enableUnderAttackWarning(CPlayer* player, unsigned unitId) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    if (p && p->getGroupBar()) p->getGroupBar()->enableUnderAttackWarning(unitId);
}

unsigned CInterfaceIngame::getFirstGroupUnit(CPlayer* player, unsigned groupId) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    return p && p->getGroupBar() ? p->getGroupBar()->getFirstGroupUnit(groupId) : 0u;
}

void CInterfaceIngame::addUnitToGroup(CPlayer* player, unsigned groupId, unsigned unitId) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    if (!p || !p->getGroupBar()) return;
    if (CGroupInfo* g = p->getGroupBar()->getGroupInfo(groupId)) p->getGroupBar()->addUnitToGroup(*g, unitId);
}

bool CInterfaceIngame::isGameObjectSelected(CPlayer* player, unsigned id) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    return p && p->isGameObjectSelected(id);
}

void CInterfaceIngame::selectGameObject(CPlayer* player, unsigned id) {
    if (CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player)) p->selectGameObject(id);
}

void CInterfaceIngame::showGameEndText(CPlayer* player, const std::string& text, int callerScript,
                                       const CVector2* scrollTo) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    if (!p) return;
    p->setOverlayText(text, "", 0.0f, callerScript, false, true, "");
    if (scrollTo) p->getCamera()->slowScrollTo(*scrollTo, 1.0f);
    uisound::play("ringringring");
}

// CScriptUnitTrigger type 5: CGroupBar::isUnitInGroup(id)
bool CInterfaceIngame::isUnitInGroup(CPlayer* player, unsigned id) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    return p && p->getGroupBar() && p->getGroupBar()->isUnitInGroup(id);
}

// CScriptSpeaker::run (Ghidra 0x38f... the speaker part)
bool CInterfaceIngame::showSpeakerText(CPlayer* player, const CScriptSpeaker& sp) {
    CInterfaceIngamePerPlayer* p = getInterfaceForPlayer(player);
    if (!p) return false;
    if (sp.mIconName.empty()) p->setOverlayText(sp.mText, sp.mCmdText, sp.mShowTime, sp.mId, sp.mNoPause, false, "");
    else p->setOverlayText(sp.mText, sp.mCmdText, sp.mShowTime, sp.mId, sp.mNoPause, true, sp.mIconName);
    if (sp.mHasScrollTo) p->getCamera()->slowScrollTo(sp.mScrollTo, 1.0f);
    uisound::play("ringringring");
    return true;
}

}  // namespace py
