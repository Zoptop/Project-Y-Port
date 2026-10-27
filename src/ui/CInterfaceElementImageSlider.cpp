#include "CInterfaceElementImageSlider.h"

#include <cstdio>

#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CCameraMenu.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTextRender.h"
#include "render/CTexture.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {
constexpr unsigned GL_TEXTURE0_ = 0x84C0;
}

// Ghidra 0x18ecc8
CInterfaceElementImageSlider::CInterfaceElementImageSlider(CInterfaceMenu* menu, int action, float x, float y,
                                                           float w, float h, const std::string& title, float size,
                                                           const Vec3& color)
    : CInterfaceElement(menu, x, y), mTitle(title), mColor(color), mSize(size) {
    mAction = action;
    mHitX = mX;
    mHitY = mY;
    mW = w;
    mHitW = w;
    mH = h;
    mHitH = h;
    mTexture = nullptr;
}

// Ghidra 0x18ee1c
CInterfaceElementImageSlider::CInterfaceElementImageSlider(
    CInterfaceMenu* menu, int action, float x, float y, float titleX, float titleY, float nameX, float nameY,
    float line1X, float line1Y, float line2X, float line2Y, float touchX, float touchY, float w, float h,
    const std::string& title, const std::string& subMesh, float size, const Vec3& color, float hitW, float hitH,
    float hitOffX, float hitOffY)
    : CInterfaceElement(menu, x, y), mTitle(title), mColor(color), mSize(size), mSubMesh(subMesh) {
    mAction = action;
    mTitleX = titleX;
    mTitleY = titleY;
    mNameX = nameX;
    mNameY = nameY;
    mLine1X = line1X;
    mLine1Y = line1Y;
    mLine2X = line2X;
    mLine2Y = line2Y;
    mTouchX = touchX;
    mTouchY = touchY;
    mW = w;
    mH = h;
    mTexture = nullptr;
    mHitX = hitOffX + mX;
    mHitY = hitOffY + mY;
    mHitW = hitW;
    mHitH = hitH;
}

// Ghidra 0x18eff8 (the preview texture stays in the texture manager)
CInterfaceElementImageSlider::~CInterfaceElementImageSlider() = default;

// Ghidra 0x18fce0
void CInterfaceElementImageSlider::addEntry(unsigned id, const std::string& name, const std::string& line1,
                                            const std::string& line2, CTexture* texture, bool purchased,
                                            bool downloaded, unsigned numPlayers, unsigned revision,
                                            unsigned currentRevision, const std::string& line3,
                                            const std::string& line4) {
    auto e = std::make_unique<CEntry>();
    e->id = id;
    e->name = name;
    e->line1 = line1;
    e->line2 = line2;
    e->line3 = line3;
    e->line4 = line4;
    e->purchased = purchased;
    e->downloaded = downloaded;
    e->texture = texture;
    e->numPlayers = numPlayers;
    e->revision = revision;
    e->currentRevision = currentRevision;
    mEntries.emplace(id, std::move(e));   // [!] the original leaks the CEntry of a duplicate id
}

CInterfaceElementImageSlider::CEntry& CInterfaceElementImageSlider::current() {
    std::unique_ptr<CEntry>& e = mEntries[mCurrentId];
    if (!e) e = std::make_unique<CEntry>();   // [PC] the original reads through a null CEntry* here
    return *e;
}

unsigned CInterfaceElementImageSlider::nextId() const {
    if (mEntries.empty()) return mCurrentId;
    auto it = mEntries.find(mCurrentId);
    if (it != mEntries.end()) ++it;
    if (it == mEntries.end()) it = mEntries.begin();
    return it->first;
}

// Ghidra 0x18ec68
void CInterfaceElementImageSlider::changeTexture(const std::string& file) {
    if (mTexture) {
        CTextureManager::get().removeTexture(mTexture->name());
        mTexture = nullptr;
    }
    if (!gl::isLoaded()) return;
    mTexture = CTextureManager::get().getTexture(file, 1, 2, 2, true);
}

// Ghidra 0x18ea1c
bool CInterfaceElementImageSlider::isHit(CInterfaceEvent* e) {
    const CVector2 p = mMenu->mCamera->viewportPlaneHit(CVector2(e->x, e->y));
    return p.x > mHitX && p.x < mHitX + mHitW && p.y > mHitY - mHitH && p.y < mHitY;
}

// Ghidra 0x18eab4 (no disabled check in the base class)
int CInterfaceElementImageSlider::processEventActionUp(CInterfaceEvent* e) {
    if (!isHit(e)) return 0;
    mCurrentId = nextId();
    mMenu->processAction(mAction, this);
    return 1;
}

// Ghidra 0x1901c8
int CInterfaceElementMapImageSlider::processEventActionUp(CInterfaceEvent* e) {
    if (mDisabled) return 0;
    if (!isHit(e)) return 0;
    mCurrentId = nextId();
    char file[256];
    std::snprintf(file, sizeof file, "map%uicon.png", mCurrentId);
    changeTexture(file);
    CEntry& en = current();
    mMenu->mMapOutdated = en.revision < en.currentRevision;   // menu+0x5fc
    en.purchased = mEngine->isMapPurchased(mCurrentId);
    en.downloaded = mMenu->mMapOutdated ? false : mEngine->isMapDownloaded(mCurrentId);
    mMenu->processAction(mAction, this);
    return 1;
}

// Ghidra 0x18f17c (see the header for the layout)
void CInterfaceElementImageSlider::draw() {
    CTextRender* tr = mEngine->getTextRender();
    if (tr) {
        const Vec3& c = mColor;
        const float s = mSize;
        tr->drawStringDirect(mTitleX, mTitleY, 0.0f, s, c.x, c.y, c.z, 1.0f, 2, "%s", formatNoArgs(mTitle).c_str());
        CEntry& en = current();
        tr->drawStringDirect(mNameX, mNameY + 2.0f, 0.0f, s * 0.75f, c.x * 0.75f, c.y * 0.75f, c.z * 0.75f, 1.0f, 0,
                             "%s", formatNoArgs(en.name).c_str());
        char rev[100];
        if (en.revision < en.currentRevision) {
            std::snprintf(rev, sizeof rev, CTxtLoc::get().getC(0x11b), en.revision, en.currentRevision);
            tr->drawStringDirect(mNameX, mNameY - 5.0f, 0.0f, s * 0.4f, c.x * 0.75f, c.y * 0.25f, c.z * 0.25f, 1.0f,
                                 0, "%s", formatNoArgs(rev).c_str());
        } else {
            std::snprintf(rev, sizeof rev, CTxtLoc::get().getC(0x11a), en.revision);
            tr->drawStringDirect(mNameX, mNameY - 5.0f, 0.0f, s * 0.4f, c.x * 0.75f, c.y * 0.75f, c.z * 0.75f, 1.0f,
                                 0, "%s", formatNoArgs(rev).c_str());
        }
        const float s75 = s * 0.75f;
        const Vec3 c75(c.x * 0.75f, c.y * 0.75f, c.z * 0.75f);
        tr->drawStringDirect(mLine1X, mLine1Y, 0.0f, s75, c75.x, c75.y, c75.z, 1.0f, 0, "%s",
                             formatNoArgs(en.line1).c_str());
        tr->drawStringDirect(mLine2X, mLine2Y, 0.0f, s75, c75.x, c75.y, c75.z, 1.0f, 0, "%s",
                             formatNoArgs(en.line2).c_str());
        if (!en.line3.empty())
            tr->drawStringDirect(mLine3X, mLine3Y, 0.0f, s * 0.6f, c75.x, c75.y, c75.z, 1.0f, 0, "%s",
                                 formatNoArgs(en.line3).c_str());
        if (!en.line4.empty())
            tr->drawStringDirect(mLine4X, mLine4Y, 0.0f, s * 0.6f, c75.x, c75.y, c75.z, 1.0f, 0, "%s",
                                 formatNoArgs(en.line4).c_str());
        if (!mDisabled)
            tr->drawStringDirect(mTouchX, mTouchY - (s + s), 0.0f, s * 0.6f, c75.x, c75.y, c75.z, 1.0f, 1, "%s",
                                 CTxtLoc::get().getC(0x9a));
        if (en.purchased && !en.downloaded)
            tr->drawStringDirect(mTouchX, mTouchY, 0.0f, s75, c75.x, c75.y, c75.z, 1.0f, 1, "%s",
                                 CTxtLoc::get().getC(0x9e));
        tr->drawStringDirect(mLine2X, mLine2Y, 0.0f, s75, c75.x, c75.y, c75.z, 1.0f, 0, "%s",
                             formatNoArgs(en.line2).c_str());   // [!] queued twice in the original
    }
    CGLSLProgram* prog = mMenu->mProgramSimpleAlpha;
    if (!mTexture || !prog || !mMenu->mMesh || !gl::isLoaded()) return;
    mMenu->mMesh->bindBuffer();
    mTexture->bind(GL_TEXTURE0_);
    prog->startDraw();
    const CEntry& en = current();
    if (!en.purchased)
        prog->setUniformBlendColor(1.0f, 0.0f, 0.0f, bitsf(0x3f333333));
    else if (!en.downloaded)
        prog->setUniformBlendColor(0.0f, 0.0f, 1.0f, bitsf(0x3f333333));
    else
        prog->setUniformBlendColor(0.0f, 0.0f, 0.0f, 1.0f);
    prog->setUniformMVPMatrix(mMenu->mCamera->getViewProjMatrix());
    drawSubMesh(mMenu->mMesh, mSubMesh);
    prog->endDraw();
}

}  // namespace py
