// CInputScript.h - [PC] timed input scripts for headless end-to-end tests
// (ProjectY --input <file>). Format: docs/re/ui_phase5.md "Input scripts".
//
// One event per line: `[frame] command args...`. frame is an absolute frame
// number, `+N` (N frames after the previous line) or omitted (same frame as
// the previous line; 0 for the first). Events of a frame are queued, in file
// order, on the current interface before that frame's CEngine::draw.
// '#' starts a comment.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace py {

class CInterface;

struct ScriptEvent {
    enum Kind {
        MOUSE_DOWN, MOUSE_UP, MOUSE_MOVE, WHEEL, KEY_DOWN, KEY_UP, TEXT,
        TOUCH_DOWN, TOUCH_MOVE, TOUCH_UP, BACK, MENU, MENU_ACTION, SCREENSHOT, QUIT,
    };
    int frame = 0;
    Kind kind = QUIT;
    int button = 0;           // mouse button (EMouseButton)
    int x = 0, y = 0;         // pixels, origin top-left
    float wheelX = 0.0f, wheelY = 0.0f;
    int32_t keycode = 0;
    unsigned modifiers = 0;   // EKeyModifier
    unsigned pointer = 0;     // touch pointer / menu action
    std::string text;         // text input (Latin-1) or screenshot path
    int line = 0;
};

class CInputScript {
public:
    // keyFromName maps a key name ("escape", "a", "f1", ...) to a keycode, or
    // 0 if unknown (main_sdl passes SDL_GetKeyFromName).
    using KeyResolver = std::function<int32_t(const std::string&)>;

    bool load(const std::string& file, const KeyResolver& keyFromName);
    bool parse(const std::string& text, const KeyResolver& keyFromName);
    const std::string& error() const { return mError; }
    const std::vector<ScriptEvent>& events() const { return mEvents; }

    // The events of `frame` in file order.
    std::vector<const ScriptEvent*> eventsAt(int frame) const;
    int lastFrame() const;

    // Queues one input event on `target` (SCREENSHOT and QUIT are the
    // shell's: ignored here). `heldButtons` tracks the mouse buttons the
    // script holds, for the move events' button mask.
    static void dispatch(const ScriptEvent& e, CInterface* target, unsigned& heldButtons);

private:
    std::vector<ScriptEvent> mEvents;
    std::string mError;
};

}  // namespace py
