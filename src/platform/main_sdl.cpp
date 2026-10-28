// main_sdl.cpp - PC replacement for the Android Java shell.
//
// The original Activity (com.luminencelabs.ProjectY.gf.ProjectY) created a
// GLSurfaceView and forwarded to JNI: AppOnCreate -> CEngine::init,
// GL2View.Draw -> CEngine::draw every frame, MotionEvent -> touch input,
// AppOnPause/Resume, KeyBack/KeyMenu. This file plays that role with SDL2:
//   - window + GL context, CEngine::initInterfaces (intro -> menu);
//   - the window size becomes CEngine::setResolution, and a phone-like DPI
//     derived from it becomes the GUI scale (CEngine::setDPI) [PC];
//   - SDL events become CInterface events on the current interface (mouse
//     buttons/motion/wheel, keys, text; see ui/CInterface.h for how the left
//     button and Escape also produce the original touch/Back events);
//   - every frame calls CEngine::draw.
//
// Options:
//   --data <dir>         game data (default: data)
//   --width/--height N   window size (default 1280x720), --fullscreen
//   --frames N           quit after N frames
//   --fps N              advance the engine clock by 1000/N ms per frame
//                        instead of real time (default 60 when --frames is
//                        given, so headless runs are reproducible)
//   --screenshot f.png   write the last frame as PNG
//   --input <file>       timed input script (docs/re/ui_phase5.md)
//   --gui-scale F        override the GUI scale (CEngine::mGUIScale)
//   --save-dir <dir>     where options.txt and the savegames live
//                        (default: SDL_GetPrefPath, the user's app data)
//   --check-shaders      compile every .gfx and exit
//   --skirmish <map>     [PC] start a skirmish on map <map> directly in the
//                        ingame screen (--players N, default the map's; --fow; the
//                        minimap with --minimap 0..3, default 2; --gametype N, the
//                        custom game's game type, 3 = resources)
//   --mission <map>      [PC] start a campaign mission (301..318) or the
//                        tutorial (201) the way the menus do (the campaign's
//                        player setup, the loading screen, initSingleplayerGame),
//                        whether or not it is unlocked; winning it unlocks the
//                        next one in the save dir's cmp1prog.txt as usual
//   --viewer             the phase-2 map viewer instead of the game
//                        (--map N, --zoom Z, --cam X Y, --yaw DEG;
//                        PageUp/PageDown cycle maps, Esc quits)
#include <SDL.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CMemoryFile.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CLevel.h"
#include "game/CScriptManager.h"
#include "game/CWorldView.h"
#include "platform/GLFunctions.h"
#include "render/CCamera.h"
#include "render/CGLSLProgram.h"
#include "render/CMesh.h"
#include "render/CTexture.h"
#include "render/CTextRender.h"
#include "ui/CInputScript.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"
#include "ui/CInterfaceMenu.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

using namespace py;

namespace {

struct Options {
    std::string dataDir = "data";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    int frames = -1;            // run N frames then quit (automated tests)
    float fps = 0.0f;           // fixed engine clock (0 = real time)
    int map = 1;
    std::string screenshot;     // write a PNG of the last frame
    std::string inputScript;    // --input
    std::string saveDir;        // --save-dir (empty: SDL_GetPrefPath)
    float guiScale = 0.0f;      // --gui-scale (0 = from the window size)
    float zoom = -1.0f;         // initial zoom (default 2.0 = closest)
    float camX = 1e9f, camY = 1e9f;
    float yawDeg = 0.0f;
    bool checkShaders = false;  // compile every .gfx and exit (non-zero on failure)
    // world view (game/CWorldView.h)
    bool fow = false;           // --fow: fog of war (option UseFOW), seen by player 1
    bool allAI = false;         // --all-ai: player 1 is a computer player too
    bool noSim = false;         // --no-sim: draw the map without running the game
    int steps = 0;              // --steps N: simulate N steps (0.1 s) before the first frame
    bool viewer = false;        // --viewer: the phase-2 map viewer
    int skirmish = 0;           // --skirmish <map> [PC]
    int players = 0;            // --players N (with --skirmish; 0 = the map's "set numplayers")
    int minimap = 2;            // --minimap 0..3 (with --skirmish)
    int gameType = 0;           // --gametype N (with --skirmish; engine +0x2d0)
    int mission = 0;            // --mission <map> [PC]
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto nextS = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--data") o.dataDir = nextS();
        else if (a == "--width") o.width = std::atoi(nextS().c_str());
        else if (a == "--height") o.height = std::atoi(nextS().c_str());
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--frames") o.frames = std::atoi(nextS().c_str());
        else if (a == "--fps") o.fps = static_cast<float>(std::atof(nextS().c_str()));
        else if (a == "--map") o.map = std::atoi(nextS().c_str());
        else if (a == "--screenshot") o.screenshot = nextS();
        else if (a == "--input") o.inputScript = nextS();
        else if (a == "--save-dir") o.saveDir = nextS();
        else if (a == "--gui-scale") o.guiScale = static_cast<float>(std::atof(nextS().c_str()));
        else if (a == "--zoom") o.zoom = static_cast<float>(std::atof(nextS().c_str()));
        else if (a == "--cam") {
            o.camX = static_cast<float>(std::atof(nextS().c_str()));
            o.camY = static_cast<float>(std::atof(nextS().c_str()));
        } else if (a == "--yaw") o.yawDeg = static_cast<float>(std::atof(nextS().c_str()));
        else if (a == "--check-shaders") o.checkShaders = true;
        else if (a == "--fow") o.fow = true;
        else if (a == "--all-ai") o.allAI = true;
        else if (a == "--no-sim") o.noSim = true;
        else if (a == "--steps") o.steps = std::atoi(nextS().c_str());
        else if (a == "--viewer") o.viewer = true;
        else if (a == "--skirmish") o.skirmish = std::atoi(nextS().c_str());
        else if (a == "--players") o.players = std::atoi(nextS().c_str());
        else if (a == "--minimap") o.minimap = std::atoi(nextS().c_str());
        else if (a == "--gametype") o.gameType = std::atoi(nextS().c_str());
        else if (a == "--mission") o.mission = std::atoi(nextS().c_str());
    }
    if (o.fps <= 0.0f && o.frames > 0) o.fps = 60.0f;
    return o;
}

// The game-speed table from CEngine::getSpeedFactor.
float speedFactor(int setting) {
    static const float table[] = {0.1f, 0.2f, 0.5f, 1.0f, 1.5f, 2.0f};
    if (setting < 0 || setting > 5) return 1.0f;
    return table[setting];
}

bool createContext(SDL_Window* win, SDL_GLContext* out, bool* isES) {
    // Prefer desktop GL 3.3 core; fall back to GLES 3.0 / 2.0 (e.g. via ANGLE).
    struct Attempt {
        int profile, major, minor;
        bool es;
        const char* name;
    } attempts[] = {
        {SDL_GL_CONTEXT_PROFILE_CORE, 3, 3, false, "OpenGL 3.3 core"},
        {SDL_GL_CONTEXT_PROFILE_ES, 3, 0, true, "OpenGL ES 3.0"},
        {SDL_GL_CONTEXT_PROFILE_ES, 2, 0, true, "OpenGL ES 2.0"},
    };
    for (const auto& a : attempts) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, a.profile);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, a.major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, a.minor);
        if (SDL_GLContext ctx = SDL_GL_CreateContext(win)) {
            CUtils::log("GL context: %s", a.name);
            *out = ctx;
            *isES = a.es;
            return true;
        }
    }
    return false;
}

// CEngine::initOpenGL state
void initOpenGLState() {
    gl::Hint(0x8192, 0x1102);    // GL_GENERATE_MIPMAP_HINT, GL_NICEST (ignored by core profiles)
    gl::GetError();
    gl::Enable(0x0B71);          // GL_DEPTH_TEST
    gl::Enable(0x0B44);          // GL_CULL_FACE
    gl::Disable(0x0BD0);         // GL_DITHER
    gl::DepthMask(1);
    gl::DepthFunc(0x0201);       // GL_LESS
    gl::CullFace(0x0405);        // GL_BACK
    gl::FrontFace(0x0901);       // GL_CCW
}

void saveScreenshot(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    gl::ReadPixels(0, 0, w, h, 0x1908, 0x1401, px.data());   // GL_RGBA, GL_UNSIGNED_BYTE
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;   // the frame is opaque; ignore framebuffer alpha
    stbi_flip_vertically_on_write(1);
    if (stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4)) CUtils::log("screenshot written: %s", path.c_str());
}

// Compiles and links every .gfx in data/ and data/add/; returns the number of failures.
int checkAllShaders() {
    std::set<std::string> names;
    for (const std::string& dir : {CUtils::dataPath(""), CUtils::dataPath("add")}) {
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec))
            if (e.path().extension() == ".gfx") names.insert(e.path().filename().string());
    }
    int failed = 0;
    for (const auto& n : names) {
        const bool ok = CGLSLProgramManager::get().getGLSLProgram(n) != nullptr;
        if (!ok) ++failed;
        CUtils::log("shader %-40s %s", n.c_str(), ok ? "ok" : "FAILED");
    }
    CUtils::log("shader check: %d of %d failed", failed, static_cast<int>(names.size()));
    return failed;
}

// "set numplayers N" of a map script (2 if missing).
unsigned mapPlayerCount(int id) {
    const unsigned n = CInterfaceIngame::mapNumPlayers(static_cast<unsigned>(id));
    return n >= 1 && n <= 4 ? n : 2u;
}

std::vector<int> availableMaps() {
    std::vector<int> ids;
    for (int id : {201, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312,
                   313, 314, 315, 316, 317, 318})
        if (!CMemoryFile::resolve("map" + std::to_string(id) + "English.txt").empty()) ids.push_back(id);
    return ids;
}

// ---------------------------------------------------------------------------
// Input translation [PC]

unsigned modifiersFromSDL(Uint16 mod) {
    unsigned m = 0;
    if (mod & KMOD_SHIFT) m |= MOD_SHIFT;
    if (mod & KMOD_CTRL) m |= MOD_CTRL;
    if (mod & KMOD_ALT) m |= MOD_ALT;
    if (mod & KMOD_GUI) m |= MOD_GUI;
    return m;
}

unsigned buttonMaskFromSDL(Uint32 state) {
    unsigned m = 0;
    for (int b = 1; b <= 5; ++b)
        if (state & SDL_BUTTON(b)) m |= 1u << (b - 1);
    return m;
}

// UTF-8 -> Latin-1 (the font's encoding); characters above U+00FF are dropped.
std::string utf8ToLatin1(const char* s) {
    std::string out;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p;) {
        unsigned cp = 0;
        int extra = 0;
        if (*p < 0x80) cp = *p, extra = 0;
        else if ((*p & 0xe0) == 0xc0) cp = *p & 0x1f, extra = 1;
        else if ((*p & 0xf0) == 0xe0) cp = *p & 0x0f, extra = 2;
        else cp = *p & 0x07, extra = 3;
        ++p;
        for (int i = 0; i < extra && *p; ++i, ++p) cp = (cp << 6) | (*p & 0x3f);
        if (cp >= 0x20 && cp <= 0xff) out.push_back(static_cast<char>(cp));
    }
    return out;
}

int32_t keyFromName(const std::string& name) {
    SDL_Keycode k = SDL_GetKeyFromName(name.c_str());
    if (k == SDLK_UNKNOWN && name.size() == 1) k = static_cast<unsigned char>(std::tolower(name[0]));
    return static_cast<int32_t>(k);
}

// Window coordinates (points) -> drawable pixels (differs on HiDPI displays).
struct PixelMapper {
    SDL_Window* win;
    int px(int x) const {
        int ww = 1, wh = 1, dw = 1, dh = 1;
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GL_GetDrawableSize(win, &dw, &dh);
        return ww > 0 ? x * dw / ww : x;
    }
    int py(int y) const {
        int ww = 1, wh = 1, dw = 1, dh = 1;
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GL_GetDrawableSize(win, &dw, &dh);
        return wh > 0 ? y * dh / wh : y;
    }
};

// [PC] A phone-like DPI for the window: the original sized its HUD by the
// display DPI (CEngine::setDPI, (xdpi + ydpi) / 436.21). On PC we pretend the
// window's short side is a 2.7-inch phone screen side, so the HUD keeps the
// proportions it had on a phone (1080 px -> 400 dpi -> scale 1.83).
void applyWindowDPI(CEngine* engine, const Options& opt, int w, int h) {
    const float dpi = static_cast<float>(w < h ? w : h) / 2.7f;
    engine->setDPI(dpi, dpi, w, h);
    if (opt.guiScale > 0.0f) engine->mGUIScale = opt.guiScale;
}

// ---------------------------------------------------------------------------
// The game

// [PC] --mission: what the menus do when a campaign episode is replayed from
// the progress screen (26, action 0x52) or the tutorial is started (screen 9,
// action 0xe). The campaign is read first (the "Campaign" button's
// CCampaign::startInitAsync). The menu then shows the loading screen and
// starts the game (draw modes 1 and 2).
bool startMission(CEngine* engine, int mission) {
    CInterfaceMenu* m = engine->mMenuInterface.get();
    if (!m) return false;
    engine->mCurrentInterface = m;
    if (mission == 201) {
        engine->mGameType = 0;   // the single player screen's action 1
        m->mNextMenu = m->mCurrentMenu = MENU_TUTORIAL;
        m->processAction(0xe, nullptr);
        return true;
    }
    CCampaign* c = engine->getCurCampaign();
    if (!c) return false;
    c->startInitAsync(false);
    CCampaignMapInfo* info = c->getMapInfo(static_cast<unsigned>(mission));
    if (!info) return false;
    c->mCurChapter = info->chapter;
    c->mCurEpisode = info->episode;
    engine->mGameType = 1;   // the campaign screen (setupElements 25)
    m->mNextMenu = MENU_CAMPAIGNPROGRESS;
    m->setupElements(MENU_CAMPAIGNPROGRESS, 0);
    m->mCurrentMenu = MENU_CAMPAIGNPROGRESS;
    m->processAction(0x52, nullptr);
    return m->mDrawMode == 1;
}

int runGame(const Options& opt, SDL_Window* window) {
    CEngine* engine = CEngine::getSingletonPtr();
    CInputScript script;
    if (!opt.inputScript.empty() && !script.load(opt.inputScript, keyFromName)) {
        CUtils::log("input script: %s", script.error().c_str());
        std::fprintf(stderr, "input script %s: %s\n", opt.inputScript.c_str(), script.error().c_str());
        return 3;
    }

    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    applyWindowDPI(engine, opt, w, h);
    // CEngine::init: the options file (the original's app files dir; [PC] the
    // user's app data folder or --save-dir)
    if (!opt.saveDir.empty()) {
        engine->mSaveDir = opt.saveDir;
    } else if (char* pref = SDL_GetPrefPath("LuminenceLabs", "ProjectY")) {
        engine->mSaveDir = pref;
        SDL_free(pref);
        while (!engine->mSaveDir.empty() && (engine->mSaveDir.back() == '/' || engine->mSaveDir.back() == '\\'))
            engine->mSaveDir.pop_back();
    }
    {
        std::error_code ec;
        std::filesystem::create_directories(engine->mSaveDir, ec);
    }
    CUtils::log("save dir: %s", engine->mSaveDir.c_str());
    engine->loadOptions();
    applyWindowDPI(engine, opt, w, h);   // loadOptions re-derives the GUI scale; keep --gui-scale
    engine->mOpenURLHook = [](const std::string& url) { SDL_OpenURL(url.c_str()); };   // [PC] Java openURL
    engine->initInterfaces();
    engine->setResolution(w, h);
    if (opt.skirmish > 0) {
        // [PC] straight into a game (the menu's initSingleplayerGame without the menu)
        engine->mMinimapSize = opt.minimap;
        const unsigned players = opt.players >= 1 && opt.players <= 4 ? static_cast<unsigned>(opt.players)
                                                                    : mapPlayerCount(opt.skirmish);
        if (!CInterfaceIngame::startSkirmish(static_cast<unsigned>(opt.skirmish), players, opt.fow, w, h,
                                             opt.gameType)) {
            std::fprintf(stderr, "--skirmish %d: the map did not load\n", opt.skirmish);
            return 4;
        }
    } else if (opt.mission > 0) {
        engine->mMinimapSize = opt.minimap;
        if (!startMission(engine, opt.mission)) {
            std::fprintf(stderr, "--mission %d: not a campaign mission or the tutorial\n", opt.mission);
            return 4;
        }
    }

    const PixelMapper pm{window};
    SDL_Cursor* cursors[6] = {SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW),
                              SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND),
                              SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR),
                              SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL),
                              SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND),
                              SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND)};
    int cursorShown = -1;
    unsigned scriptButtons = 0;
    int frame = 0;
    bool running = true;
    const float t0 = CUtils::getTimer();
    engine->mLastFrameTime = opt.fps > 0.0f ? 0.0f : t0;   // CEngine::startTimer
    while (running) {
        CInterface* cur = engine->mCurrentInterface;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            cur = engine->mCurrentInterface;
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        SDL_GL_GetDrawableSize(window, &w, &h);
                        applyWindowDPI(engine, opt, w, h);
                        engine->setResolution(w, h);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (cur && e.button.which != SDL_TOUCH_MOUSEID)
                        cur->addEventMouseButtonDown(e.button.button, pm.px(e.button.x), pm.py(e.button.y),
                                                     modifiersFromSDL(SDL_GetModState()), e.button.clicks);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (cur && e.button.which != SDL_TOUCH_MOUSEID)
                        cur->addEventMouseButtonUp(e.button.button, pm.px(e.button.x), pm.py(e.button.y),
                                                   modifiersFromSDL(SDL_GetModState()));
                    break;
                case SDL_MOUSEMOTION:
                    if (cur && e.motion.which != SDL_TOUCH_MOUSEID)
                        cur->addEventMouseMove(pm.px(e.motion.x), pm.py(e.motion.y), buttonMaskFromSDL(e.motion.state),
                                               modifiersFromSDL(SDL_GetModState()));
                    break;
                case SDL_MOUSEWHEEL: {
                    int mx = 0, my = 0;
                    SDL_GetMouseState(&mx, &my);
                    float dx = static_cast<float>(e.wheel.x), dy = static_cast<float>(e.wheel.y);
                    if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dx = -dx, dy = -dy;
                    if (cur) cur->addEventMouseWheel(dx, dy, pm.px(mx), pm.py(my), modifiersFromSDL(SDL_GetModState()));
                    break;
                }
                case SDL_KEYDOWN:
                    if (cur) cur->addEventKeyDown(e.key.keysym.sym, modifiersFromSDL(e.key.keysym.mod), e.key.repeat != 0);
                    break;
                case SDL_KEYUP:
                    if (cur) cur->addEventKeyUp(e.key.keysym.sym, modifiersFromSDL(e.key.keysym.mod));
                    break;
                case SDL_TEXTINPUT: {
                    const std::string t = utf8ToLatin1(e.text.text);
                    if (cur && !t.empty()) cur->addEventTextInput(t);
                    break;
                }
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                case SDL_FINGERUP: {
                    // real touch screens: the original's touch events, one pointer per finger
                    const int x = static_cast<int>(e.tfinger.x * static_cast<float>(w));
                    const int y = static_cast<int>(e.tfinger.y * static_cast<float>(h));
                    const unsigned id = static_cast<unsigned>(e.tfinger.fingerId & 0xff);
                    if (!cur) break;
                    if (e.type == SDL_FINGERDOWN) cur->addEventActionDown(id, x, y);
                    else if (e.type == SDL_FINGERUP) cur->addEventActionUp(id, x, y);
                    else cur->addEventActionMove(id, x, y);
                    break;
                }
                default: break;
            }
        }

        // scripted input of this frame
        std::string shotThisFrame;
        for (const ScriptEvent* se : script.eventsAt(frame)) {
            if (se->kind == ScriptEvent::SCREENSHOT) shotThisFrame = se->text;
            else if (se->kind == ScriptEvent::QUIT) running = false;
            else if (se->kind == ScriptEvent::DUMP) {
                // [PC] the ingame state for the script checks
                auto* ig = dynamic_cast<CInterfaceIngame*>(engine->mIngameInterface.get());
                const std::string st = ig && ig->ipp(0) ? ig->ipp(0)->debugState() : std::string("no ingame interface");
                CUtils::log("state %s: %s", se->text.c_str(), st.c_str());
                std::printf("state %s: %s\n", se->text.c_str(), st.c_str());
                std::fflush(stdout);
            }
            else if (se->kind == ScriptEvent::MAPSCRIPT) {
                // [PC] a script line for the running game, e.g. "gamestate result(1) player(1)"
                CScriptManager* sm = engine->mGame && engine->mGame->getLevel()
                                         ? engine->mGame->getLevel()->getScriptManager()
                                         : nullptr;
                if (!sm || !sm->addScript("script " + se->text)) CUtils::log("mapscript: no game or bad line");
            }
            else CInputScript::dispatch(*se, engine->mCurrentInterface, scriptButtons);
        }

        const float now = opt.fps > 0.0f ? static_cast<float>(frame + 1) * (1000.0f / opt.fps)
                                         : CUtils::getTimer();
        engine->draw(now);
        if (engine->mCurrentInterface) {   // [PC] the cursor shape for the hovered action
            int hint = engine->mCurrentInterface->getCursorHint();
            if (hint < 0 || hint > 5) hint = 0;
            if (hint != cursorShown && cursors[hint]) {
                SDL_SetCursor(cursors[hint]);
                cursorShown = hint;
            }
        }

        ++frame;
        const bool last = (opt.frames > 0 && frame >= opt.frames) || !running || engine->mQuitRequested;
        if (!shotThisFrame.empty()) saveScreenshot(shotThisFrame, w, h);
        if (last && !opt.screenshot.empty()) saveScreenshot(opt.screenshot, w, h);
        SDL_GL_SwapWindow(window);
        if (last) running = false;
    }
    CUtils::log("frames: %d%s", frame, engine->mQuitRequested ? " (quit from the menu)" : "");
    engine->releaseInterfaces();
    for (SDL_Cursor* c : cursors)
        if (c) SDL_FreeCursor(c);
    return 0;
}

// ---------------------------------------------------------------------------
// The phase-2 map viewer (--viewer)

int runViewer(const Options& opt, SDL_Window* window, const std::string& version) {
    const std::vector<int> maps = availableMaps();
    int mapId = opt.map;
    std::unique_ptr<CGame> game;
    CLevel* level = nullptr;
    CCameraLevel cam;
    CWorldView view;
    auto loadLevel = [&](int id) {
        CEngine* engine = CEngine::getSingletonPtr();
        engine->mGame = nullptr;
        game.reset();
        level = nullptr;
        CMeshManager::get().clear();
        CTextureManager::get().clear();
        CGLSLProgramManager::get().clear();
        // the text renderer (labels), on the freshly cleared resources
        engine->mTextRender = std::make_unique<CTextRender>();
        if (!engine->mTextRender->init()) CUtils::log("CTextRender: font resources missing");
        // Viewer setup: the map's own player count (the menu decides this in
        // the game), slot 1 human, the rest AI; campaign maps are "scripted".
        engine->setupSkirmish(static_cast<unsigned>(id), mapPlayerCount(id));
        engine->mGameType = id >= 200 ? 1 : 0;
        engine->mUseFOW = opt.fow;
        if (opt.allAI) engine->mPlayerSetup[1].type = 2;
        game = std::make_unique<CGame>();
        engine->mGame = game.get();
        if (!game->init(false)) {
            CUtils::log("map %d failed to load", id);
            return false;
        }
        level = game->getLevel();
        view.reset(game.get(), game->getPlayer(1));
        for (int i = 0; i < opt.steps; ++i) view.advance(0.1f);
        cam = CCameraLevel();
        cam.setMapSize(level->mapWidth(), level->mapHeight());
        cam.setTarget(0, 0);
        std::string name = level->mapName();
        while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
        CUtils::log("map %d: %zu game objects, %zu scripts", id, game->objectList().size(),
                    level->getScriptManager()->scripts().size());
        SDL_SetWindowTitle(window, ("ProjectY " + version + " - " + name + " (map " + std::to_string(id) + ")").c_str());
        return true;
    };
    if (!loadLevel(mapId)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ProjectY",
                                 ("Could not load map " + std::to_string(mapId) + ". Are the map files in data/add?").c_str(),
                                 window);
        return 1;
    }
    if (opt.zoom > 0) cam.setZoom(opt.zoom);
    if (opt.camX < 1e8f) cam.setTarget(opt.camX, opt.camY);
    cam.rotateBy(opt.yawDeg * 3.14159265f / 180.0f);
    if (cam.isStrategyView()) {   // start already settled at the strategy tilt
        for (int i = 0; i < 20; ++i) cam.timeUpdate(0.1f);
    }

    // --- frame loop: the timing half of CEngine::draw ---
    int speedSetting = 3;   // "normal" (factor 1.0); CEngine+0x1b8
    float lastTime = CUtils::getTimer();
    bool running = true;
    int frame = 0;
    bool dragPan = false, dragRotate = false;
    Vec3 dragAnchor;
    int lastMouseX = 0;
    while (running) {
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(window, &w, &h);
        cam.setViewport(w, h);
        cam.refresh();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE) running = false;   // KeyBack
                    if (e.key.keysym.sym == SDLK_PAGEUP || e.key.keysym.sym == SDLK_PAGEDOWN) {
                        size_t idx = 0;
                        for (size_t i = 0; i < maps.size(); ++i)
                            if (maps[i] == mapId) idx = i;
                        idx = (e.key.keysym.sym == SDLK_PAGEDOWN) ? (idx + 1) % maps.size()
                                                                 : (idx + maps.size() - 1) % maps.size();
                        mapId = maps[idx];
                        loadLevel(mapId);
                    }
                    break;
                case SDL_MOUSEWHEEL: cam.zoomBy(e.wheel.y > 0 ? 1.15f : 1.0f / 1.15f); break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        dragPan = cam.screenToGround(static_cast<float>(e.button.x), static_cast<float>(e.button.y),
                                                     dragAnchor);
                    } else if (e.button.button == SDL_BUTTON_RIGHT) {
                        dragRotate = true;
                        lastMouseX = e.button.x;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_LEFT) dragPan = false;
                    if (e.button.button == SDL_BUTTON_RIGHT) dragRotate = false;
                    break;
                case SDL_MOUSEMOTION:
                    if (dragPan) {
                        Vec3 now;
                        if (cam.screenToGround(static_cast<float>(e.motion.x), static_cast<float>(e.motion.y), now)) {
                            cam.scrollBy(dragAnchor.x - now.x, dragAnchor.y - now.y);
                            cam.refresh();
                        }
                    }
                    if (dragRotate) {
                        // CCameraLevel::rotateCam: psi = psi0 + (x0 - x) * 4 / screenWidth
                        cam.rotateBy(static_cast<float>(lastMouseX - e.motion.x) * 4.0f / static_cast<float>(w));
                        lastMouseX = e.motion.x;
                    }
                    break;
                default: break;
            }
        }

        const float now = CUtils::getTimer();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;
        if (dt > 0.2f) dt = 0.2f;   // 0x3e4ccccd
        dt = dt * speedFactor(speedSetting);

        // keyboard scrolling, relative to the camera yaw
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        float fx = 0, fy = 0;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) fy += 1;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) fy -= 1;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) fx += 1;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) fx -= 1;
        if (fx != 0 || fy != 0) {
            const float speed = 600.0f / cam.zoom() * dt;
            const float c = std::cos(cam.yaw()), s = std::sin(cam.yaw());
            cam.scrollBy((fx * c - fy * s) * speed, (fx * s + fy * c) * speed);
        }
        cam.timeUpdate(dt);
        cam.refresh();

        gl::Viewport(0, 0, w, h);
        gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
        // fixed 30 fps for scripted runs (--frames), so screenshots repeat
        if (!opt.noSim) view.advance(opt.frames > 0 ? 1.0f / 30.0f : dt);
        view.draw(cam, w, h);

        ++frame;
        const bool last = opt.frames > 0 && frame >= opt.frames;
        if (last && !opt.screenshot.empty()) saveScreenshot(opt.screenshot, w, h);
        SDL_GL_SwapWindow(window);
        if (last) running = false;
    }
    CEngine::getSingletonPtr()->mGame = nullptr;
    game.reset();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parseArgs(argc, argv);
    CUtils::setLogFile("projecty.log");
    CUtils::setDataDir(opt.dataDir);
    CUtils::startTimer();
    CUtils::log("ProjectY PC port - starting (data: %s)", opt.dataDir.c_str());

    // --- CEngine::init equivalents ---
    CUtils::log("Math init");
    CMath math(4096);
    CUtils::log("Math init done");
    if (!CConfig::get().load(CUtils::dataPath("config.txt"))) {
        const std::string msg = "Could not read " + CUtils::dataPath("config.txt") +
                                "\n\nRun tools/extract_assets.py to create the data folder, "
                                "or start with --data <folder>.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ProjectY", msg.c_str(), nullptr);
        return 1;
    }
    CTxtLoc::get().load("English");
    const std::string version = CConfig::get().getString("Version", "?");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        CUtils::log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (opt.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_Window* window = SDL_CreateWindow(("ProjectY " + version + " (PC port)").c_str(), SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, opt.width, opt.height, flags);
    if (!window) {
        CUtils::log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GLContext context = nullptr;
    bool isES = false;
    if (!createContext(window, &context, &isES)) {
        CUtils::log("No usable OpenGL context: %s", SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ProjectY", "No usable OpenGL context.", window);
        return 1;
    }
    SDL_GL_SetSwapInterval(opt.frames > 0 ? 0 : 1);
    const int missing = gl::load(reinterpret_cast<gl::ProcLoader>(SDL_GL_GetProcAddress));
    CUtils::log("GL vendor: %s | renderer: %s | version: %s | missing functions: %d",
                reinterpret_cast<const char*>(gl::GetString(gl::VENDOR)),
                reinterpret_cast<const char*>(gl::GetString(gl::RENDERER)),
                reinterpret_cast<const char*>(gl::GetString(gl::VERSION)), missing);
    CGLSLProgram::setUseES(isES);
    if (!isES) {   // core profile needs a vertex array object
        unsigned vao = 0;
        gl::GenVertexArrays(1, &vao);
        gl::BindVertexArray(vao);
    }
    initOpenGLState();

    int rc = 0;
    if (opt.checkShaders) {
        const int failed = checkAllShaders();
        rc = failed == 0 ? 0 : 2;
    } else if (opt.viewer) {
        rc = runViewer(opt, window, version);
    } else {
        rc = runGame(opt, window);
    }

    CUtils::log("shutting down");
    CEngine::getSingletonPtr()->mGame = nullptr;
    CEngine::getSingletonPtr()->releaseInterfaces();
    CMeshManager::get().clear();
    CTextureManager::get().clear();
    CGLSLProgramManager::get().clear();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return rc;
}
