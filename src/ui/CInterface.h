// CInterface.h - port of ProjectY::CInterface (Ghidra 0x188b50..0x189234),
// the base class of the three screens (CInterfaceIntro, CInterfaceMenu,
// CInterfaceIngame), with its input event queue.
//
// The platform layer (JNI in the original, main_sdl.cpp here) pushes events
// with addEvent* from any thread; CEngine::draw drains the queue once per
// frame with processEvents(), which dispatches each event to a virtual
// handler of this interface. Each interface has its own queue; events are
// queued on the interface that is current when they arrive (engine+0x168).
//
// Original event types (CInterfaceEvent +4) and vtable slots:
//   0 ActionUp   -> +0x10 processEventActionUp
//   1 ActionDown -> +0x14 processEventActionDown
//   2 ActionMove -> +0x18 processEventActionMove
//   3 Back key   -> +0x08 processEventBackKeyPressed
//   4 Menu key   -> +0x0c processEventMenuKeyPressed
//   5 download   -> +0x20 processEventFileDownloadFinished
//   6 MenuAction -> +0x1c processEventMenuAction (pointer field = EMENUACTIONS)
// Other slots: +0x00 draw, +0x04 timeUpdate, +0x24 setResolution,
// +0x28 getCamera, +0x2c processTextInput, +0x30.. network callbacks.
//
// [PC] Mouse, wheel, keyboard and text input are extra event types (7..13)
// with their own handlers, no-ops by default. Emulation of the original
// input happens at dispatch time, after the PC handler ran:
//   - left mouse button down/up and motion while it is held also call
//     processEventActionDown/Up/Move with pointer 0 when
//     mouseEmulatesTouch() is true (the default);
//   - Escape (and AC Back) key-down also calls processEventBackKeyPressed,
//     the Menu/Application key processEventMenuKeyPressed, when
//     keysEmulateAndroidKeys() is true (the default; repeats are ignored).
// An interface that maps the mouse itself (the ingame HUD, section C.6 of
// docs/re/scripts_rendering_ui.md) overrides these to return false.
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace py {

class CEngine;
class CCamera;

// Event types. 0..6 are the original's; 7.. are [PC] additions.
enum EInterfaceEventType : int {
    EVENT_ACTION_UP = 0,
    EVENT_ACTION_DOWN = 1,
    EVENT_ACTION_MOVE = 2,
    EVENT_BACK_KEY = 3,
    EVENT_MENU_KEY = 4,
    EVENT_FILE_DOWNLOAD_FINISHED = 5,
    EVENT_MENU_ACTION = 6,
    EVENT_MOUSE_BUTTON_DOWN = 7,   // [PC] button, x, y, modifiers, clicks
    EVENT_MOUSE_BUTTON_UP = 8,     // [PC] button, x, y, modifiers
    EVENT_MOUSE_MOVE = 9,          // [PC] x, y, buttons held (mask), modifiers
    EVENT_MOUSE_WHEEL = 10,        // [PC] wheelX, wheelY (notches, +y = away from the user), x, y
    EVENT_KEY_DOWN = 11,           // [PC] keycode (SDL_Keycode), modifiers, repeat
    EVENT_KEY_UP = 12,             // [PC] keycode, modifiers
    EVENT_TEXT_INPUT = 13,         // [PC] text (Latin-1, the font's encoding)
};

// [PC] mouse buttons (same numbers as SDL_BUTTON_*)
enum EMouseButton : int { MOUSE_LEFT = 1, MOUSE_MIDDLE = 2, MOUSE_RIGHT = 3, MOUSE_X1 = 4, MOUSE_X2 = 5 };

// [PC] modifier bits (subset of SDL's KMOD_* collapsed to left|right)
enum EKeyModifier : unsigned {
    MOD_SHIFT = 1u << 0,
    MOD_CTRL = 1u << 1,
    MOD_ALT = 1u << 2,
    MOD_GUI = 1u << 3,
};

// [PC] the few SDL keycodes the shared code needs (values = SDLK_*)
enum EKeyCode : int32_t {
    KEY_ESCAPE = 27,
    KEY_RETURN = 13,
    KEY_BACKSPACE = 8,
    KEY_TAB = 9,
    KEY_SPACE = 32,
    KEY_AC_BACK = 0x4000010E,   // SDLK_AC_BACK
    KEY_MENU = 0x40000076,      // SDLK_MENU
    KEY_APPLICATION = 0x40000065,   // SDLK_APPLICATION
};

// ProjectY::CInterfaceEvent (0x14 bytes in the original) + [PC] fields.
struct CInterfaceEvent {
    unsigned pointer = 0;     // +0x00 touch pointer id / EMENUACTIONS / download id
    int type = 0;             // +0x04 EInterfaceEventType
    float timestamp = 0.0f;   // +0x08 CUtils::getTimer() at queue time (ms)
    float x = 0.0f;           // +0x0c pixels, origin top-left
    float y = 0.0f;           // +0x10
    // [PC]
    int button = 0;           // EMouseButton (mouse button events)
    unsigned buttons = 0;     // mouse move: mask of held buttons, bit (button-1)
    int clicks = 0;           // mouse down: 1 single, 2 double click, ...
    float wheelX = 0.0f;      // mouse wheel
    float wheelY = 0.0f;
    int32_t keycode = 0;      // SDL_Keycode
    unsigned modifiers = 0;   // EKeyModifier bits
    bool repeat = false;      // key down auto-repeat
    std::string text;         // text input (Latin-1)
};

class CInterface {
public:
    CInterface();            // Ghidra 0x189234
    virtual ~CInterface();   // Ghidra 0x1891b4

    // ---- vtable (original slot offsets)
    virtual void draw() = 0;                                              // +0x00
    virtual void timeUpdate(float dt) = 0;                                // +0x04
    virtual void processEventBackKeyPressed(CInterfaceEvent* e) = 0;      // +0x08
    virtual void processEventMenuKeyPressed(CInterfaceEvent* e) = 0;      // +0x0c
    virtual void processEventActionUp(CInterfaceEvent* e) = 0;            // +0x10
    virtual void processEventActionDown(CInterfaceEvent* e) = 0;          // +0x14
    virtual void processEventActionMove(CInterfaceEvent* e) = 0;          // +0x18
    virtual void processEventMenuAction(CInterfaceEvent*) {}              // +0x1c  Ghidra 0x188b64
    virtual void processEventFileDownloadFinished(CInterfaceEvent*) {}    // +0x20  Ghidra 0x188b60
    virtual void setResolution(int width, int height) = 0;                // +0x24
    virtual CCamera* getCamera() = 0;                                     // +0x28
    virtual void processTextInput(std::string, bool) {}                   // +0x2c  Ghidra 0x188b50
    virtual void processGameClientDisconnected() {}                       // +0x34  Ghidra 0x188b5c
    virtual bool processLobbyClientConnected(bool connected) { return connected; }   // +0x4c
    // (+0x30 processChatReceivedGame, +0x38 processChatReceivedLobby,
    //  +0x3c processChatAddUser, +0x40 processReceiveStats, +0x44 Top10,
    //  +0x48 Achievements: network/lobby callbacks, added with networking.)

    // ---- [PC] input handlers, no-ops by default
    virtual void processEventMouseButtonDown(CInterfaceEvent*) {}
    virtual void processEventMouseButtonUp(CInterfaceEvent*) {}
    virtual void processEventMouseMove(CInterfaceEvent*) {}
    virtual void processEventMouseWheel(CInterfaceEvent*) {}
    virtual void processEventKeyDown(CInterfaceEvent*) {}
    virtual void processEventKeyUp(CInterfaceEvent*) {}
    virtual void processEventTextInput(CInterfaceEvent*) {}
    // [PC] see the header comment
    virtual bool mouseEmulatesTouch() const { return true; }
    virtual bool keysEmulateAndroidKeys() const { return true; }

    // ---- event queue (thread-safe)
    void processEvents();                                     // Ghidra 0x188b7c
    void addEvent(std::unique_ptr<CInterfaceEvent> e);        // Ghidra 0x188cdc
    void addEventActionUp(unsigned pointer, int x, int y);    // Ghidra 0x188e98
    void addEventActionDown(unsigned pointer, int x, int y);  // Ghidra 0x188f24
    void addEventActionMove(unsigned pointer, int x, int y);  // Ghidra 0x188fb0
    void addEventBackKeyPressed();                            // Ghidra 0x18903c
    void addEventMenuAction(int action);                      // Ghidra 0x189098
    void addEventMenuKeyPressed();                            // Ghidra 0x1890f4
    void addEventDownloadFinished(int id);                    // Ghidra 0x189150
    // [PC]
    void addEventMouseButtonDown(int button, int x, int y, unsigned modifiers, int clicks = 1);
    void addEventMouseButtonUp(int button, int x, int y, unsigned modifiers);
    void addEventMouseMove(int x, int y, unsigned buttonMask, unsigned modifiers);
    void addEventMouseWheel(float dx, float dy, int x, int y, unsigned modifiers);
    void addEventKeyDown(int32_t keycode, unsigned modifiers, bool repeat);
    void addEventKeyUp(int32_t keycode, unsigned modifiers);
    void addEventTextInput(const std::string& latin1);

    size_t pendingEvents() const;

    // +0x34: 0 menu, 1 ingame, 2 intro (CEngine::draw applies the game speed
    // only when the current interface is the ingame one)
    int getType() const { return mType; }

    static constexpr int TYPE_MENU = 0;
    static constexpr int TYPE_INGAME = 1;
    static constexpr int TYPE_INTRO = 2;

protected:
    void dispatch(CInterfaceEvent* e);

    CEngine* mEngine = nullptr;   // +0x04
    int mType = 0;                // +0x34
    bool mLeftDown = false;       // [PC] touch emulation: left button held

private:
    std::deque<std::unique_ptr<CInterfaceEvent>> mEvents;   // +0x08 deque
    // +0x30 boost::mutex, held while processEvents dispatches (as the
    // original). Recursive so a handler may queue on its own interface; the
    // new event is then dispatched in the same processEvents call.
    mutable std::recursive_mutex mMutex;
};

}  // namespace py
