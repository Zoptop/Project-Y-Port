#include "CInputScript.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "ui/CInterface.h"

namespace py {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool toInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool toFloat(const std::string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtof(s.c_str(), &end);
    return *end == '\0';
}

int buttonFromName(const std::string& n) {
    const std::string s = lower(n);
    if (s == "left" || s == "1") return MOUSE_LEFT;
    if (s == "middle" || s == "2") return MOUSE_MIDDLE;
    if (s == "right" || s == "3") return MOUSE_RIGHT;
    if (s == "x1" || s == "4") return MOUSE_X1;
    if (s == "x2" || s == "5") return MOUSE_X2;
    return 0;
}

// "shift", "ctrl", "alt", "gui" joined with '+'
bool modifiersFromName(const std::string& n, unsigned& mods) {
    std::stringstream ss(lower(n));
    std::string part;
    while (std::getline(ss, part, '+')) {
        if (part == "shift") mods |= MOD_SHIFT;
        else if (part == "ctrl" || part == "control") mods |= MOD_CTRL;
        else if (part == "alt") mods |= MOD_ALT;
        else if (part == "gui" || part == "cmd" || part == "win") mods |= MOD_GUI;
        else return false;
    }
    return true;
}

// "ctrl+shift+a" -> modifiers + key name
bool splitKey(const std::string& n, unsigned& mods, std::string& key) {
    const size_t p = n.rfind('+');
    if (p == std::string::npos || p + 1 >= n.size()) {
        key = n;
        return true;
    }
    key = n.substr(p + 1);
    return modifiersFromName(n.substr(0, p), mods);
}

}  // namespace

bool CInputScript::load(const std::string& file, const KeyResolver& keyFromName) {
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        mError = "can't read " + file;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), keyFromName);
}

bool CInputScript::parse(const std::string& text, const KeyResolver& keyFromName) {
    mEvents.clear();
    mError.clear();
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    int frame = 0;
    auto fail = [&](const std::string& msg) {
        mError = "line " + std::to_string(lineNo) + ": " + msg;
        return false;
    };
    while (std::getline(in, line)) {
        ++lineNo;
        const size_t hash = line.find('#');
        std::string body = hash == std::string::npos ? line : line.substr(0, hash);
        std::istringstream ls(body);
        std::vector<std::string> tok;
        for (std::string t; ls >> t;) tok.push_back(t);
        if (tok.empty()) continue;
        size_t i = 0;
        int f = 0;
        if (tok[0][0] == '+' && toInt(tok[0].substr(1), f)) {
            frame += f;
            ++i;
        } else if (toInt(tok[0], f)) {
            frame = f;
            ++i;
        }
        if (i >= tok.size()) return fail("missing command");
        ScriptEvent e;
        e.frame = frame;
        e.line = lineNo;
        const std::string cmd = lower(tok[i++]);
        const size_t nargs = tok.size() - i;
        auto arg = [&](size_t k) -> const std::string& { return tok[i + k]; };
        auto readXY = [&](size_t k) { return toInt(arg(k), e.x) && toInt(arg(k + 1), e.y); };
        auto optMods = [&](size_t k) { return nargs <= k || modifiersFromName(arg(k), e.modifiers); };

        if (cmd == "mousedown" || cmd == "mouseup" || cmd == "click") {
            if (nargs < 3 || !(e.button = buttonFromName(arg(0))) || !readXY(1) || !optMods(3))
                return fail(cmd + " <left|middle|right|x1|x2> <x> <y> [modifiers]");
            if (cmd == "click") {
                e.kind = ScriptEvent::MOUSE_DOWN;
                mEvents.push_back(e);
                e.kind = ScriptEvent::MOUSE_UP;
            } else {
                e.kind = cmd == "mousedown" ? ScriptEvent::MOUSE_DOWN : ScriptEvent::MOUSE_UP;
            }
        } else if (cmd == "mousemove") {
            if (nargs < 2 || !readXY(0) || !optMods(2)) return fail("mousemove <x> <y> [modifiers]");
            e.kind = ScriptEvent::MOUSE_MOVE;
        } else if (cmd == "wheel") {
            if (nargs < 1 || !toFloat(arg(0), e.wheelY)) return fail("wheel <dy> [<x> <y>] [modifiers]");
            if (nargs >= 3 && !readXY(1)) return fail("wheel <dy> [<x> <y>] [modifiers]");
            if (!optMods(nargs >= 3 ? 3 : 1)) return fail("wheel: bad modifiers");
            e.kind = ScriptEvent::WHEEL;
        } else if (cmd == "key" || cmd == "keydown" || cmd == "keyup") {
            std::string name;
            if (nargs != 1 || !splitKey(arg(0), e.modifiers, name)) return fail(cmd + " [modifiers+]<keyname>");
            e.keycode = keyFromName ? keyFromName(name) : 0;
            if (e.keycode == 0) return fail("unknown key '" + name + "'");
            if (cmd == "key") {
                e.kind = ScriptEvent::KEY_DOWN;
                mEvents.push_back(e);
                e.kind = ScriptEvent::KEY_UP;
            } else {
                e.kind = cmd == "keydown" ? ScriptEvent::KEY_DOWN : ScriptEvent::KEY_UP;
            }
        } else if (cmd == "text") {
            // the rest of the line after "text " (spaces kept)
            const size_t p = lower(body).find("text");
            std::string rest = body.substr(p + 4);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ')) rest.pop_back();
            e.text = rest;
            e.kind = ScriptEvent::TEXT;
        } else if (cmd == "mapscript") {
            // the rest of the line: a map script line without its "script" word
            const size_t p = lower(body).find("mapscript");
            std::string rest = body.substr(p + 9);
            while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ')) rest.pop_back();
            if (rest.empty()) return fail("mapscript <type> <params>");
            e.text = rest;
            e.kind = ScriptEvent::MAPSCRIPT;
        } else if (cmd == "touchdown" || cmd == "touchmove" || cmd == "touchup") {
            int id = 0;
            if (nargs != 3 || !toInt(arg(0), id) || !readXY(1)) return fail(cmd + " <pointer> <x> <y>");
            e.pointer = static_cast<unsigned>(id);
            e.kind = cmd == "touchdown" ? ScriptEvent::TOUCH_DOWN
                                        : (cmd == "touchmove" ? ScriptEvent::TOUCH_MOVE : ScriptEvent::TOUCH_UP);
        } else if (cmd == "back") {
            e.kind = ScriptEvent::BACK;
        } else if (cmd == "menu") {
            e.kind = ScriptEvent::MENU;
        } else if (cmd == "menuaction") {
            int a = 0;
            if (nargs != 1 || !toInt(arg(0), a)) return fail("menuaction <n>");
            e.pointer = static_cast<unsigned>(a);
            e.kind = ScriptEvent::MENU_ACTION;
        } else if (cmd == "screenshot") {
            if (nargs != 1) return fail("screenshot <file.png>");
            e.text = arg(0);
            e.kind = ScriptEvent::SCREENSHOT;
        } else if (cmd == "quit") {
            e.kind = ScriptEvent::QUIT;
        } else if (cmd == "dump") {
            e.text = nargs >= 1 ? arg(0) : std::string();
            e.kind = ScriptEvent::DUMP;
        } else {
            return fail("unknown command '" + cmd + "'");
        }
        mEvents.push_back(e);
    }
    return true;
}

std::vector<const ScriptEvent*> CInputScript::eventsAt(int frame) const {
    std::vector<const ScriptEvent*> r;
    for (const ScriptEvent& e : mEvents)
        if (e.frame == frame) r.push_back(&e);
    return r;
}

int CInputScript::lastFrame() const {
    int f = -1;
    for (const ScriptEvent& e : mEvents) f = std::max(f, e.frame);
    return f;
}

void CInputScript::dispatch(const ScriptEvent& e, CInterface* t, unsigned& held) {
    if (!t) return;
    switch (e.kind) {
        case ScriptEvent::MOUSE_DOWN:
            held |= 1u << (e.button - 1);
            t->addEventMouseButtonDown(e.button, e.x, e.y, e.modifiers, 1);
            break;
        case ScriptEvent::MOUSE_UP:
            held &= ~(1u << (e.button - 1));
            t->addEventMouseButtonUp(e.button, e.x, e.y, e.modifiers);
            break;
        case ScriptEvent::MOUSE_MOVE: t->addEventMouseMove(e.x, e.y, held, e.modifiers); break;
        case ScriptEvent::WHEEL: t->addEventMouseWheel(e.wheelX, e.wheelY, e.x, e.y, e.modifiers); break;
        case ScriptEvent::KEY_DOWN: t->addEventKeyDown(e.keycode, e.modifiers, false); break;
        case ScriptEvent::KEY_UP: t->addEventKeyUp(e.keycode, e.modifiers); break;
        case ScriptEvent::TEXT: t->addEventTextInput(e.text); break;
        case ScriptEvent::TOUCH_DOWN: t->addEventActionDown(e.pointer, e.x, e.y); break;
        case ScriptEvent::TOUCH_MOVE: t->addEventActionMove(e.pointer, e.x, e.y); break;
        case ScriptEvent::TOUCH_UP: t->addEventActionUp(e.pointer, e.x, e.y); break;
        case ScriptEvent::BACK: t->addEventBackKeyPressed(); break;
        case ScriptEvent::MENU: t->addEventMenuKeyPressed(); break;
        case ScriptEvent::MENU_ACTION: t->addEventMenuAction(static_cast<int>(e.pointer)); break;
        case ScriptEvent::SCREENSHOT:
        case ScriptEvent::QUIT:
        case ScriptEvent::MAPSCRIPT:
        case ScriptEvent::DUMP: break;
    }
}

}  // namespace py
