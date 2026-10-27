// CInterfaceIngamePerPlayer.h - port of ProjectY::CInterfaceIngamePerPlayer
// (Ghidra 0x1a10e4..0x1abf54, 0x170 bytes): the HUD, camera and input
// state of one local player ("IPP").
//
// Touch state machine (processEventActionDown/Move/Up and timeUpdate; see
// docs/re/scripts_rendering_ui.md C.2), selection and orders
// (processTouch -> getAvailableActions -> processBestAction{Single|Long}Touch
// -> tryAction*, C.3). Every order goes through the game's command manager
// (CGame::getCommandManager, game +0x178); tryActionSelectGameObject and the
// menu actions only change the selection / open a CDialMenu.
//
// [PC] The mouse/keyboard mapping of C.6 lives in CInterfaceIngamePerPlayerPC.cpp
// (processPC* and timeUpdatePC); every PC-only behaviour is marked [PC].
//
// Drawing (CInterfaceIngamePerPlayerDraw.cpp) follows the original order
// (B.1). The world-rendering calls go through CIngameWorldDraw.h so the
// placeholder renderer can be swapped for the real one in one place.
#pragma once

#include <list>
#include <string>
#include <vector>

#include "core/CVector2.h"
#include "core/CVector3.h"
#include "render/Math3D.h"

namespace py {

class CActionBar;
class CCamera;
class CCameraLevel;
class CDialMenu;
class CEngine;
class CEventBar;
class CGameObject;
class CGroupBar;
class CInterfaceIngame;
class CPlayer;
class CTexture;
struct CInterfaceEvent;

// getAvailableActions bits (docs/re/scripts_rendering_ui.md C.3)
enum EIngameAction : unsigned {
    ACTION_SELECT = 0x1,
    ACTION_ENTER = 0x2,
    ACTION_ATTACK = 0x4,
    ACTION_MOVE = 0x8,
    ACTION_LOOKAT = 0x10,
    ACTION_RALLY = 0x20,
    ACTION_SPECOP = 0x40,
    ACTION_CALLCOMMANDER = 0x80,
    ACTION_UPGRADEMENU = 0x100,
    ACTION_PRODUCTIONMENU = 0x200,
    ACTION_WEAPONMENU = 0x400,
    ACTION_DEFEND = 0x800,
    ACTION_GATHER = 0x1000,
    ACTION_RETURNRES = 0x2000,
};

// The order in which processBestAction{Single|Long}Touch try the actions
// (Ghidra 0x1a9560 / 0x1a96e8); exposed for the tests and the cursor hint.
extern const unsigned kSingleTouchPriority[13];
extern const unsigned kLongTouchPriority[14];

// [PC] cursor shapes chosen from the hover actions (CInterface::getCursorHint)
enum ECursorHint : int { CURSOR_DEFAULT = 0, CURSOR_SELECT, CURSOR_ATTACK, CURSOR_MOVE, CURSOR_COMMAND, CURSOR_MENU };

class CInterfaceIngamePerPlayer {
public:
    // one touch pointer (0x28 bytes at +0x3c and +0x64)
    struct CPointer {
        int id = -1;              // +0x00 (-1 = free)
        CVector2 current;         // +0x04 updateStoredValues
        CVector2 start;           // +0x0c down position (reset to current when a drag starts)
        CVector2 world;           // +0x14 ground point under `start`
        float downTime = 0.0f;    // +0x1c event timestamp (ms)
        bool active = false;      // +0x20
        bool moved = false;       // +0x21
        int order = 0;            // +0x24 CInterfaceIngame +0xa0 at the down
    };

    // a chat line (0x14 bytes): colour, text, age
    struct CChatLine {
        Vec3 color;
        std::string text;
        float age = 0.0f;
    };

    CInterfaceIngamePerPlayer();    // Ghidra 0x1ab788
    ~CInterfaceIngamePerPlayer();   // Ghidra 0x1a10e4

    void init(CInterfaceIngame* parent, CPlayer* player);   // Ghidra 0x1a4a38
    void setPlayer(CPlayer* player);                        // Ghidra 0x1a8120
    // Ghidra 0x1a8164: main camera viewport and the minimap camera
    void setResolution(int width, int height, int x, int y, unsigned orientation);

    // ---- frame
    void draw();                    // Ghidra 0x1a6e3c (CInterfaceIngamePerPlayerDraw.cpp)
    void timeUpdate(float dt);      // Ghidra 0x1a9b24

    // ---- touch input (the original's)
    void processEventActionDown(CInterfaceEvent* e);   // Ghidra 0x1aa9f8
    void processEventActionMove(CInterfaceEvent* e);   // Ghidra 0x1aa1a8
    void processEventActionUp(CInterfaceEvent* e);     // Ghidra 0x1ab0e4
    void updateStoredValues(CInterfaceEvent* e);       // Ghidra 0x1aa130
    int getActivePointerNum() const;                   // Ghidra 0x1a40fc
    CPointer* getFirstActivePointer();                 // Ghidra 0x1a476c
    CPointer* getSecondActivePointer();                // Ghidra 0x1a47f0
    CPointer* getPointer(int id);                      // Ghidra 0x1a47c8
    void setDoubleTouchState(int state);               // Ghidra 0x1a7e78
    void scheduleTouchUpForDoubleTouchTimeout(const CVector2& pos);   // Ghidra 0x1a581c
    void processDoubleTouch(const CVector2& pos);      // Ghidra 0x1a5110
    void processTextInput(const std::string& text, bool cancelled);   // Ghidra 0x1a5264

    // ---- selection and orders
    // [PC] actionFilter: the action bits processTouch may use (all for touch;
    // ACTION_SELECT for a left click, the rest for a right click)
    void processTouch(const CVector2& screenPos, bool longTouch, unsigned actionFilter = ~0u);   // Ghidra 0x1a9888
    unsigned getAvailableActions(CGameObject* sel, CGameObject* target, const CVector2& pos);    // Ghidra 0x1a4128
    bool processBestActionSingleTouch(unsigned mask, CGameObject* target, CGameObject* sel, const CVector2& pos);   // 0x1a9560
    bool processBestActionLongTouch(unsigned mask, CGameObject* target, CGameObject* sel, const CVector2& pos);     // 0x1a96e8
    CGameObject* getTouchedGameObject(const CVector2& world);   // Ghidra 0x1a4824
    bool tryActionSelectGameObject(unsigned mask, CGameObject* target);                         // 0x1a8870
    bool tryActionResReturnToDropoffBuilding(unsigned mask, CGameObject* target, CGameObject* sel);   // 0x1a88dc
    bool tryActionEnterGameObject(unsigned mask, CGameObject* target, CGameObject* sel);        // 0x1a89cc
    bool tryActionAttackGameObject(unsigned mask, CGameObject* target, CGameObject* sel);       // 0x1a8b7c
    bool tryActionDefendGameObject(unsigned mask, CGameObject* target, CGameObject* sel);       // 0x1a8c50
    bool tryActionMoveTo(unsigned mask, CGameObject* sel, const CVector2& pos);                 // 0x1a8d0c
    bool tryActionGatherResourcesAt(unsigned mask, CGameObject* sel, const CVector2& pos);      // 0x1a8dd8
    bool tryActionOpAt(unsigned mask, CGameObject* sel, const CVector2& pos);                   // 0x1a8ea8
    bool tryActionCallCommanderForGameObject(unsigned mask, CGameObject* target);               // 0x1a90b4
    bool tryActionLookAt(unsigned mask, CGameObject* target, CGameObject* sel, const CVector2& pos);   // 0x1a9170
    bool tryActionSetRallypoint(unsigned mask, CGameObject* target, CGameObject* sel, const CVector2& pos);   // 0x1a9270
    bool tryActionShowProductionMenu(unsigned mask, CGameObject* target);                       // 0x1a9428
    bool tryActionShowWeaponChangeMenu(unsigned mask, CGameObject* target);                     // 0x1a9490
    bool tryActionShowUpgradeMenu(unsigned mask, CGameObject* target);                          // 0x1a94f8
    void openDialMenu(CGameObject* object);   // the common body of the three tryActionShow*Menu
    void closeDialMenu();

    void selectGameObject(unsigned id);          // Ghidra 0x1a7318
    void unselectGameObject(unsigned id);        // Ghidra 0x1aa0a0
    void clearGameObjectSelection();             // Ghidra 0x1a12bc
    bool isGameObjectSelected(unsigned id) const;   // Ghidra 0x1a4b60
    void doMultiSelect(const CVector2& p1, const CVector2& p2);   // Ghidra 0x1a7364
    void selectByUnitType(unsigned type);        // Ghidra 0x1a7d34
    void startObjectInfo(unsigned id);           // Ghidra 0x1a85a4
    bool titelBarProcessActionUp(const CVector2& pos, bool longTouch);   // Ghidra 0x1a8788

    // ---- scripts (speaker, timer, counter, marker)
    // Ghidra 0x1a7ea8: showtime 0 = modal (pauses unless noPause or networked)
    void setOverlayText(const std::string& text, const std::string& cmdText, float showTime, int callerScript,
                        bool noPause, bool showIcon, const std::string& iconName);
    void timeUpdateOverlayText();                // Ghidra 0x1a8600
    // the overlay part of processTouch: true when the touch was taken
    bool processOverlayTouch();
    void setMarker(bool enable, unsigned type, unsigned objId);   // Ghidra 0x1a7e98
    void setCounterAndText(unsigned type, unsigned subtype, unsigned count, unsigned required);   // 0x1a856c
    void setTimerAndText(float start, float time, const std::string& text);   // Ghidra 0x1a8584

    // ---- coordinates
    CVector2 screenPixelPosToPixelPos(const CVector2& p) const;   // Ghidra 0x1a5850 (rotation)
    CVector2 screenPosToPixelPos(const CVector2& ndc) const;      // Ghidra 0x1a597c
    // [PC] the pixel position of a world point through the main camera
    // (the projection doMultiSelect / CDialMenu::timeUpdate use); w > 0 if in front
    CVector2 worldToPixel(const Vec3& p, float* w = nullptr) const;
    // the minimap test of Down (requireRect: inside the minimap viewport) / Up
    bool isInMinimap(const CVector2& pixel, CVector2* worldHit, bool requireRect = true) const;

    // ---- drawing (CInterfaceIngamePerPlayerDraw.cpp)
    void drawSelection();                          // Ghidra 0x1a3494
    void drawSelectionRectangle(float alpha);      // Ghidra 0x1a34e0
    void drawTitleBar();                           // Ghidra 0x1a3848
    void drawChatText();                           // Ghidra 0x1a12f8
    void drawTimer();                              // Ghidra 0x1a35d8
    void drawCounter();                            // Ghidra 0x1a18d0
    void drawOverlayText();                        // Ghidra 0x1a2484
    void drawInfoScreen();                         // Ghidra 0x1a21c4
    void drawMarker();                             // Ghidra 0x1a5a58
    void drawCircleQuads();                        // Ghidra 0x1a16cc
    void drawStandardQuads();                      // Ghidra 0x1a405c
    void drawColoredQuads();                       // Ghidra 0x1a17d0
    void drawColored4Quads();                      // Ghidra 0x1a1750
    void drawColoredQuadsMinimap(CCamera* cam);    // Ghidra 0x1a1850
    void drawScreenSpaceQuads(bool keepTexture);   // Ghidra 0x1a1aa0
    void drawScreenSpaceQuadsColored();            // Ghidra 0x1a30f8
    void drawScreenSpaceQuadsColored4();           // Ghidra 0x1a1e4c
    Mat4 screenSpaceMatrix() const;                // the ortho matrix of the three above

    // ---- [PC] mouse and keyboard (CInterfaceIngamePerPlayerPC.cpp)
    void processPCMouseDown(int button, const CVector2& pos, unsigned mods, int clicks, float timeMs);
    void processPCMouseUp(int button, const CVector2& pos, unsigned mods, float timeMs);
    void processPCMouseMove(const CVector2& pos, unsigned buttons, unsigned mods);
    void processPCWheel(float notches, const CVector2& pos);
    void processPCKeyDown(int32_t key, unsigned mods, bool repeat);
    void processPCKeyUp(int32_t key, unsigned mods);
    void timeUpdatePC(float dt);
    int getCursorHint() const;
    bool hudHitTest(const CVector2& pos, bool longTouch);   // title/event/group/action bar
    bool processDialClick(const CVector2& pos);            // [PC] click with the dial menu open
    void selectGroupByIndex(int index, bool assign);       // [PC] number keys

    CCameraLevel* getCamera() const { return mCamera; }          // Ghidra 0x1a475c
    CCamera* getCameraMinimap() const { return mCameraMinimap; } // Ghidra 0x1a4764
    CPlayer* getPlayer() const { return mPlayer; }
    CInterfaceIngame* getParent() const { return mParent; }
    const std::list<unsigned>& selection() const { return mSelection; }
    CDialMenu* getDialMenu() const { return mDialMenu; }
    CActionBar* getActionBar() const { return mActionBar; }
    CGroupBar* getGroupBar() const { return mGroupBar; }
    CEventBar* getEventBar() const { return mEventBar; }

    // ---- fields (original offsets)
    int mDoubleTouchState = 0;          // +0x00 0 idle, 1 first down, 2 first up, 3 second down
    float mDoubleTouchTime = 0.0f;      // +0x04
    CVector2 mScheduledTouch;           // +0x08
    bool mTouchScheduled = false;       // +0x10
    std::vector<CChatLine> mChatLines;  // +0x14
    CInterfaceIngame* mParent = nullptr;   // +0x20
    CPlayer* mPlayer = nullptr;         // +0x24
    CEngine* mEngine = nullptr;         // +0x28
    std::list<unsigned> mSelection;     // +0x2c selected object ids
    bool mPinchStarted = false;         // +0x34
    bool mLongTouchFired = false;       // +0x35
    bool mAllowViewSwitch = true;       // +0x36 pinch may cross the strategy-view boundary
    CDialMenu* mDialMenu = nullptr;     // +0x38
    CPointer mPointer[2];               // +0x3c / +0x64
    unsigned mFollowUnit = 0;           // +0x8c
    CCameraLevel* mCamera = nullptr;    // +0x90
    CCamera* mCameraMinimap = nullptr;  // +0x94
    CEventBar* mEventBar = nullptr;     // +0x98
    CGroupBar* mGroupBar = nullptr;     // +0x9c
    CActionBar* mActionBar = nullptr;   // +0xa0
    std::string mStatusText = "Startup complete";   // +0xa4 (title bar, second line)
    bool mOverlayDisplayed = false;     // +0xa8
    std::string mOverlayText;           // +0xac
    std::string mOverlayCmdText;        // +0xb0
    bool mOverlayWaitsForTouch = false; // +0xb4
    float mOverlayTTL = 0.0f;           // +0xb8 (-1 persistent)
    float mOverlayRuntime = 0.0f;       // +0xbc
    int mOverlayCallerScript = -1;      // +0xc0 (-2 game end)
    bool mOverlayFullyDisplayed = false;   // +0xc4
    bool mActionSoundPlayed = false;    // +0xc5 (reset every frame)
    bool mActionMarkerShown = false;    // +0xc6 (reset every frame)
    bool mOverlayNoPause = false;       // +0xc7
    std::string mOverlayIconName;       // +0xc8
    bool mOverlayShowIcon = false;      // +0xcc
    CTexture* mAvatarTexture = nullptr; // +0xd0
    std::string mAvatarSubtitle;        // +0xd4
    CVector2 mAvatarUV, mAvatarUVSize;  // +0xd8 / +0xe0
    Vec3 mAvatarColor;                  // +0xe8
    bool mOverlayMinimized = false;     // +0xf4
    bool mInfoScreenActive = false;     // +0xf5
    unsigned mInfoScreenObject = 0;     // +0xf8
    float mZoomBeforeInfo = 0.0f;       // +0xfc
    CVector2 mPosBeforeInfo;            // +0x100
    std::string mChatInput;             // +0x108
    bool mChatInputActive = false;      // +0x10c
    bool mRotateMode = false;           // +0x114
    CVector2 mRectMin, mRectMax;        // +0x118 / +0x120 selection rectangle (pixels)
    bool mRectVisible = false;          // +0x128
    float mRectAlpha = 0.0f;            // +0x12c
    bool mMinimapMode = false;          // +0x130
    float mTimerRuntime = 0.0f;         // +0x134
    std::string mTimerText;             // +0x138
    unsigned mCounterType = 0;          // +0x13c 0 none, 1 unit type, 2 building type
    unsigned mCounterSubtype = 0;       // +0x140
    unsigned mCounterCount = 0;         // +0x144
    unsigned mCounterRequired = 0;      // +0x148
    bool mMarkerEnabled = false;        // +0x14c
    unsigned mMarkerType = 0;           // +0x150
    unsigned mMarkerObject = 0;         // +0x154
    CVector2 mBrainsTextPos;            // +0x158 (marker type 1)
    CVector2 mSectorsTextPos;           // +0x160 (marker type 2)
    CVector2 mMinimapCenter;            // +0x168 (marker type 7)

    // ---- [PC] mouse state (not in the original)
    struct PCState {
        CVector2 mouse;                 // last cursor position
        bool haveMouse = false;
        bool leftDown = false, rightDown = false, middleDown = false;
        bool leftRotate = false;        // Alt + left drag
        bool boxDrag = false;           // left drag beyond the threshold
        bool leftOnMinimap = false;
        bool leftConsumed = false;      // the down was taken by the HUD / dial / overlay
        CVector2 leftStart;
        int leftClicks = 0;
        CVector2 rightStart;
        float rightDownTime = 0.0f;     // ms (event timestamp)
        float rightHeld = 0.0f;         // seconds held, from timeUpdatePC
        bool rightLongFired = false;
        CVector2 middleAnchor;          // ground point under the cursor at the middle down
        bool keyLeft = false, keyRight = false, keyUp = false, keyDown = false;
        bool keyRotL = false, keyRotR = false;
        bool windowFocused = true;
    } mPC;

};

}  // namespace py
