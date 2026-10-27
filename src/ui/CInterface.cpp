#include "CInterface.h"

#include "core/CUtils.h"
#include "game/CEngine.h"

namespace py {

namespace {
std::unique_ptr<CInterfaceEvent> makeEvent(int type, unsigned pointer) {
    auto e = std::make_unique<CInterfaceEvent>();
    e->pointer = pointer;
    e->type = type;
    e->timestamp = CUtils::getTimer();
    return e;
}
}  // namespace

// Ghidra 0x189234
CInterface::CInterface() { mEngine = CEngine::getSingletonPtr(); }

// Ghidra 0x1891b4
CInterface::~CInterface() = default;

// Ghidra 0x188cdc
void CInterface::addEvent(std::unique_ptr<CInterfaceEvent> e) {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    mEvents.push_back(std::move(e));
}

size_t CInterface::pendingEvents() const {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    return mEvents.size();
}

// Ghidra 0x188b7c: pops events in order and dispatches each by type.
void CInterface::processEvents() {
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    while (!mEvents.empty()) {
        std::unique_ptr<CInterfaceEvent> e = std::move(mEvents.front());
        mEvents.pop_front();
        dispatch(e.get());
    }
}

void CInterface::dispatch(CInterfaceEvent* e) {
    switch (e->type) {
        case EVENT_ACTION_UP: processEventActionUp(e); break;
        case EVENT_ACTION_DOWN: processEventActionDown(e); break;
        case EVENT_ACTION_MOVE: processEventActionMove(e); break;
        case EVENT_BACK_KEY: processEventBackKeyPressed(e); break;
        case EVENT_MENU_KEY: processEventMenuKeyPressed(e); break;
        case EVENT_FILE_DOWNLOAD_FINISHED: processEventFileDownloadFinished(e); break;
        case EVENT_MENU_ACTION: processEventMenuAction(e); break;
        // [PC] events: the PC handler first, then the emulated original event.
        case EVENT_MOUSE_BUTTON_DOWN:
            processEventMouseButtonDown(e);
            if (e->button == MOUSE_LEFT && mouseEmulatesTouch()) {
                mLeftDown = true;
                processEventActionDown(e);
            }
            break;
        case EVENT_MOUSE_BUTTON_UP:
            processEventMouseButtonUp(e);
            if (e->button == MOUSE_LEFT && mouseEmulatesTouch() && mLeftDown) {
                mLeftDown = false;
                processEventActionUp(e);
            }
            break;
        case EVENT_MOUSE_MOVE:
            processEventMouseMove(e);
            if (mLeftDown && mouseEmulatesTouch()) processEventActionMove(e);
            break;
        case EVENT_MOUSE_WHEEL: processEventMouseWheel(e); break;
        case EVENT_KEY_DOWN:
            processEventKeyDown(e);
            if (!e->repeat && keysEmulateAndroidKeys()) {
                if (e->keycode == KEY_ESCAPE || e->keycode == KEY_AC_BACK) processEventBackKeyPressed(e);
                else if (e->keycode == KEY_MENU || e->keycode == KEY_APPLICATION) processEventMenuKeyPressed(e);
            }
            break;
        case EVENT_KEY_UP: processEventKeyUp(e); break;
        case EVENT_TEXT_INPUT: processEventTextInput(e); break;
        default: break;
    }
}

// Ghidra 0x188e98
void CInterface::addEventActionUp(unsigned pointer, int x, int y) {
    auto e = makeEvent(EVENT_ACTION_UP, pointer);
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    addEvent(std::move(e));
}

// Ghidra 0x188f24
void CInterface::addEventActionDown(unsigned pointer, int x, int y) {
    auto e = makeEvent(EVENT_ACTION_DOWN, pointer);
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    addEvent(std::move(e));
}

// Ghidra 0x188fb0
void CInterface::addEventActionMove(unsigned pointer, int x, int y) {
    auto e = makeEvent(EVENT_ACTION_MOVE, pointer);
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    addEvent(std::move(e));
}

// Ghidra 0x18903c
void CInterface::addEventBackKeyPressed() { addEvent(makeEvent(EVENT_BACK_KEY, 0)); }

// Ghidra 0x189098
void CInterface::addEventMenuAction(int action) { addEvent(makeEvent(EVENT_MENU_ACTION, static_cast<unsigned>(action))); }

// Ghidra 0x1890f4
void CInterface::addEventMenuKeyPressed() { addEvent(makeEvent(EVENT_MENU_KEY, 0)); }

// Ghidra 0x189150
void CInterface::addEventDownloadFinished(int id) {
    addEvent(makeEvent(EVENT_FILE_DOWNLOAD_FINISHED, static_cast<unsigned>(id)));
}

// ---- [PC]

void CInterface::addEventMouseButtonDown(int button, int x, int y, unsigned modifiers, int clicks) {
    auto e = makeEvent(EVENT_MOUSE_BUTTON_DOWN, 0);
    e->button = button;
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    e->modifiers = modifiers;
    e->clicks = clicks;
    addEvent(std::move(e));
}

void CInterface::addEventMouseButtonUp(int button, int x, int y, unsigned modifiers) {
    auto e = makeEvent(EVENT_MOUSE_BUTTON_UP, 0);
    e->button = button;
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    e->modifiers = modifiers;
    addEvent(std::move(e));
}

void CInterface::addEventMouseMove(int x, int y, unsigned buttonMask, unsigned modifiers) {
    auto e = makeEvent(EVENT_MOUSE_MOVE, 0);
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    e->buttons = buttonMask;
    e->modifiers = modifiers;
    addEvent(std::move(e));
}

void CInterface::addEventMouseWheel(float dx, float dy, int x, int y, unsigned modifiers) {
    auto e = makeEvent(EVENT_MOUSE_WHEEL, 0);
    e->wheelX = dx;
    e->wheelY = dy;
    e->x = static_cast<float>(x);
    e->y = static_cast<float>(y);
    e->modifiers = modifiers;
    addEvent(std::move(e));
}

void CInterface::addEventKeyDown(int32_t keycode, unsigned modifiers, bool repeat) {
    auto e = makeEvent(EVENT_KEY_DOWN, 0);
    e->keycode = keycode;
    e->modifiers = modifiers;
    e->repeat = repeat;
    addEvent(std::move(e));
}

void CInterface::addEventKeyUp(int32_t keycode, unsigned modifiers) {
    auto e = makeEvent(EVENT_KEY_UP, 0);
    e->keycode = keycode;
    e->modifiers = modifiers;
    addEvent(std::move(e));
}

void CInterface::addEventTextInput(const std::string& latin1) {
    auto e = makeEvent(EVENT_TEXT_INPUT, 0);
    e->text = latin1;
    addEvent(std::move(e));
}

}  // namespace py
