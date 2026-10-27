// Phase 5a: UI foundation - event queue, text metrics and wrapping, quads,
// cameras and element hit tests. None of these need an OpenGL context.
// Expected values are computed by hand (see the comments) or, for the real
// font, from font_lavoir_64.csv (tests that need data/ skip without it).
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "core/CMemoryFile.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "render/CCameraMenu.h"
#include "render/CQuad.h"
#include "render/CTextRender.h"
#include "test_main.h"
#include "ui/CInputScript.h"
#include "ui/CInterface.h"
#include "ui/CInterfaceElementButton2.h"
#include "ui/CInterfaceElementText.h"
#include "ui/CInterfaceIntro.h"
#include "ui/CInterfaceMenu.h"

using namespace py;

namespace {

// Records the handler calls of an interface.
class RecordingInterface : public CInterface {
public:
    std::vector<std::string> calls;
    bool emulate = true;
    CInterface* requeueTarget = nullptr;

    void draw() override {}
    void timeUpdate(float) override {}
    void processEventBackKeyPressed(CInterfaceEvent*) override { calls.push_back("back"); }
    void processEventMenuKeyPressed(CInterfaceEvent*) override { calls.push_back("menu"); }
    void processEventActionUp(CInterfaceEvent* e) override { calls.push_back("up" + xy(e)); }
    void processEventActionDown(CInterfaceEvent* e) override { calls.push_back("down" + xy(e)); }
    void processEventActionMove(CInterfaceEvent* e) override { calls.push_back("move" + xy(e)); }
    void processEventMenuAction(CInterfaceEvent* e) override {
        calls.push_back("action" + std::to_string(e->pointer));
        if (e->pointer == 1 && requeueTarget) requeueTarget->addEventMenuAction(2);   // queued while dispatching
    }
    void processEventFileDownloadFinished(CInterfaceEvent* e) override {
        calls.push_back("download" + std::to_string(e->pointer));
    }
    void setResolution(int, int) override {}
    CCamera* getCamera() override { return nullptr; }
    void processEventMouseButtonDown(CInterfaceEvent* e) override {
        calls.push_back("mdown" + std::to_string(e->button) + xy(e));
    }
    void processEventMouseButtonUp(CInterfaceEvent* e) override { calls.push_back("mup" + std::to_string(e->button)); }
    void processEventMouseMove(CInterfaceEvent* e) override { calls.push_back("mmove" + xy(e)); }
    void processEventMouseWheel(CInterfaceEvent* e) override {
        calls.push_back("wheel" + std::to_string(static_cast<int>(e->wheelY)));
    }
    void processEventKeyDown(CInterfaceEvent* e) override { calls.push_back("kdown" + std::to_string(e->keycode)); }
    void processEventKeyUp(CInterfaceEvent* e) override { calls.push_back("kup" + std::to_string(e->keycode)); }
    void processEventTextInput(CInterfaceEvent* e) override { calls.push_back("text:" + e->text); }
    bool mouseEmulatesTouch() const override { return emulate; }
    bool keysEmulateAndroidKeys() const override { return emulate; }

    static std::string xy(const CInterfaceEvent* e) {
        return "(" + std::to_string(static_cast<int>(e->x)) + "," + std::to_string(static_cast<int>(e->y)) + ")";
    }
};

// A synthetic font: every listed code advances `stored` (csv value + 1).
void uniformFont(CTextRender& tr, int csvValue) {
    std::string def;
    for (int c = 32; c < 128; ++c) def += std::to_string(c) + " " + std::to_string(csvValue) + "\n";
    tr.loadFontDefinitionFromBuffer(def);
}

bool haveFont() {
    if (CMemoryFile::resolve("font_lavoir_64.csv").empty()) {
        std::printf("    (skipped: font_lavoir_64.csv not found - run tools/extract_assets.py)\n");
        return false;
    }
    return true;
}

bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

}  // namespace

// ---------------------------------------------------------------- event queue

TEST(ui_event_queue_order_and_dispatch) {
    RecordingInterface ui;
    ui.addEventActionDown(0, 10, 20);
    ui.addEventActionMove(1, 11, 21);
    ui.addEventActionUp(0, 12, 22);
    ui.addEventBackKeyPressed();
    ui.addEventMenuKeyPressed();
    ui.addEventMenuAction(0x71);
    ui.addEventDownloadFinished(5);
    CHECK(ui.pendingEvents() == 7);
    CHECK(ui.calls.empty());   // nothing is dispatched before processEvents
    ui.processEvents();
    const std::vector<std::string> want = {"down(10,20)", "move(11,21)", "up(12,22)", "back",
                                           "menu",        "action113",   "download5"};
    CHECK(ui.calls == want);
    CHECK(ui.pendingEvents() == 0);
}

TEST(ui_event_fields) {
    // CInterfaceEvent as the original builds it: pointer, type, timestamp, x, y
    struct Probe : RecordingInterface {
        CInterfaceEvent last;
        void processEventActionDown(CInterfaceEvent* e) override { last = *e; }
    } ui;
    ui.addEventActionDown(3, -5, 700);
    ui.processEvents();
    CHECK(ui.last.pointer == 3);
    CHECK(ui.last.type == EVENT_ACTION_DOWN);
    CHECK(ui.last.x == -5.0f && ui.last.y == 700.0f);
}

TEST(ui_mouse_emulates_touch) {
    RecordingInterface ui;
    ui.addEventMouseMove(1, 1, 0, 0);                // no button held: PC event only
    ui.addEventMouseButtonDown(MOUSE_LEFT, 5, 6, 0);
    ui.addEventMouseMove(7, 8, 1, 0);                // left held: also a touch move
    ui.addEventMouseButtonDown(MOUSE_RIGHT, 9, 9, 0);   // right: PC only
    ui.addEventMouseButtonUp(MOUSE_RIGHT, 9, 9, 0);
    ui.addEventMouseButtonUp(MOUSE_LEFT, 7, 8, 0);
    ui.addEventMouseMove(2, 2, 0, 0);
    ui.addEventMouseWheel(0.0f, -1.0f, 0, 0, 0);
    ui.processEvents();
    const std::vector<std::string> want = {"mmove(1,1)", "mdown1(5,6)", "down(5,6)",  "mmove(7,8)", "move(7,8)",
                                           "mdown3(9,9)", "mup3",       "mup1",       "up(7,8)",    "mmove(2,2)",
                                           "wheel-1"};
    CHECK(ui.calls == want);
}

TEST(ui_mouse_emulation_off) {
    RecordingInterface ui;
    ui.emulate = false;
    ui.addEventMouseButtonDown(MOUSE_LEFT, 5, 6, 0);
    ui.addEventMouseMove(7, 8, 1, 0);
    ui.addEventMouseButtonUp(MOUSE_LEFT, 7, 8, 0);
    ui.addEventKeyDown(KEY_ESCAPE, 0, false);
    ui.processEvents();
    const std::vector<std::string> want = {"mdown1(5,6)", "mmove(7,8)", "mup1", "kdown27"};
    CHECK(ui.calls == want);
}

TEST(ui_keys_emulate_back_and_menu) {
    RecordingInterface ui;
    ui.addEventKeyDown(KEY_ESCAPE, 0, false);
    ui.addEventKeyDown(KEY_ESCAPE, 0, true);   // auto-repeat: no second Back
    ui.addEventKeyUp(KEY_ESCAPE, 0);
    ui.addEventKeyDown(KEY_MENU, 0, false);
    ui.addEventKeyDown('a', MOD_SHIFT, false);
    ui.addEventTextInput("A\xe4");
    ui.processEvents();
    const std::vector<std::string> want = {"kdown27", "back", "kdown27", "kup27", "kdown" + std::to_string(KEY_MENU),
                                           "menu",    "kdown97", "text:A\xe4"};
    CHECK(ui.calls == want);
}

TEST(ui_event_queued_during_dispatch_runs_in_same_pass) {
    RecordingInterface ui;
    ui.requeueTarget = &ui;
    ui.addEventMenuAction(1);
    ui.addEventBackKeyPressed();
    ui.processEvents();
    // the action queued by the handler goes to the back of the queue
    const std::vector<std::string> want = {"action1", "back", "action2"};
    CHECK(ui.calls == want);
}

// ---------------------------------------------------------------- text metrics

TEST(ui_text_metrics_synthetic_font) {
    CTextRender tr;
    // csv "65 31", "66 20", "32 9" -> stored advances 32, 21, 10
    tr.loadFontDefinitionFromBuffer("65 31\n66 20\n32 9\n");
    CHECK(tr.getCharWidth('A') == 32 && tr.getCharWidth('B') == 21 && tr.getCharWidth(' ') == 10);
    CHECK(tr.getCharWidth('C') == 0);
    // (32 + 21 + 10 + 32) / 64 * 64 = 95; calcTextLength doubles it
    CHECK(tr.calcTextPixelLength("AB A", 64.0f) == 95.0f);
    CHECK(tr.calcTextLength("AB A", 64.0f) == 190.0f);
    // size 10: 5 + 3.28125 + 1.5625 + 5 (all exact in binary)
    CHECK(tr.calcTextPixelLength("AB A", 10.0f) == 14.84375f);
    CHECK(tr.calcTextPixelLength("", 10.0f) == 0.0f);
}

TEST(ui_text_metrics_real_font) {
    if (!haveFont()) return;
    CTextRender tr;
    CHECK(tr.loadFontDefinition("font_lavoir_64.csv"));
    // csv: space 10, 'A' 32, 'W' 48, 255 30 -> stored 11, 33, 49, 31
    CHECK(tr.getCharWidth(' ') == 11 && tr.getCharWidth('A') == 33 && tr.getCharWidth('W') == 49);
    CHECK(tr.getCharWidth(255) == 31);
    // "PROJECTY": 32+37+41+33+30+33+35+35 = 276
    CHECK(tr.calcTextPixelLength("PROJECTY", 64.0f) == 276.0f);
    CHECK(tr.calcTextPixelLength("PROJECTY", 5.5f) == 23.71875f);   // 276 * 5.5 / 64
    // "Singleplayer\r" (as CTxtLoc returns it): 344 -> * 5 / 64
    CHECK(tr.calcTextPixelLength("Singleplayer\r", 5.0f) == 26.875f);
    CHECK(tr.calcTextLength("Hello World", 64.0f) == 638.0f);       // 2 * 319
}

// ---------------------------------------------------------------- wrapping

TEST(ui_break_text_pixel) {
    CTextRender tr;
    uniformFont(tr, 63);   // every char 64/64 * size = size wide
    // widths at size 1: "aaa"=3, "aaa bb"=6 (fits 6), "aaa bb cccc"=11 -> break after "bb"
    std::vector<std::string> l = tr.breakTextPixel(1.0f, 6.0f, 100, "aaa bb cccc d");
    CHECK(l.size() == 2 && l[0] == "aaa bb" && l[1] == "cccc d");
    // maxChars 7: the second break point (9) passes it -> one line cut at 7, the rest dropped
    l = tr.breakTextPixel(1.0f, 100.0f, 7, "aaaa bbbb cccc");
    CHECK(l.size() == 1 && l[0] == "aaaa bb");
    // a single word longer than the width stays whole (lastSpace == 0 guard)
    l = tr.breakTextPixel(1.0f, 3.0f, 100, "abcdefgh");
    CHECK(l.size() == 1 && l[0] == "abcdefgh");
    // no text
    l = tr.breakTextPixel(1.0f, 3.0f, 100, "");
    CHECK(l.size() == 1 && l[0].empty());
}

TEST(ui_break_text_prefix) {
    CTextRender tr;
    uniformFont(tr, 63);
    // calcTextLength = 2 per char: same breaks as width 6 above, at width 12.
    // The continuation "  cccc d" then breaks at its own first space (pos 1):
    //   "  " + "cccc" = 12 fits, "  cccc d" = 16 does not -> "  cccc" / "  d"
    const std::vector<std::string> l = tr.breakText(1.0f, 12.0f, 100, "aaa bb cccc d", "  ");
    CHECK(l.size() == 3 && l[0] == "aaa bb" && l[1] == "  cccc" && l[2] == "  d");
}

TEST(ui_draw_string_pixel_bounded_quirk) {
    CTextRender tr;
    uniformFont(tr, 63);
    CCamera cam;
    cam.initGLviewport(800, 600);
    tr.setCameraOverride(&cam);
    // No single-word guard: "abcdefgh" (8 > 5) emits "" and loses a letter
    // until "defgh" fits: "", "", "", "defgh", "ij"
    const float y = tr.drawStringPixelBounded(10.0f, 20.0f, 1.0f, 1, 1, 1, 1, 5.0f, 2.0f, "abcdefgh ij", 100);
    CHECK(y == 20.0f + 5.0f * 3.0f);   // y += size + spacing per line
    const auto& q = tr.queue();
    CHECK(q.size() == 5);
    if (q.size() == 5) {
        CHECK(q[0].text.empty() && q[1].text.empty() && q[2].text.empty());
        CHECK(q[3].text == "defgh" && q[4].text == "ij");
        // drawStringPixel: x' = ((x+x)/W - 1) * aspect, y' = (y*-2)/H + 1, size' = size/H
        CHECK(q[0].x == (20.0f / 800.0f - 1.0f) * (800.0f / 600.0f));
        CHECK(q[0].y == (20.0f * -2.0f) / 600.0f + 1.0f);
        CHECK(q[1].y == (23.0f * -2.0f) / 600.0f + 1.0f);
        CHECK(q[0].size == 1.0f / 600.0f);
        CHECK(q[0].align == 0);
    }
}

// ---------------------------------------------------------------- quads

TEST(ui_text_quads) {
    CTextRender tr;
    tr.loadFontDefinitionFromBuffer("65 31\n66 20\n");   // A: 32, B: 21
    tr.drawStringDirect(0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.5f, 0.25f, 0.5f, 0, "AB");
    CHECK(tr.buildQuads(false) == 2);
    const CQuadPosUVColor4& a = tr.quads()[0];
    // 'A' = 65: cell 33 -> row 2, col 1; advF = 0.5; w = 0.5 * 1.0 = 0.5, h = 1
    // centre (0.25, -0.5); p0 = (0, -1), p2 = (0.5, 0); uv tl (1/16, 2/16), size (1/32, 1/16)
    CHECK(a.v[0].pos[0] == 0.0f && a.v[0].pos[1] == -1.0f && a.v[0].pos[2] == 0.0f);
    CHECK(a.v[2].pos[0] == 0.5f && a.v[2].pos[1] == 0.0f);
    CHECK(a.v[5].pos[0] == 0.0f && a.v[5].pos[1] == 0.0f);   // p3 top-left
    CHECK(a.v[0].uv[0] == 0.0625f && a.v[0].uv[1] == 0.1875f);
    CHECK(a.v[2].uv[0] == 0.09375f && a.v[2].uv[1] == 0.125f);
    CHECK(a.v[3].pos[0] == a.v[0].pos[0] && a.v[4].pos[0] == a.v[2].pos[0]);   // p0 p1 p2 p0 p2 p3
    CHECK(a.v[0].color[0] == 1.0f && a.v[0].color[1] == 0.5f && a.v[0].color[3] == 0.5f);
    // 'B' starts where 'A' ends (x = 0.5) and is 21/64 * 2 * 0.5 wide
    const CQuadPosUVColor4& b = tr.quads()[1];
    CHECK(b.v[0].pos[0] == 0.5f && b.v[2].pos[0] == 0.5f + 0.328125f);
    // drawDirect premultiplies the colour by alpha
    CHECK(tr.buildQuads(true) == 2 && tr.quads()[0].v[0].color[0] == 0.5f && tr.quads()[0].v[0].color[2] == 0.125f);
    // centred: offset -calcTextLength/2 = -(0.5 + 0.328125)/2
    tr.clearQueue();
    tr.drawStringDirect(0.0f, 0.0f, 0.0f, 0.5f, 1, 1, 1, 1, 1, "AB");
    tr.buildQuads(false);
    CHECK(tr.quads()[0].v[0].pos[0] == -0.4140625f);
    // right aligned: the whole length
    tr.clearQueue();
    tr.drawStringDirect(0.0f, 0.0f, 0.0f, 0.5f, 1, 1, 1, 1, 2, "AB");
    tr.buildQuads(false);
    CHECK(tr.quads()[0].v[0].pos[0] == -0.828125f);
}

TEST(ui_quad_layouts) {
    // CQuadPosUV from a top-left corner (2D ctor): centre (x + w/2, y - h/2)
    const CQuadPosUV q(CVector2(10.0f, 20.0f), CVector2(4.0f, 2.0f), CVector2(0.0f, 0.0f), CVector2(1.0f, 1.0f));
    CHECK(q.v[0].pos[0] == 10.0f && q.v[0].pos[1] == 18.0f);   // bottom-left
    CHECK(q.v[2].pos[0] == 14.0f && q.v[2].pos[1] == 20.0f);   // top-right
    CHECK(q.v[0].uv[1] == 1.0f && q.v[5].uv[1] == 0.0f);       // image top at the quad top
    // ring quad: WIDTH = width / r, uv from (-1,-1) to (1,1)
    const CQuadPosUVColor4Width r(Vec3(0, 0, 0), 4.0f, 1.0f, Vec3(1, 0, 0), 0.5f);
    CHECK(r.v[0].width == 0.25f && r.v[0].uv[0] == -1.0f && r.v[0].uv[1] == 1.0f && r.v[2].uv[0] == 1.0f);
    CHECK(r.v[0].pos[0] == -4.0f && r.v[2].pos[1] == 4.0f);
    // matrix ctor with identity: corners (+-hx, +-hy, z)
    const float id[16] = {1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 0, 0, 0, 0, 1};
    const CQuadPosUVTTL t(2.0f, id, CVector2(1.0f, 3.0f), CVector2(0.5f, 0.5f), CVector2(0.25f, 0.25f), 0.7f);
    CHECK(t.v[0].pos[0] == 4.0f && t.v[0].pos[1] == 3.0f && t.v[0].pos[2] == 2.0f && t.v[0].ttl == 0.7f);
    CHECK(t.v[5].pos[0] == 4.0f && t.v[5].pos[1] == 9.0f && t.v[5].uv[0] == 0.5f && t.v[5].uv[1] == 0.5f);
}

// ---------------------------------------------------------------- camera + hit tests

namespace {
// The menu camera of a 1280x720 screen. Expected screen <-> world mapping
// (computed here in double, independently of CCamera): the camera looks
// straight down from 520 / zoom; half the screen height covers
// (520 / zoom) * tan(25 deg) world units.
struct MenuFixture {
    std::unique_ptr<CInterfaceMenu> menu = std::make_unique<CInterfaceMenu>();
    double unitsPerPixel = 0;
    MenuFixture() {
        CEngine* e = CEngine::getSingletonPtr();
        e->mTextRender = std::make_unique<CTextRender>();
        e->mTextRender->loadFontDefinitionFromBuffer("65 31\n66 20\n32 9\n");
        menu->mCamera = std::make_unique<CCameraMenu>();
        menu->setResolution(1280, 720);
        const double a = 1280.0 / 720.0;
        // CInterfaceMenu::setResolution constants 0x4079bd87, 0x3deec031, 0xbfa816ed, 0x3f666666
        const double zoom = (a * 3.9021928310394287 - 0.11657751351594925 + a * a * -1.3131996393203735) * 0.9;
        unitsPerPixel = (520.0 / zoom) * std::tan(25.0 * 3.14159265358979 / 180.0) / 360.0;
    }
    ~MenuFixture() { CEngine::getSingletonPtr()->mTextRender.reset(); }
    // screen pixel of a world point
    int sx(double wx) const { return static_cast<int>(std::lround(640.0 + wx / unitsPerPixel)); }
    int sy(double wy) const { return static_cast<int>(std::lround(360.0 - wy / unitsPerPixel)); }
    CInterfaceEvent at(double wx, double wy) const {
        CInterfaceEvent e;
        e.x = static_cast<float>(sx(wx));
        e.y = static_cast<float>(sy(wy));
        return e;
    }
};
}  // namespace

TEST(ui_menu_camera_plane_hit) {
    MenuFixture f;
    const CCameraMenu& cam = *f.menu->mCamera;
    CHECK(near(cam.mZoom, 2.4032653f, 1e-5f));
    const CVector2 c = cam.viewportPlaneHit(CVector2(640.0f, 360.0f));
    CHECK(near(c.x, 0.0f, 1e-3f) && near(c.y, 0.0f, 1e-3f));
    // top-left corner: (-640, +360) pixels from the centre
    const CVector2 tl = cam.viewportPlaneHit(CVector2(0.0f, 0.0f));
    CHECK(near(tl.x, static_cast<float>(-640.0 * f.unitsPerPixel), 2e-3f));
    CHECK(near(tl.y, static_cast<float>(360.0 * f.unitsPerPixel), 2e-3f));
    // the camera follows its target
    f.menu->mCamera->mTargetX = 100.0f;
    f.menu->mCamera->mTargetY = -50.0f;
    f.menu->mCamera->refresh();
    const CVector2 c2 = cam.viewportPlaneHit(CVector2(640.0f, 360.0f));
    CHECK(near(c2.x, 100.0f, 1e-3f) && near(c2.y, -50.0f, 1e-3f));
}

TEST(ui_menu_camera_slow_scroll) {
    CCameraMenu cam;
    cam.initGLviewport(1280, 720);
    cam.mTargetY = -500.0f;
    cam.slowScrollTo(CVector2(0.0f, 0.0f), 10.0f);
    // k = min(10 * dt, 0.5): dt 0.1 -> halves the distance per update
    cam.timeUpdate(0.1f);
    CHECK(cam.mTargetY == -250.0f);
    cam.timeUpdate(0.01f);   // k = 0.1
    CHECK(near(cam.mTargetY, -225.0f, 1e-4f));
}

TEST(ui_text_element_hit) {
    MenuFixture f;
    // "AB A" size 8: hit box width = calcTextPixelLength = 95/64*8 = 11.875, height 8, top at y
    CInterfaceElementText left(f.menu.get(), 10.0f, 20.0f, "AB A", 0, 8.0f, Vec3(1, 1, 1));
    CHECK(left.mWidth == 11.875f && left.mHeight == 8.0f);
    CInterfaceEvent in = f.at(16.0, 16.0), outR = f.at(23.5, 16.0), outUp = f.at(16.0, 21.5), outDown = f.at(16.0, 10.5);
    CHECK(left.isHit(&in));
    CHECK(!left.isHit(&outR) && !left.isHit(&outUp) && !left.isHit(&outDown));
    CInterfaceElementText centre(f.menu.get(), 10.0f, 20.0f, "AB A", 1, 8.0f, Vec3(1, 1, 1));
    CInterfaceEvent c1 = f.at(5.0, 16.0), c2 = f.at(2.5, 16.0);
    CHECK(centre.isHit(&c1) && !centre.isHit(&c2));   // x in (4.06, 15.94)
    CInterfaceElementText right(f.menu.get(), 10.0f, 20.0f, "AB A", 2, 8.0f, Vec3(1, 1, 1));
    CInterfaceEvent r1 = f.at(0.0, 16.0), r2 = f.at(11.5, 16.0);
    CHECK(right.isHit(&r1) && !right.isHit(&r2));     // x in (-1.875, 10)
}

TEST(ui_button2_hit_and_action) {
    MenuFixture f;
    // main menu "Exit": (1, -50.3) + offset (1.8, 4.3), w 35 -> box 45 wide, 17.5 high, right aligned
    CInterfaceElementButton2 b(f.menu.get(), 6, "", 1.0f, bitsf(0xc2493333), "Exit", 2, false, 5.0f, Vec3(1, 1, 1),
                               35.0f, 12.0f, bitsf(0x3fe66666), bitsf(0x4089999a));
    CHECK(b.mHitW == 45.0f && b.mHitH == 17.5f);
    CHECK(b.mMeshFirst == 0 && b.mMeshCount == 2);   // empty submesh name
    // box: x in (2.8 - 45, 2.8), y in (-46 - 17.5, -46)
    CInterfaceEvent inside = f.at(-20.0, -55.0), leftOut = f.at(-43.5, -55.0), below = f.at(-20.0, -64.5);
    CHECK(b.isHit(&inside) && !b.isHit(&leftOut) && !b.isHit(&below));
    // down + up inside -> processAction(6): the placeholder menu asks the shell to quit
    CEngine::getSingletonPtr()->mQuitRequested = false;
    b.processEventActionDown(&inside);
    CHECK(b.mPressed);
    CHECK(b.processEventActionUp(&inside) == 1);
    CHECK(!b.mPressed && b.mClicked);
    CHECK(CEngine::getSingletonPtr()->mQuitRequested);
    CEngine::getSingletonPtr()->mQuitRequested = false;
    // released outside: nothing
    CInterfaceElementButton2 c(f.menu.get(), 6, "", 1.0f, 0.0f, "x", 2, true, 5.0f, Vec3(1, 1, 1), 0, 0, 0, 0);
    CHECK(c.processEventActionUp(&leftOut) == 0 && !CEngine::getSingletonPtr()->mQuitRequested);
    // disabled buttons ignore touches
    c.mDisabled = true;
    CInterfaceEvent cin = f.at(-1.0, -1.0);
    CHECK(!c.isHit(&leftOut));
    c.processEventActionDown(&cin);
    CHECK(!c.mPressed);
}

TEST(ui_button2_press_animation) {
    MenuFixture f;
    CInterfaceElementButton2 b(f.menu.get(), 1, "", 0.0f, 0.0f, "x", 0, true, 5.0f, Vec3(1, 1, 1), 0, 0, 0, 0);
    CHECK(b.animate(0.1f) == 0.0f);   // at rest: t clamps at 0, no red
    b.mPressed = true;
    b.animate(0.1f);
    CHECK(near(b.mAnim, 0.2f, 1e-6f));
    b.animate(0.1f);
    b.animate(0.1f);   // 0.4 -> 0.6 -> back to 0.4: hovers below 0.5
    CHECK(near(b.mAnim, 0.4f, 1e-6f));
    const float k = b.animate(0.1f);
    CHECK(near(k, 0.9045085f, 2e-3f));   // (1 - cos(0.8 pi)) / 2
    b.mPressed = false;
    b.animate(0.1f);
    CHECK(near(b.mAnim, 0.2f, 1e-6f));
    b.mClicked = true;   // a click runs t up past 1, then ends
    for (int i = 0; i < 5; ++i) b.animate(0.1f);
    CHECK(!b.mClicked && b.mAnim == 0.0f);
}

// ---------------------------------------------------------------- intro, strings, scale, script

TEST(ui_intro_timeline) {
    int phase = -1;
    float a = -1.0f;
    CInterfaceIntro::phaseAt(0.5f, phase, a);
    CHECK(phase == 0 && a == 0.5f);
    CInterfaceIntro::phaseAt(1.5f, phase, a);
    CHECK(phase == 0 && a == 1.0f);
    CInterfaceIntro::phaseAt(2.25f, phase, a);
    CHECK(phase == 0 && a == 0.75f);
    CInterfaceIntro::phaseAt(3.5f, phase, a);
    CHECK(phase == 1 && a == 0.5f);
    CInterfaceIntro::phaseAt(5.75f, phase, a);
    CHECK(phase == 1 && a == 0.25f);
    CInterfaceIntro::phaseAt(6.5f, phase, a);
    CHECK(phase == 2 && a == 0.5f);
    CInterfaceIntro::phaseAt(9.0f, phase, a);
    CHECK(phase == 2 && a == 1.0f);
}

TEST(ui_txtloc_indexed_strings) {
    CTxtLoc t;
    // A comment line in the body: its words become keys (original strtok
    // sequence), so "B" lands in slot 3, not 2.
    t.loadFromBuffer("# header line\nA alpha\r\n# c d\nB beta two\r\n");
    CHECK(t.getS(0) == "alpha\r");
    CHECK(t.getS(2) == "d");
    CHECK(t.getS(3) == "beta two\r");
    CHECK(t.getS(-1).empty() && t.getS(CTxtLoc::kNumIds).empty());
}

TEST(ui_txtloc_real_strings) {
    if (CMemoryFile::resolve("stringsEnglish.txt").empty()) {
        std::printf("    (skipped: stringsEnglish.txt not found)\n");
        return;
    }
    CTxtLoc t;
    CHECK(t.load("English"));
    // index = line number - 2: line 9 "SingleMenuButton Singleplayer", line 17 "MenuMain PROJECTY"
    CHECK(t.getS(7) == "Singleplayer\r");
    CHECK(t.getS(0xf) == "PROJECTY\r");
    CHECK(std::string(t.getC(6)) == "Back\r");
}

TEST(ui_gui_scale_from_dpi) {
    CEngine::release();
    CEngine* e = CEngine::getSingletonPtr();
    // desktop DPI below 100 is "implausible": both axes become 160 -> 320 / 436.21
    e->setDPI(96.0f, 96.0f, 1280, 720);
    CHECK(e->mGUIScaleDPI == 320.0f / bitsf(0x43da1abe));
    CHECK(e->mGUIScale == e->mGUIScaleDPI * 1.0f);   // option 3 = x1
    e->setDPI(400.0f, 400.0f, 1920, 1080);
    CHECK(near(e->mGUIScale, 1.833979f, 1e-5f));
    e->mGUIScalingOption = 4;   // x1.5
    e->updateGUIScaling();
    CHECK(e->mGUIScale == e->mGUIScaleDPI * 1.5f);
    CEngine::release();
}

TEST(ui_input_script_parse) {
    auto keys = [](const std::string& n) -> int32_t {
        if (n == "escape") return KEY_ESCAPE;
        if (n.size() == 1) return n[0];
        return 0;
    };
    CInputScript s;
    const bool ok = s.parse(
        "# comment\n"
        "120 mousedown left 640 360\n"
        "key escape            # same frame\n"
        "+10 click right 5 6 shift\n"
        "200 key ctrl+a\n"
        "text hello world\n"
        "300 wheel -2 100 200\n"
        "touchdown 1 7 8\n"
        "menuaction 113\n"
        "400 screenshot out.png\n"
        "quit\n",
        keys);
    CHECK_MSG(ok, "%s", s.error().c_str());
    const auto& ev = s.events();
    CHECK(ev.size() == 13);
    if (ev.size() == 13) {
        CHECK(ev[0].frame == 120 && ev[0].kind == ScriptEvent::MOUSE_DOWN && ev[0].button == MOUSE_LEFT &&
              ev[0].x == 640 && ev[0].y == 360);
        CHECK(ev[1].frame == 120 && ev[1].kind == ScriptEvent::KEY_DOWN && ev[1].keycode == KEY_ESCAPE);
        CHECK(ev[2].frame == 120 && ev[2].kind == ScriptEvent::KEY_UP);
        CHECK(ev[3].frame == 130 && ev[3].kind == ScriptEvent::MOUSE_DOWN && ev[3].button == MOUSE_RIGHT &&
              ev[3].modifiers == MOD_SHIFT);
        CHECK(ev[4].frame == 130 && ev[4].kind == ScriptEvent::MOUSE_UP);
        CHECK(ev[5].frame == 200 && ev[5].keycode == 'a' && ev[5].modifiers == MOD_CTRL);
        CHECK(ev[7].kind == ScriptEvent::TEXT && ev[7].text == "hello world" && ev[7].frame == 200);
        CHECK(ev[8].kind == ScriptEvent::WHEEL && ev[8].wheelY == -2.0f && ev[8].x == 100 && ev[8].y == 200);
        CHECK(ev[9].kind == ScriptEvent::TOUCH_DOWN && ev[9].pointer == 1 && ev[9].frame == 300);
        CHECK(ev[10].kind == ScriptEvent::MENU_ACTION && ev[10].pointer == 113);
        CHECK(ev[11].kind == ScriptEvent::SCREENSHOT && ev[11].text == "out.png" && ev[11].frame == 400);
        CHECK(ev[12].kind == ScriptEvent::QUIT && ev[12].frame == 400);
    }
    CHECK(s.eventsAt(120).size() == 3 && s.lastFrame() == 400);
    CHECK(!s.parse("10 mousedown sideways 1 2\n", keys) && s.error().find("line 1") == 0);
    CHECK(!s.parse("10 key nosuchkey\n", keys));
    CHECK(!s.parse("10 jump\n", keys));

    // dispatch onto an interface: the held-button mask follows the script
    RecordingInterface ui;
    unsigned held = 0;
    CInputScript ok2;
    ok2.parse("mousedown left 1 2\nmousemove 3 4\nmouseup left 3 4\n", keys);
    for (const ScriptEvent& e : ok2.events()) CInputScript::dispatch(e, &ui, held);
    ui.processEvents();
    const std::vector<std::string> want = {"mdown1(1,2)", "down(1,2)", "mmove(3,4)", "move(3,4)", "mup1", "up(3,4)"};
    CHECK(ui.calls == want);
    CHECK(held == 0);
}
