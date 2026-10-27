// CInterfaceIngame.h - port of ProjectY::CInterfaceIngame (Ghidra
// 0x19dc18..0x1a0df0, 0xf0 bytes), the ingame screen (engine +0x170).
//
// It owns one CInterfaceIngamePerPlayer (IPP) per local player (+0x98, and
// +0x9c in split screen), routes the touch pointers to them and holds the
// quad buffers every HUD element queues into (the IPP's draw* functions
// flush them):
//   standard quads (CQuadPosUV, world space, select.gfx)          +0x60 / +0x78
//   screen quads (CQuadPosUV, screen space, select.gfx)           +0x64 / +0x7c
//   coloured quads (CQuadPosUVColor, world, interfacecolored.gfx) +0x68 / +0x80
//   coloured4 quads (CQuadPosUVColor4, world, ...alpha.gfx)       +0x6c / +0x8c
//   coloured screen quads (CQuadPosUVColor, screen)               +0x70 / +0x84
//   coloured4 screen quads (CQuadPosUVColor4, screen)             +0x74 / +0x88
// (999 quads each), and the selection/range circles (+0xb4, circletest.3DS.mesh
// with circleuniform.gfx).
//
// The "...Pixel" add functions take pixel coordinates (origin top-left, y
// down) and convert them to the screen space of the current camera (x in
// [-aspect, aspect], y in [-1, 1], y up). Only display rotation 0 is ported
// (the PC has no rotated display); split screen keeps its structure but the
// port creates one IPP (initSingleplayer).
//
// [PC] Mouse and keyboard are handled directly (mouseEmulatesTouch() and
// keysEmulateAndroidKeys() return false) and routed to the IPP's PC handlers
// (CInterfaceIngamePerPlayerPC.cpp); SDL finger events still arrive as the
// original touch events.
#pragma once

#include <memory>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "game/CScriptManager.h"
#include "game/IIngameUI.h"
#include "render/CIngameQuads.h"
#include "render/CQuad.h"
#include "ui/CInterface.h"

namespace py {

class CCamera;
class CGLSLProgram;
class CInterfaceIngamePerPlayer;
class CMesh;
class CPlayer;
class CTexture;

class CInterfaceIngame : public CInterface, public IIngameUI, public IScriptUI {
public:
    static constexpr int kMaxQuads = 999;

    using CCircleInfo = CIngameQuads::CCircleInfo;   // a queued selection/range circle (0x20 bytes)

    CInterfaceIngame();             // Ghidra 0x19e154
    ~CInterfaceIngame() override;   // Ghidra 0x19e1d4

    // ---- CInterface vtable
    void draw() override;                                            // Ghidra 0x19dc2c
    void timeUpdate(float dt) override;                              // Ghidra 0x19dcd0
    void processEventBackKeyPressed(CInterfaceEvent* e) override;    // Ghidra 0x19dc18
    void processEventMenuKeyPressed(CInterfaceEvent* e) override;    // Ghidra 0x19dd4c
    void processEventActionUp(CInterfaceEvent* e) override;          // Ghidra 0x19ff84
    void processEventActionDown(CInterfaceEvent* e) override;        // Ghidra 0x19fc40
    void processEventActionMove(CInterfaceEvent* e) override;        // Ghidra 0x19fe38
    void setResolution(int width, int height) override;              // Ghidra 0x19de48
    CCamera* getCamera() override;                                   // Ghidra 0x19dcc0
    void processTextInput(std::string text, bool cancelled) override;   // Ghidra 0x19dd78

    // ---- [PC] input (routed to the current IPP)
    void processEventMouseButtonDown(CInterfaceEvent* e) override;
    void processEventMouseButtonUp(CInterfaceEvent* e) override;
    void processEventMouseMove(CInterfaceEvent* e) override;
    void processEventMouseWheel(CInterfaceEvent* e) override;
    void processEventKeyDown(CInterfaceEvent* e) override;
    void processEventKeyUp(CInterfaceEvent* e) override;
    bool mouseEmulatesTouch() const override { return false; }       // [PC]
    bool keysEmulateAndroidKeys() const override { return false; }   // [PC]
    int getCursorHint() const override;                              // [PC]

    // ---- setup
    void init();                                                  // Ghidra 0x19e27c (GL resources)
    void initSingleplayer(int width, int height, unsigned player);   // Ghidra 0x19e7fc
    void initSplitscreen(int width, int height, unsigned p1, unsigned p2);   // Ghidra 0x19e734
    // [PC] the --skirmish shell option and the tests: CInterfaceMenu::
    // initSingleplayerGame without the menu (setupSkirmish(map, players):
    // player 1 human, the others AI; option UseFOW = fow), the new ingame
    // interface made current. Returns it, or null if the map did not load.
    // `players` below the map's "set numplayers" is raised to it (see
    // skirmishPlayerCount).
    static CInterfaceIngame* startSkirmish(unsigned mapId, unsigned players, bool fow, int width, int height,
                                           int gameType = 0);
    // [PC] the map script's "set numplayers" (0 if none), and the player count
    // a skirmish of `requested` players starts with (at least that).
    static unsigned mapNumPlayers(unsigned mapId);
    static unsigned skirmishPlayerCount(unsigned mapId, unsigned requested);

    // ---- quad queues (Ghidra addresses in the .cpp)
    void addSelectionQuad(const Vec3& pos, float size);
    void addStandardQuad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize, float half);
    void addStandardQuadScreenSpace(const CVector2& pos, const CVector2& uvTL, const CVector2& uvSize,
                                    const CVector2& size);
    void addStandardQuadPixel(const CVector2& topLeft, const CVector2& uvTL, const CVector2& uvSize,
                              const CVector2& size);
    void addColored4Quad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize, const CVector2& half,
                         const Vec3& color, float alpha);
    void addColored4QuadPixel(const CVector2& topLeft, const CVector2& uvTL, const CVector2& uvSize,
                              const CVector2& size, const Vec3& color, float alpha, float yaw = 0.0f);
    void addColoredQuadPixel(const CVector2& topLeft, const CVector2& uvTL, const CVector2& uvSize,
                             const CVector2& size, const Vec3& color);
    void addStandardQuadScreenSpaceAdjustOnlyAspect(const CVector2& pos, const CVector2& uvTL,
                                                    const CVector2& uvSize, const CVector2& size);
    void addColoredQuadScreenSpaceAdjustOnlyAspect(const CVector2& pos, const CVector2& uvTL,
                                                   const CVector2& uvSize, const CVector2& size,
                                                   const Vec3& color);
    void addColoredQuad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize, float half,
                        const Vec3& color);
    void addColoredQuad(const Vec3& center, const CVector2& uvTL, const CVector2& uvSize, const CVector2& half,
                        const Vec3& color);
    // Ghidra 0x1a0df0 (the width argument is not stored)
    void addCircleQuad(const Vec3& pos, float radius, float width, const Vec3& color, float alpha);
    void drawCircle(const CCircleInfo& c, CCamera* cam);   // Ghidra 0x1a028c
    void startDrawCircle();                                // Ghidra 0x1a0d38
    void endDrawCircle();                                  // Ghidra 0x1a0d78

    // ---- IIngameUI (game objects -> HUD; installed while this interface exists)
    bool hasInterface(CPlayer* player) override;
    void addEvent(CPlayer* player, int type, unsigned objId, const CVector2& position, bool unique,
                  const std::string& shortText) override;
    void enableUnderAttackWarning(CPlayer* player, unsigned unitId) override;
    unsigned getFirstGroupUnit(CPlayer* player, unsigned groupId) override;
    void addUnitToGroup(CPlayer* player, unsigned groupId, unsigned unitId) override;
    bool isGameObjectSelected(CPlayer* player, unsigned id) override;
    void selectGameObject(CPlayer* player, unsigned id) override;
    void unselectGameObject(CPlayer* player, unsigned id) override;
    void addUnitToGroupOfUnit(CPlayer* player, unsigned groupUnit, unsigned unit) override;
    bool isCurrentInterface() override;
    void highlightRallyGroup(CPlayer* player, unsigned buildingId, unsigned groupId) override;
    void showGameEndText(CPlayer* player, const std::string& text, int callerScript,
                         const CVector2* scrollTo) override;
    // ---- IScriptUI (the scripts' queries and the speaker)
    bool isUnitInGroup(CPlayer* player, unsigned id) override;
    bool showSpeakerText(CPlayer* player, const CScriptSpeaker& speaker) override;

    // ---- players
    CInterfaceIngamePerPlayer* getCurrentInterfacePerPlayer() const;   // Ghidra 0x19fac8
    CPlayer* getCurrentInterfacePlayer() const;                        // Ghidra 0x1a0154
    CInterfaceIngamePerPlayer* getInterfaceForPlayer(CPlayer* player) const;   // Ghidra 0x1a0168
    bool isLocalPlayer(CPlayer* player) const;                         // Ghidra 0x1a0da0
    CInterfaceIngamePerPlayer* ipp(int i) const { return mIPP[i]; }

    // split-screen pointer routing
    bool updateSplitscreenCoords(CVector2& pos) const;   // Ghidra 0x19fad8
    bool isPointerAssignedSingle(unsigned id) const;     // Ghidra 0x19fbcc
    bool isPointerAssignedSplit(unsigned id) const;      // Ghidra 0x19fbf8
    int getInterfaceAssignedSplit(unsigned id) const;    // Ghidra 0x19fde8

    // ---- fields (original offsets)
    CGLSLProgram* mSelectShader = nullptr;          // +0x3c select.gfx
    CGLSLProgram* mColoredShader = nullptr;         // +0x40 interfacecolored.gfx
    CGLSLProgram* mColoredMMShader = nullptr;       // +0x44 interfacecolored_mm.gfx
    CGLSLProgram* mColoredAlphaShader = nullptr;    // +0x48 interfacecoloredalpha.gfx
    CGLSLProgram* mCircleShader = nullptr;          // +0x4c circle.gfx
    CGLSLProgram* mCircleUniformShader = nullptr;   // +0x50 circleuniform.gfx
    CTexture* mInterfaceTexture = nullptr;          // +0x54 interface.png
    float mSelectionQuadZ = 0.0f;                   // +0x58
    float mSelectionQuadYaw = 0.0f;                 // +0x5c (advances with the game time)
    // the world-space queues (+0x60/+0x78 standard, +0x68/+0x80 coloured,
    // +0x6c/+0x8c coloured4, +0xb4 circles): the game objects' draw code
    // fills them through CDrawContext::quads (phase 5b), so they live in a
    // CIngameQuads that this interface owns
    CIngameQuads mWorldQuads;
    int mNumScreenQuads = 0;                        // +0x64
    int mNumColoredScreenQuads = 0;                 // +0x70
    int mNumColored4ScreenQuads = 0;                // +0x74
    std::vector<CQuadPosUV> mScreenQuads;           // +0x7c
    std::vector<CQuadPosUVColor> mColoredScreenQuads;   // +0x84
    std::vector<CQuadPosUVColor4> mColored4ScreenQuads; // +0x88
    int mSplitMode = 0;                             // +0x90 0 single, 1 split screen
    int mCurrentIPP = 0;                            // +0x94
    CInterfaceIngamePerPlayer* mIPP[2] = {nullptr, nullptr};   // +0x98/+0x9c
    int mPointerCounter = 1;                        // +0xa0 order of pointer downs
    int mHeight = 1;                                // +0xa4
    int mWidth = 1;                                 // +0xa8
    bool mLandscape = true;                         // +0xac
    CMesh* mCircleMesh = nullptr;                   // +0xb0 circletest.3DS.mesh
    uint32_t mCircleSub[6][2] = {};                 // +0xc0 circle10/25/50/100/150/200 (first, count)

private:
    std::unique_ptr<CInterfaceIngamePerPlayer> mOwned[2];
};

}  // namespace py
