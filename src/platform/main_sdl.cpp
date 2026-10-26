// main_sdl.cpp - PC replacement for the Android Java shell.
//
// The original Activity (com.luminencelabs.ProjectY.gf.ProjectY) created a
// GLSurfaceView and forwarded to JNI: AppOnCreate -> CEngine::init,
// GL2View.Draw -> CEngine::draw every frame, MotionEvent -> touch input,
// AppOnPause/Resume, KeyBack/KeyMenu. This file plays that role with SDL2.
//
// Phase 0 scope: window + GL context, data directory, CMath/CConfig/CTxtLoc
// initialisation, and the original frame-time logic of CEngine::draw
// (dt = elapsed ms / 1000, capped at 0.2 s, times the game-speed factor).
#include <SDL.h>

#include <cstdio>
#include <string>

#include "core/CConfig.h"
#include "core/CMath.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "platform/GLFunctions.h"

using namespace py;

namespace {

struct Options {
    std::string dataDir = "data";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    int frames = -1;   // run N frames then quit (for automated tests)
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](int& out) {
            if (i + 1 < argc) out = std::atoi(argv[++i]);
        };
        if (a == "--data" && i + 1 < argc) {
            o.dataDir = argv[++i];
        } else if (a == "--width") {
            next(o.width);
        } else if (a == "--height") {
            next(o.height);
        } else if (a == "--fullscreen") {
            o.fullscreen = true;
        } else if (a == "--frames") {
            next(o.frames);
        }
    }
    return o;
}

// The game-speed table from CEngine::getSpeedFactor.
float speedFactor(int setting) {
    static const float table[] = {0.1f, 0.2f, 0.5f, 1.0f, 1.5f, 2.0f};
    if (setting < 0 || setting > 5) return 1.0f;
    return table[setting];
}

bool createContext(SDL_Window* win, SDL_GLContext* out) {
    // Prefer desktop GL 3.3 core; fall back to GLES 2.0 (e.g. via ANGLE).
    struct Attempt {
        int profile, major, minor;
        const char* name;
    } attempts[] = {
        {SDL_GL_CONTEXT_PROFILE_CORE, 3, 3, "OpenGL 3.3 core"},
        {SDL_GL_CONTEXT_PROFILE_ES, 3, 0, "OpenGL ES 3.0"},
        {SDL_GL_CONTEXT_PROFILE_ES, 2, 0, "OpenGL ES 2.0"},
    };
    for (const auto& a : attempts) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, a.profile);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, a.major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, a.minor);
        if (SDL_GLContext ctx = SDL_GL_CreateContext(win)) {
            CUtils::log("GL context: %s", a.name);
            *out = ctx;
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parseArgs(argc, argv);
    CUtils::setLogFile("projecty.log");
    CUtils::setDataDir(opt.dataDir);
    CUtils::startTimer();
    CUtils::log("ProjectY PC port - starting (data: %s)", opt.dataDir.c_str());

    // --- CEngine::init equivalents (phase 0 subset) ---
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

    const std::string title = "ProjectY " + version + " (PC port)";
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (opt.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          opt.width, opt.height, flags);
    if (!window) {
        CUtils::log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GLContext context = nullptr;
    if (!createContext(window, &context)) {
        CUtils::log("No usable OpenGL context: %s", SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ProjectY", "No usable OpenGL context.", window);
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    const int missing = gl::load(reinterpret_cast<gl::ProcLoader>(SDL_GL_GetProcAddress));
    CUtils::log("GL vendor: %s | renderer: %s | version: %s | missing functions: %d",
                reinterpret_cast<const char*>(gl::GetString(gl::VENDOR)),
                reinterpret_cast<const char*>(gl::GetString(gl::RENDERER)),
                reinterpret_cast<const char*>(gl::GetString(gl::VERSION)), missing);

    // --- frame loop: the timing half of CEngine::draw ---
    int speedSetting = 3;   // "normal" (factor 1.0); CEngine+0x1b8
    float lastTime = CUtils::getTimer();
    float fpsTimer = 0.0f;
    int fpsFrames = 0;
    bool running = true;
    int frame = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;   // KeyBack
        }

        const float now = CUtils::getTimer();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;
        if (dt > 0.2f) dt = 0.2f;   // 0x3e4ccccd
        dt = dt * speedFactor(speedSetting);

        fpsTimer += dt;
        ++fpsFrames;
        if (fpsTimer >= 1.0f) {
            CUtils::log("fps %.1f", static_cast<float>(fpsFrames) / fpsTimer);
            fpsTimer = 0.0f;
            fpsFrames = 0;
        }

        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(window, &w, &h);
        gl::Viewport(0, 0, w, h);
        gl::ClearColor(0.05f, 0.07f, 0.10f, 1.0f);
        gl::Clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
        SDL_GL_SwapWindow(window);

        if (opt.frames > 0 && ++frame >= opt.frames) running = false;
    }

    CUtils::log("shutting down");
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
