// CInterfaceMenu.h - port of ProjectY::CInterfaceMenu (0x6cc bytes), the
// menu screens. The menu is one 3D scene (menu.3DS.mesh seen from above by a
// CCameraMenu); every screen ("menu type") is a set of elements placed on the
// z = 0 plane around its own spot, and changing screens scrolls the camera
// there (CCameraMenu::slowScrollTo, speed 10).
//
// Flow: processAction(action, element) changes the requested screen (+0x90)
// and starts the camera scroll; CInterfaceMenu::processEventActionUp /
// processEventBackKeyPressed then call setupElements(+0x90) when it differs
// from the current screen (+0x8c) or a rebuild is forced (+0x5bc).
// setupElements deletes all elements and builds the screen's set.
//
// Draw modes (+0x80): 0 the menu scene; 1 the loading screen (one frame,
// then 2); 2 starts the game chosen by +0x7c (1 single player, 2 split
// screen, 3 load the save +0x84, 4 network game); 3 the campaign end credits.
//
// Files: CInterfaceMenu.cpp (scene, input, quads, game start),
// CInterfaceMenuActions.cpp (processAction), CInterfaceMenuScreens.cpp
// (setupElements and the createElement* builders).
// See docs/re/ui_phase5.md "Phase 5d" for the screen list and what is ported.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "game/CCampaign.h"
#include "render/CQuad.h"
#include "render/Math3D.h"
#include "ui/CInterface.h"

namespace py {

class CCameraMenu;
struct CPlayerSetup;
class CGLSLProgram;
class CInterfaceElement;
class CInterfaceElementButton2;
class CInterfaceElementImageSlider;
class CInterfaceElementMapImageSlider;
class CInterfaceElementPlayerControl;
class CInterfaceElementText;
class CInterfaceElementTextSlider2;
class CInterfaceElementValueBar;
class CMesh;
class CTexture;
struct CTextListing;

// EMENUTYPES (setupElements' jump table at Ghidra 0x1abda8)
enum EMenuType : int {
    MENU_MAIN = 0,
    MENU_SINGLEPLAYER = 1,
    MENU_MULTIPLAYER = 2,
    MENU_PROFILE = 3,
    MENU_INGAME = 4,
    MENU_OPTIONS = 5,
    MENU_OPTIONS2 = 6,
    MENU_SAVEGAME = 7,
    MENU_LOADGAME = 8,
    MENU_TUTORIAL = 9,
    MENU_CUSTOMGAME = 10,
    MENU_STATISTICS = 11,
    MENU_AFTERMATH = 12,
    MENU_DOWNLOAD = 13,
    MENU_CREDITS = 14,
    MENU_SPLITSCREEN = 15,
    MENU_HOSTSERVER = 16,
    MENU_ENTERIP = 17,
    MENU_SERVERLIST = 18,
    MENU_LOBBYCHAT = 19,
    MENU_WAITINGROOM = 20,
    MENU_UPDATENEWS = 21,
    MENU_CAMPAIGN = 25,
    MENU_CAMPAIGNPROGRESS = 26,   // the episode list (CInterfaceElementCampaignButtons)
    MENU_SURRENDER = 30,
};

class CInterfaceMenu : public CInterface {
public:
    CInterfaceMenu();             // Ghidra 0x1b07dc
    ~CInterfaceMenu() override;   // Ghidra 0x1b0b9c

    void init(int menuType);                                     // Ghidra 0x1cd118
    void setupElements(int menuType, int action = 0);            // Ghidra 0x1bbcb8
    void processAction(int action, CInterfaceElement* element);  // Ghidra 0x1cd3f8
    void processBackButton();                                    // Ghidra 0x1d3ae8

    // Ghidra 0x1af6cc: the local player (the last human slot [!]), a new
    // CGame, the ingame interface, which becomes current; resumes the game.
    void initSingleplayerGame();
    // initSingleplayerGame's local player: [!] the LAST human slot among
    // 1..n (1 if there is none)
    static unsigned localPlayerFromSetup(const CPlayerSetup* setup, unsigned n);
    void initMultiplayerSplitGame();     // Ghidra 0x1af614 (split screen: notPorted, falls back to single player)
    void initMultiplayerNetworkGame();   // Ghidra 0x1af7fc (notPorted)
    // processAction 0x50 / 0x52 / 0x53: start the current campaign
    // episode (see the .cpp)
    void startCurrentCampaignEpisode();

    // ---- CInterface
    void draw() override;                                             // Ghidra 0x1d3b10
    void timeUpdate(float dt) override;                               // Ghidra 0x1ad558
    void processEventBackKeyPressed(CInterfaceEvent* e) override;     // Ghidra 0x1d59e8
    void processEventMenuKeyPressed(CInterfaceEvent* e) override;     // Ghidra 0x1d5a38
    void processEventActionUp(CInterfaceEvent* e) override;           // Ghidra 0x1d5a88
    void processEventActionDown(CInterfaceEvent* e) override;         // Ghidra 0x1ad5f4
    void processEventActionMove(CInterfaceEvent* e) override;         // Ghidra 0x1ad670
    void processEventMenuAction(CInterfaceEvent* e) override;         // Ghidra 0x1d3b04
    void processTextInput(std::string text, bool done) override;      // Ghidra 0x1ad9b4
    void setResolution(int width, int height) override;               // Ghidra 0x1ad488
    CCamera* getCamera() override;                                    // Ghidra 0x1ad480
    // [PC] the wheel steps the slider under the mouse; keys and text go to
    // the active text input (CInterfaceElementTextInput2)
    void processEventMouseWheel(CInterfaceEvent* e) override;
    void processEventKeyDown(CInterfaceEvent* e) override;
    void processEventTextInput(CInterfaceEvent* e) override;
    // [PC] a key the text field used (Escape cancelling an edit) is not also Back
    bool keysEmulateAndroidKeys() const override { return !mKeyConsumed; }

    // ---- quads (Ghidra 0x1ae0ec..0x1b047c). World quads on the menu plane:
    // centre, interface.png uv/uv size, half extents, colour (interfacecolored.gfx).
    void addColoredQuad(const Vec3& center, const CVector2& uv, const CVector2& uvSize, const CVector2& half,
                        const Vec3& color);
    void drawColoredQuads();
    // Screen-space quads in pixels (origin top-left): textured (select.gfx,
    // +0x650) and coloured (interfacecoloredalpha.gfx, +0x694)
    void addStandardQuadPixel(const CVector2& pos, const CVector2& uv, const CVector2& uvSize, const CVector2& size);
    void drawScreenSpaceQuads();
    void addColored4QuadPixel(const CVector2& pos, const CVector2& uv, const CVector2& uvSize, const CVector2& size,
                              const Vec3& color, float alpha);
    void drawScreenSpaceQuadsColored4();

    // Takes ownership; returns the element (setupElements helper).
    template <class T>
    T* addElement(T* e) {
        mElements.emplace_back(e);
        return e;
    }
    void clearElements();

    // ---- setupElements builders (Ghidra addresses in the .cpp)
    void createElementMapSlider(const std::string& name, float x, float y, float dx, float dy,
                                const std::string& subMesh, bool disabled);
    void createElementMapSliderFixed(const std::string& name, float x, float y, float dx, float dy,
                                     const std::string& subMesh, unsigned mapId);
    void createElementsFOWSelector(const std::string& subMesh, float x, float y, int align, bool disabled);
    void createElementCrateSelector(const std::string& subMesh, float x, float y, int align, bool disabled);
    void createElementsGameTypeSettings(const std::string& subMesh, float x, float y, int align, bool disabled);
    void createElementsGameSettings(const std::string& subMesh, float x, float y, int align, bool disabled);
    void createElementsSectorSettings(const std::string& subMesh, float x, float y, int align, bool disabled);
    void createElementsUnitSettings(const std::string& subMesh, float x, float y, int align, bool disabled);
    void createElementDifficultySlider(float x, float y, const std::string& subMesh);
    void createElementsPlayerSettings(const std::string& name, float x, float y, unsigned maxHumans, bool disabled);
    void createElementsPlayerSettingsNetworked(const std::string& name, float x, float y, unsigned maxHumans,
                                               bool disabled);
    void createElementsOptionsA();
    void createElementsOptionsB();
    void createElementStatistics(float x, float y);
    // Ghidra 0x1bb4f4: the four user slots (actions 0x1a..0x1d) and, for
    // loading, the autosave (0x1e), 19 units apart
    void createElementsLoadSave(float x, float y, const std::string& prefix, bool load);
    // Ghidra 0x1ba838: one slot (file <prefix><name> in the save dir): its
    // header's first four lines (date, map, game time, sectors) or "Empty
    // slot", see the .cpp
    void createElementLoadSave(float x, float y, const std::string& name, const std::string& prefix,
                               const Vec3& color, int action, int slot, bool load, const std::string& mesh);
    // Ghidra 0x1af8a0: the file's header says a campaign mission (line 7) of
    // a loadable revision (line 5)
    bool isSavegameFromCampaign(const std::string& name, const std::string& prefix) const;
    // Ghidra 0x1afae4: any of autosave, usersave1..4
    bool areSavegamesFromCampaign(const std::string& prefix) const;
    void createElementCampaignSlider(const std::string& name, float x, float y, const std::string& subMesh,
                                     unsigned campaignId);

    // Where processAction scrolls the camera for a screen (the slowScrollTo
    // targets of the original's processAction cases; (0, 0) for the main menu)
    static CVector2 screenCenter(int menuType);

    // ---- members (original offsets)
    int mHeight = 0;                                          // +0x70
    int mWidth = 0;                                           // +0x74
    std::unique_ptr<CCameraMenu> mCamera;                     // +0x78
    int mStartGameMode = 0;                                   // +0x7c 1 single, 2 split, 3 load, 4 network
    int mDrawMode = 0;                                        // +0x80
    std::string mLoadFile;                                    // +0x84
    std::string mSaveFile;                                    // +0x88
    int mCurrentMenu = -1;                                    // +0x8c
    int mNextMenu = 0;                                        // +0x90
    std::vector<std::unique_ptr<CInterfaceElement>> mElements;   // +0x94
    CGLSLProgram* mProgramSimple = nullptr;                   // +0xa0 simple.gfx
    CGLSLProgram* mProgramSimpleAlpha = nullptr;              // +0xa4 simplealpha.gfx
    CGLSLProgram* mProgramPlayerColorMenu = nullptr;          // +0xa8 playercolormenu.gfx
    CMesh* mMesh = nullptr;                                   // +0xac menu.3DS.mesh
    CTexture* mBackground = nullptr;                          // +0xb0 background.png
    CTexture* mCreditsBlack = nullptr;                        // +0xb4 credits_black.png
    int mSaveSlot = 0;                                        // +0xb8
    std::string mSlotFiles[5];                                // +0xbc (5 x 0x100 bytes)
    bool mForceSetup = false;                                 // +0x5bc
    int mBackAction = 0;                                      // +0x5c0 action of the Back key (0 = none)
    CInterfaceElementButton2* mStartButton = nullptr;         // +0x5c4
    std::vector<CQuadPosUVColor> mColoredQuads;               // +0x5cc (count +0x5c8, 1999 max)
    CGLSLProgram* mProgramInterfaceColored = nullptr;         // +0x5d0 interfacecolored.gfx
    CTexture* mInterfaceTexture = nullptr;                    // +0x5d4 interface.png
    bool mDownloadInProgress = false;                         // +0x5d8
    float mDownloadPollTime = 0.0f;                           // +0x5dc
    CInterfaceElementValueBar* mDownloadBar = nullptr;        // +0x5e4
    CInterfaceElementText* mDownloadStatus = nullptr;         // +0x5e8
    CInterfaceElementText* mDownloadTitle = nullptr;          // +0x5ec
    CInterfaceElementMapImageSlider* mMapSlider = nullptr;    // +0x5f0
    class CInterfaceElementCampaignImageSlider* mCampaignSlider = nullptr;   // +0x5f4
    std::string mSelectedMapName;                             // +0x5f8
    bool mMapOutdated = false;                                // +0x5fc
    CInterfaceElementPlayerControl* mPlayerControl = nullptr; // +0x608
    class CInterfaceElementTextInput2* mTextInput = nullptr;  // +0x60c the input the text dialog answers
    CInterfaceElementButton2* mReadyButton = nullptr;         // +0x61c
    CInterfaceElementButton2* mCampaignStartButton = nullptr; // +0x620
    CInterfaceElementButton2* mCampaignReplayButton = nullptr;   // +0x624
    int mLanServerList = 0;                                   // +0x62c 1 = LAN discovery, 0 = public
    CGLSLProgram* mProgramSelect = nullptr;                   // +0x648 select.gfx
    std::vector<CQuadPosUV> mStandardQuads;                   // +0x650 (count +0x64c, 999 max)
    float mCreditsScroll = 0.0f;                              // +0x654
    std::vector<CTextListing> mCreditsText;                   // +0x658 (the original's vector* , leaked)
    int mModalState = 0;                                      // +0x65c
    int mActionAfterLongOp = 0;                               // +0x660
    int mRankedOption = 0;                                    // +0x664
    int mTop10Option = 2;                                     // +0x668
    CInterfaceElement* mTouchedElement = nullptr;             // +0x690
    std::vector<CQuadPosUVColor4> mColor4Quads;               // +0x694 (count +0x698, 999 max)
    CGLSLProgram* mProgramInterfaceColoredAlpha = nullptr;    // +0x69c interfacecoloredalpha.gfx
    std::string mDownloadMapName;                             // +0x6c8
    class CInterfaceElementTextInput2* mActiveTextInput = nullptr;   // [PC] the text input that has the keyboard
    bool mKeyConsumed = false;                                // [PC] see keysEmulateAndroidKeys
};

}  // namespace py
