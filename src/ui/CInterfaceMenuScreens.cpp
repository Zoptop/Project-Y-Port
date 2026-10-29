// CInterfaceMenuScreens.cpp - CInterfaceMenu::setupElements (Ghidra
// 0x1bbcb8, decompiled in a separate run: decompiled/ProjectY__CInterfaceMenu_
// setupElements.c) and its createElement* builders.
//
// Every number below is the original's (world units of the menu plane;
// hex float patterns where they are not short decimals). Button2 arguments:
// (action, submesh, x, y, text, align, autoSize, size, colour, w, h, hitOffX,
// hitOffY). Strings: CTxtLoc ids (strings<Language>.txt).
//
// Ported screens: 0 main, 1 single player, 2 multiplayer (layout; the
// network actions log notPorted), 3 profile (no lobby: the online options are
// disabled, as offline in the original), 4 ingame, 5/6 options, 7/8 save/load
// (layout; every slot is empty until savegames are ported), 9 tutorial,
// 10 custom game, 11 statistics, 12 aftermath, 13 download (layout), 14
// credits, 15 split screen, 16 host, 17 enter IP, 18 server list (no list
// element yet), 20 waiting room (layout), 21 update news (title and back),
// 25 campaign, 26 campaign progress (episode list). Others: a stand-in
// page with the screen number and Back ([PC], marked).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/CConfig.h"
#include "core/CMemoryFile.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "render/CCameraMenu.h"
#include "render/Colors.h"
#include "ui/CInterfaceElement.h"
#include "ui/CInterfaceElementButton2.h"
#include "ui/CInterfaceElementCampaignButtons.h"
#include "ui/CInterfaceElementCampaignImageSlider.h"
#include "ui/CInterfaceElementImage.h"
#include "ui/CInterfaceElementImageButton.h"
#include "ui/CInterfaceElementImageSlider.h"
#include "ui/CInterfaceElementMultiText.h"
#include "ui/CInterfaceElementPlayerControl.h"
#include "ui/CInterfaceElementText.h"
#include "ui/CInterfaceElementTextInput2.h"
#include "ui/CInterfaceElementTextSlider2.h"
#include "ui/CInterfaceElementValueBar.h"
#include "ui/CInterfaceMenu.h"

namespace py {

namespace {

const char* T(int id) { return CTxtLoc::get().getC(id); }

const Vec3& white() { return colors::COL_WHITE; }

// The green start buttons: mesh (0.2, 0.6, 0.3), text 0.7 white + 0.3 green
const Vec3 kStartMesh(bitsf(0x3e4ccccd), bitsf(0x3f19999a), bitsf(0x3e99999a));
Vec3 startTextColor() {
    const Vec3& w = colors::COL_WHITE;
    const Vec3& g = colors::COL_GREEN;
    const float a = bitsf(0x3f333333), b = bitsf(0x3e99999a);
    return Vec3(w.x * a + g.x * b, w.y * a + g.y * b, w.z * a + g.z * b);
}

}  // namespace

// Ghidra 0x1bbcb8
void CInterfaceMenu::setupElements(int menuType, int action) {
    clearElements();
    mBackAction = 0;   // +0x5c0
    mStartButton = nullptr;
    mMapSlider = nullptr;
    mPlayerControl = nullptr;
    mTextInput = nullptr;
    mReadyButton = nullptr;
    mCampaignStartButton = nullptr;
    mCampaignReplayButton = nullptr;
    mCampaignSlider = nullptr;
    mDownloadStatus = nullptr;
    mDownloadTitle = nullptr;
    mDownloadBar = nullptr;
    CEngine* e = mEngine;
    const Vec3 colorG = e->mMenuColorG;   // engine+0x230, the screen titles
    auto title = [&](uint32_t x, uint32_t y, const char* text) {
        addElement(new CInterfaceElementText(this, bitsf(x), bitsf(y), text, 1, 5.5f, colorG));
    };
    auto button = [&](int act, const char* mesh, uint32_t x, uint32_t y, const char* text, int align, uint32_t w,
                      uint32_t ox, uint32_t oy, float size = 5.0f) {
        return addElement(new CInterfaceElementButton2(this, act, mesh, bitsf(x), bitsf(y), text, align, false, size,
                                                       white(), bitsf(w), 12.0f, bitsf(ox), bitsf(oy)));
    };
    switch (menuType) {
        case MENU_MAIN: {
            title(0xc1840000, 0x42a00000, T(0xf));
            addElement(new CInterfaceElementText(this, 24.0f, 79.0f, "-full version-", 0, bitsf(0x40840000),
                                                 colors::COL_GOLD));
            button(1, "main_singl", 0x41ca6666, 0x423e0000, T(7), 2, 0x42580000, 0x40c00000, 0x40800000);
            button(2, "main_multi", 0x41380000, 0x41d26666, T(8), 0, 0x42500000, 0xc0c9999a, 0x40733333);
            button(4, "main_optio", 0xbfd9999a, 0xc1680000, T(9), 2, 0x42480000, 0x40b00000, 0x4089999a);
            button(6, "main_exit", 0x3f800000, 0xc2493333, T(10), 2, 0x420c0000, 0x3fe66666, 0x4089999a);
            button(3, "main_profi", 0xc21a0000, 0xc2080000, T(0x24c), 0, 0x41f00000, 0xc0b9999a, 0x4089999a);
            // Hall of fame: disabled while the lobby is not connected (never, on PC)
            button(0x47, "main_hof", 0xc0a33333, 0x41d40000, T(599), 2, 0x42400000, 0x40b9999a, 0x4089999a)->mDisabled =
                true;
            button(0x23, "main_credi", 0xbf800000, 0x40b9999a, T(0x91), 2, 0x42400000, 0x40e00000, 0x40733333);
            button(0x25, "main_websi", 0x41900000, 0xc0cccccd, T(0x9f), 0, 0x42480000, 0xc099999a, 0x4089999a);
            button(0x26, "main_gameg", 0x41926666, 0xc1c26666, T(0xa1), 0, 0x42480000, 0xc0e9999a, 0x40533333);
            button(0x27, "main_forum", 0x4134cccd, 0xc2273333, T(0xa0), 0, 0x42200000, 0xc089999a, 0x40200000);
            addElement(new CInterfaceElementImageButton(this, 0x57, "btn_twitter.png", -65.0f, -2.0f, 15.0f, 15.0f));
            addElement(new CInterfaceElementImageButton(this, 0x59, "btn_facebook.png", -65.0f, -19.0f, 15.0f, 15.0f));
            char ver[300];
            std::snprintf(ver, sizeof ver, "v%s Rev %s", CConfig::get().getString("Version", "").c_str(),
                          CConfig::get().getString("Revision", "").c_str());
            const Vec3 verColor(colorG.x * 0.75f, colorG.y * 0.75f, colorG.z * 0.75f);
            addElement(new CInterfaceElementButton2(this, 0x42, "main_upd", bitsf(0x4144cccd), bitsf(0x427ecccd), ver,
                                                    1, false, 3.25f, verColor, 60.0f, 12.0f, bitsf(0x40533333), 2.5f));
            mBackAction = 6;
            break;
        }
        case MENU_SINGLEPLAYER:
            title(0x4305cccd, 0x43a80000, T(0x11));
            button(0x4d, "sp_camp", 0x42e30000, 0x439d6666, T(0x1b0), 0, 0x42700000, 0xc1200000, 0x40a00000)
                ->mDisabled = false;
            button(9, "sp_tut", 0x42b00000, 0x43936666, T(0x12), 0, 0x42700000, 0xc1200000, 0x40a00000);
            button(10, "sp_custom", 0x42aa999a, 0x43888000, T(0x13), 0, 0x42480000, 0xc0a9999a, 0x4089999a);
            button(0xb, "sp_load", 0x43124ccd, 0x437b8000, T(0x14), 2, 0x42480000, 0x40a00000, 0x40a00000);
            button(7, "sp_back", 0x42b6999a, 0x4368cccd, T(6), 0, 0x42200000, 0xc1200000, 0x40800000);
            mBackAction = 7;
            break;
        case MENU_MULTIPLAYER: {
            title(0x439f8000, 0x427c0000, T(8));
            button(0x33, "multi_spli", 0x4396c000, 0x421a0000, T(0xe1), 0, 0x42700000, 0xc0f00000, 0x4099999a);
            button(0x34, "multi_host", 0x438cc000, 0x41a66666, T(0xe2), 0, 0x42a00000, 0, 0x40733333);
            button(0x35, "multi_lan", 0x43b52666, 0x3fa66666, T(0xef), 2, 0x42a00000, 0x41700000, 0x40800000);
            button(0x36, "multi_dire", 0x43ba2666, 0xc1840000, T(0xf0), 2, 0x42a00000, 0x41200000, 0x40900000);
            // the public list and the lobby chat need the lobby connection
            button(0x37, "multi_publ", 0x43ad4000, 0xc1fc0000, T(0xf1), 2, 0x42960000, 0x41200000, 0x40900000)
                ->mDisabled = true;
            button(0x38, "multi_chat", 0x43ab4000, 0xc2493333, T(0x21a), 2, 0x42480000, 0x4104cccd, 0x4089999a)
                ->mDisabled = true;
            button(7, "multi_back", 0x438ba666, 0xc2873333, T(6), 0, 0x42480000, 0xc0e9999a, 0x40400000);
            mBackAction = 7;
            break;
        }
        case MENU_PROFILE: {
            e->validateNameAndClan();
            title(0xc4693333, 0x44251333, T(0x23c));
            char acc[64];
            std::snprintf(acc, sizeof acc, "Account ID: %li", e->mAccountId);
            addElement(new CInterfaceElementText(this, bitsf(0xc4598000), bitsf(0x44251333), acc, 0, 4.0f,
                                                 Vec3(colorG.y, colorG.z, colorG.x)));   // [!] the components rotated
            auto* nick = addElement(new CInterfaceElementTextInput2(
                this, 0x43, "prof_nick", bitsf(0xc4700000), bitsf(0x441f5333), true, bitsf(0x44084000),
                bitsf(0x44124000), "Nick:", 0, false, 4.0f, Vec3(colorG.y, colorG.z, colorG.x), 50.0f, 10.0f, -10.0f,
                7.0f));
            nick->mValue = e->mPlayerName;
            nick->mValueOffX = 15.0f;
            nick->mClearOnTap = false;
            nick->mMaxLength = 10;
            auto* clan = addElement(new CInterfaceElementTextInput2(
                this, 0x55, "prof_clan", bitsf(0xc4628000), bitsf(0x441f5333), true, bitsf(0x44084000),
                bitsf(0x44124000), "Clan:", 0, false, 4.0f, Vec3(colorG.y, colorG.z, colorG.x), 37.0f, 10.0f, -10.0f,
                7.0f));
            clan->mValue = e->mPlayerClan;
            clan->mValueOffX = 18.0f;
            clan->mClearOnTap = false;
            clan->mMaxLength = 5;
            clan->mBrackets = true;
            button(7, "prof_back", 0xc45e8000, 0x44024000, T(6), 0, 0x42480000, 0xc0a00000, 0x40800000);
            button(0x48, "prof_ach", 0xc4637333, 0x4402a000, T(0x18e), 2, 0x42700000, 0x41200000, 0x40800000);
            auto profSlider = [&](int act, const char* mesh, uint32_t x, uint32_t y, const char* label) {
                return addElement(new CInterfaceElementTextSlider2(this, act, mesh, bitsf(x), bitsf(y), label, 0, 4.0f,
                                                                   white(), 100.0f, 20.0f, 0.0f, -8.0f, 50.0f, 20.0f,
                                                                   5.0f, 2.0f));
            };
            auto* priv = profSlider(0x65, "prof_priva", 0xc4590000, 0x4414c000, T(0x241));
            priv->addValue(0, CTxtLoc::get().getS(0x222));
            priv->addValue(1, CTxtLoc::get().getS(0x223));
            priv->mValue = e->mProfilePrivate ? 0u : 1u;
            priv->mDisabled = true;   // the lobby is not connected
            auto* online = profSlider(0x66, "prof_onlin", 0xc4590000, 0x440f0000, T(0x242));
            online->addValue(0, CTxtLoc::get().getS(0x228));
            online->addValue(1, CTxtLoc::get().getS(0x227));
            online->mValue = e->mOnlineStatusPrivate ? 0u : 1u;
            online->mDisabled = true;
            auto* ranked = profSlider(0x67, "prof_ranke", 0xc4590000, 0x44084000, T(0x243));
            ranked->addValue(0, CTxtLoc::get().getS(0x229));
            ranked->addValue(1, CTxtLoc::get().getS(0x22a));
            mRankedOption = 1;   // not connected: unranked, disabled
            ranked->mValue = 1;
            ranked->mDisabled = true;
            auto* account = profSlider(0x46, "prof_accou", 0xc46fb333, 0x441cc000, "Account");
            account->addValue(0, e->mCurrentUserAccount);   // [PC] no device accounts: the stored name
            account->mValue = 0;
            account->mDisabled = true;
            // the player statistics (CEngine::loadStats + processReceiveStats): TODO, phase 5d leftover
            mBackAction = 7;
            break;
        }
        case MENU_INGAME: {
            // (CAchievementManager::save: not ported)
            title(0xc4167333, 0xc2ab999a, T(0x25));
            button(0xc, "ing_save", 0xc41aa000, 0xc2e60000, T(0x26), 0, 0x42700000, 0xc0a00000, 0x40400000)
                ->mDisabled = e->mNetworkMode == 2;
            button(0x70, "ing_surren", 0xc41ae000, 0xc31d4ccd, T(0x27), 0, 0x42480000, 0xc1200000, 0x40400000);
            button(4, "ing_option", 0xc41ae000, 0xc3084ccd, T(9), 0, 0x42480000, 0xc1200000, 0x40400000);
            button(0x20, "ing_back", 0xc419c000, 0xc338cccd, T(6), 0, 0x42480000, 0xc1700000, 0x40400000);
            mBackAction = 0x20;
            break;
        }
        case MENU_OPTIONS: {
            title(0xc38f4000, 0xc2ac0000, T(0x10));
            createElementsOptionsA();
            auto* lvl = button(0x4a, "opt_lvlups", 0xc3c5a666, 0xc377999a, T(0x19d), 0, 0x42c80000, 0xc0e00000,
                               0x40400000);
            if (!e->mGame) {
                button(7, "opt_back", 0xc3938000, 0xc3614ccd, T(6), 0, 0x42200000, 0xc1700000, 0x40a00000);
                mBackAction = 7;
            } else {
                button(8, "opt_back", 0xc3938000, 0xc3614ccd, T(6), 0, 0x42200000, 0xc1700000, 0x40a00000);
                lvl->mDisabled = true;
                mBackAction = 8;
            }
            if (e->mLowResTextures) {
                const Vec3 c(colorG.x * bitsf(0x3f19999a), colorG.y * bitsf(0x3f19999a), colorG.z * bitsf(0x3f19999a));
                addElement(new CInterfaceElementText(this, bitsf(0xc38b8000), bitsf(0xc2c66666),
                                                     "Attention: Low resolution mode", 1, 3.25f, c));
            }
            break;
        }
        case MENU_OPTIONS2:
            title(0xc38f4000, 0xc2ac0000, T(0x264));
            createElementsOptionsB();
            button(4, "opt_back", 0xc3938000, 0xc3614ccd, T(0x265), 0, 0x42200000, 0xc1700000, 0x40a00000);
            mBackAction = 4;
            break;
        case MENU_SAVEGAME:
            title(0xc4147333, 0x43826666, T(0x28));
            button(8, "save_back", 0xc405f333, 0x43068000, T(6), 2, 0x42480000, 0x41700000, 0x40400000);
            mBackAction = 8;
            createElementsLoadSave(bitsf(0xc4180000), bitsf(0x436a0000), e->mNetworkMode == 1 ? "multi" : "single",
                                   false);
            break;
        case MENU_LOADGAME:
            title(0x43f2c000, 0x43a90000, T(0x15));
            if (mCurrentMenu == MENU_SINGLEPLAYER) {
                button(1, "load_back", 0x4407a000, 0x4353cccd, T(6), 2, 0x42480000, 0x41a00000, 0x40a00000);
                mBackAction = 1;
                createElementsLoadSave(bitsf(0x43e88000), bitsf(0x439b8000), "single", true);
            } else {
                button(0x33, "load_back", 0x4407a000, 0x4353cccd, T(6), 2, 0x42480000, 0x41a00000, 0x40a00000);
                mBackAction = 0x33;
                createElementsLoadSave(bitsf(0x43e88000), bitsf(0x439b8000), "multi", true);
            }
            break;
        case MENU_TUTORIAL: {
            title(0xc2013333, 0x44185333, T(0x22));
            // the installed tutorial map's "set revision"
            int revision = 0;
            const std::string file = "map201" + e->mLanguage + ".txt";
            std::string text;
            CUtils::log("try map %s", file.c_str());
            if (CMemoryFile::readAll(file, text)) {
                size_t pos = 0;
                while (pos < text.size()) {
                    size_t end = text.find('\n', pos);
                    if (end == std::string::npos) end = text.size();
                    const std::string line = text.substr(pos, end - pos);
                    char w0[64] = "", w1[64] = "";
                    if (std::sscanf(line.c_str(), "%63s %63s", w0, w1) >= 1 && std::strcmp(w0, "set") == 0 &&
                        std::strcmp(w1, "revision") == 0)
                        std::sscanf(line.c_str(), "%*s %*s %i", &revision);
                    pos = end + 1;
                }
            }
            const bool ok = e->isMapDownloaded(0xc9) &&
                            static_cast<int>(CConfig::get().getUInt("CurrentMapRevision201", 0)) == revision;
            addElement(new CInterfaceElementButton2(this, ok ? 0xe : 0x2e, "tut_start", kStartMesh, bitsf(0xc2740000),
                                                    bitsf(0x44115333), T(ok ? 0x23 : 0x9e), 1, false, 5.0f,
                                                    white(), 60.0f, 12.0f, bitsf(0x3fe66666), bitsf(0x40900000)));
            createElementDifficultySlider(bitsf(0xc18a6666), bitsf(0x440c3333), "tut_diff");
            addElement(new CInterfaceElementButton2(this, 1, "tut_back", bitsf(0xc2540000), bitsf(0x43fe6666), T(6), 1,
                                                    false, 5.0f, white(), 50.0f, 12.0f, bitsf(0xbe99999a),
                                                    bitsf(0x4099999a)));
            mBackAction = 1;
            break;
        }
        case MENU_CUSTOMGAME: {
            title(0x43cdc000, 0x4416b333, T(0x16));
            mStartButton = addElement(new CInterfaceElementButton2(
                this, 0xd, "spc_start", kStartMesh, bitsf(0x43a90000), bitsf(0x440fc000), T(0x17), 0, false, 5.0f,
                startTextColor(), 60.0f, 12.0f, bitsf(0xc12ccccd), 4.0f));
            createElementMapSlider("spc_mapdat", bitsf(0x43c20000), bitsf(0x440b0000), -2.0f, -2.0f, "spcustomma",
                                   false);
            createElementsFOWSelector("spc_fog", bitsf(0x439fc000), bitsf(0x43ec4000), 0, false);
            createElementDifficultySlider(bitsf(0x43bbc000), bitsf(0x4405f333), "spc_diff");
            createElementCrateSelector("spc_crates", bitsf(0x43a8c000), bitsf(0x440b9333), 1, false);
            createElementsSectorSettings("spc_sec", bitsf(0x439f6666), bitsf(0x44153333), 0, false);
            createElementsUnitSettings("spc_units", bitsf(0x439f199a), bitsf(0x44194000), 0, false);
            createElementsGameTypeSettings("spc_type", bitsf(0x43a32666), bitsf(0x43f5e666), 0, false);
            createElementsPlayerSettings("spc_player", bitsf(0x43bf0000), bitsf(0x43f50000), 1, false);
            button(0x29, "spc_mapinf", 0x43ca0000, 0x44120000, T(0xa3), 0, 0x42480000, 0xc0f9999a, 0x40733333);
            createElementsGameSettings("spc_tbuild", bitsf(0x43a80000), bitsf(0x4400d333), 0, false);
            button(1, "spc_back", 0x43ba4000, 0x43e10000, T(6), 2, 0x42200000, 0x41200000, 0x40400000);
            mBackAction = 1;
            break;
        }
        case MENU_STATISTICS:
            createElementStatistics(bitsf(0xc44f7333), bitsf(0xc3a36666));
            title(0xc44eb333, 0xc3978000, T(0x37));
            addElement(new CInterfaceElementButton2(this, 8, "stat_main", bitsf(0xc44eb333), bitsf(0xc3df0000), T(0x2b),
                                                    1, false, 5.0f, white(), 50.0f, 12.0f, 0.0f, 5.0f));
            addElement(new CInterfaceElementButton2(this, 0x16, "stat_back", bitsf(0xc44e8000), bitsf(0xc3e6e666),
                                                    T(6), 1, false, 5.0f, white(), 50.0f, 12.0f, 0.0f, 0.0f));
            mBackAction = 0x16;
            break;
        case MENU_AFTERMATH: {
            e->pauseGame();
            createElementStatistics(bitsf(0xc44f7333), bitsf(0xc3a36666));
            CPlayer* local = e->mGame ? e->mGame->getPlayer(e->mLocalPlayer) : nullptr;
            auto statButton = [&](int act, const char* mesh, uint32_t x, uint32_t y, int text) {
                addElement(new CInterfaceElementButton2(this, act, mesh, bitsf(x), bitsf(y), T(text), 1, false, 5.0f,
                                                        white(), 50.0f, 12.0f, 0.0f, 2.0f));
            };
            const Vec3& red = colors::COL_RED;
            const Vec3& green = colors::COL_GREEN;
            if (local && !local->mLost && !local->mWon) {   // neither: a draw
                addElement(new CInterfaceElementText(this, bitsf(0xc44eb333), bitsf(0xc3978000), T(0x259), 1, 5.5f,
                                                     Vec3(red.x * 0.5f + 0.5f, red.y * 0.5f + 0.5f,
                                                          red.z * 0.5f + 0.5f)));
                if (e->mGameType == 1) statButton(0x53, "stat_back", 0xc44ef333, 0xc3e66666, 0x1ae);
                statButton(0x17, "stat_main", 0xc44f9333, 0xc3df0000, 0x2b);
                mBackAction = 0x17;
            } else if (local && !local->mLost) {   // victory (+0x8d set, +0x8c clear)
                addElement(new CInterfaceElementText(this, bitsf(0xc44eb333), bitsf(0xc3978000), T(0xf3), 1, 5.5f,
                                                     Vec3(green.x * 0.5f + 0.5f, green.y * 0.5f + 0.5f,
                                                          green.z * 0.5f + 0.5f)));
                if (e->mGameType != 1) {
                    statButton(0x17, "stat_main", 0xc44f9333, 0xc3df0000, 0x2b);
                    mBackAction = 0x17;
                } else {
                    // The original's "mission complete" hook (Ghidra 0x1ba548): a
                    // won campaign mission unlocks the next episode and makes it
                    // current; "Continue" (0x53) then starts it at once. After the
                    // last episode "Continue" (0x54) rolls the credits instead.
                    CCampaign* cmp = e->getCurCampaign();
                    const bool last = cmp && cmp->markMapCompleted(e->mMapId);
                    if (!last) {
                        statButton(0x53, "stat_back", 0xc44ef333, 0xc3e66666, 0x1b7);
                        statButton(0x17, "stat_main", 0xc44f9333, 0xc3df0000, 0x2b);
                        mBackAction = 0x17;
                    } else {
                        statButton(0x54, "stat_back", 0xc44ef333, 0xc3e66666, 0x1b7);
                        mBackAction = 0x54;
                    }
                }
            } else {   // defeat (also without a game)
                addElement(new CInterfaceElementText(this, bitsf(0xc44eb333), bitsf(0xc3978000), T(0xf2), 1, 5.5f,
                                                     Vec3(red.x * 0.5f + 0.5f, red.y * 0.5f + 0.5f,
                                                          red.z * 0.5f + 0.5f)));
                if (e->mGameType == 1) statButton(0x53, "stat_back", 0xc44ef333, 0xc3e66666, 0x1ae);
                statButton(0x17, "stat_main", 0xc44f9333, 0xc3df0000, 0x2b);
                mBackAction = 0x17;
            }
            // the online statistics upload (lobby) and killGameClient/killLocalServer: not ported
            break;
        }
        case MENU_DOWNLOAD: {
            title(0x44300000, 0x44160000, T(0x93));
            char line[300];
            std::snprintf(line, sizeof line, "%s [%s]", T(0x106), mDownloadMapName.c_str());
            mDownloadTitle = addElement(
                new CInterfaceElementText(this, bitsf(0x44300000), bitsf(0x44084000), line, 1, 5.0f, white()));
            mDownloadStatus = addElement(
                new CInterfaceElementText(this, bitsf(0x44300000), bitsf(0x44048000), T(0x94), 1, 4.0f, white()));
            mDownloadBar = addElement(new CInterfaceElementValueBar(this, bitsf(0x44300000), bitsf(0x440e8000), 100.0f,
                                                                    bitsf(0x40700000), "", colorG, colorG, 1, 1,
                                                                    bitsf(0x40700000), white()));
            mDownloadBar->setValuePercent(0.0f);
            // [PC] there is nothing to download: Back is always offered
            addElement(new CInterfaceElementButton2(this, 7, "", bitsf(0x44300000), bitsf(0x441b0000), T(6), 1, false,
                                                    5.0f, white(), 50.0f, 12.0f, 0.0f, 2.0f));
            mDownloadInProgress = false;
            break;
        }
        case MENU_CREDITS:
            title(0xc3754ccd, 0x43b5c000, T(0x92));
            addElement(new CInterfaceElementImage(this, bitsf(0xc3790000), bitsf(0x438a0000), 20.0f, 20.0f,
                                                  "credits_im", mCreditsBlack));
            button(7, "cred_back", 0xc380c000, 0x43400000, T(6), 0, 0x42200000, 0xc1700000, 0x40a00000);
            mBackAction = 7;
            break;
        case MENU_SPLITSCREEN: {
            title(0x4421b333, 0x42f16666, T(0xe1));
            mStartButton = addElement(new CInterfaceElementButton2(
                this, 0x3a, "split_star", kStartMesh, bitsf(0x44352000), 28.0f, T(0x17), 2, false, 5.0f,
                startTextColor(), 50.0f, 12.0f, 7.0f, bitsf(0x40733333)));
            createElementMapSlider("split_mapd", bitsf(0x4426c000), 95.0f, -2.0f, -6.0f, "splitmap", false);
            createElementsFOWSelector("split_fog", bitsf(0x44197333), bitsf(0x429b999a), 0, false);
            createElementsGameSettings("split_tbui", bitsf(0x44257333), 20.0f, 2, false);
            createElementsSectorSettings("split_sect", bitsf(0x44171333), bitsf(0x42ce3333), 0, false);
            createElementsUnitSettings("split_unit", bitsf(0x44176000), bitsf(0x4266cccd), 0, false);
            createElementsPlayerSettings("split_play", bitsf(0x44189333), 0.0f, 2, false);
            e->mCratesEnabled = false;
            createElementDifficultySlider(bitsf(0x44259333), bitsf(0x42193333), "split_crat");
            addElement(new CInterfaceElementButton2(this, 0x29, "split_minf", bitsf(0x44318000), bitsf(0x42ff999a),
                                                    T(0xa3), 1, false, 5.0f, white(), 50.0f, 12.0f, bitsf(0x40133333),
                                                    bitsf(0x40533333)));
            button(2, "split_back", 0x4419a000, 0xc2480000, T(6), 0, 0x42200000, 0xc1200000, 0x40800000);
            button(0xb, "split_load", 0x44341333, 0xc2440000, T(0x14), 2, 0x42480000, 0x41200000, 0x40a00000);
            mBackAction = 2;
            break;
        }
        case MENU_HOSTSERVER: {
            e->mUnitAvailabilityMode = 0;
            title(0x442de000, 0xc32b4ccd, T(0xe2));
            mStartButton = addElement(new CInterfaceElementButton2(
                this, 0x3b, "host_start", kStartMesh, bitsf(0x44359333), bitsf(0xc3893333), T(0xe2), 0, false, 5.0f,
                startTextColor(), 50.0f, 12.0f, 5.0f, bitsf(0x40733333)));
            addElement(new CInterfaceElementText(this, bitsf(0x442b7333), bitsf(0xc39de666), T(0xe3), 1,
                                                 bitsf(0x40700000), colors::COL_RED));
            addElement(new CInterfaceElementText(this, bitsf(0x442b7333), bitsf(0xc3a0a666), T(0xe4), 1,
                                                 bitsf(0x40700000), colors::COL_RED));
            createElementMapSlider("host_mapda", bitsf(0x44348000), -208.0f, -1.0f, -5.0f, "hostmap", false);
            createElementsFOWSelector("host_fog", bitsf(0x4426a000), bitsf(0xc365e666), 0, false);
            createElementsGameSettings("host_tbuil", bitsf(0x4426d333), bitsf(0xc392a666), 0, false);
            createElementCrateSelector("host_crate", bitsf(0x4425c000), bitsf(0xc387a666), 0, false);
            createElementsSectorSettings("host_secto", bitsf(0x44262000), bitsf(0xc340199a), 0, false);
            createElementsGameTypeSettings("host_units", bitsf(0x44256000), bitsf(0xc37be666), 0, false);
            button(0x29, "host_mapin", 0x4443d333, 0xc333999a, T(0xa3), 2, 0x42480000, 0x41380000, 0x40733333);
            button(2, "host_back", 0x442b6000, 0xc3a7c000, T(6), 0, 0x42200000, 0xc1200000, 0x40800000);
            auto* pub = addElement(new CInterfaceElementTextSlider2(
                this, 0x2b, "host_publi", bitsf(0x4441e000), bitsf(0xc3926666), T(0xe5), 2, 4.0f, white(), 100.0f,
                20.0f, 0.0f, -8.0f, 50.0f, 20.0f, 5.0f, 2.0f));
            pub->addValue(0, CTxtLoc::get().getS(0x254));
            pub->addValue(1, CTxtLoc::get().getS(0x253));
            pub->mValue = 0;               // the lobby is not connected: not published, disabled
            e->mPublishServer = false;
            pub->mDisabled = true;
            auto* name = addElement(new CInterfaceElementTextInput2(
                this, 0x6c, "host_serve", bitsf(0x44385333), bitsf(0xc327999a), true, bitsf(0x443c5333),
                bitsf(0xc329999a), "Server:", 0, false, 4.0f, white(), 100.0f, 12.0f, -10.0f, 4.0f));
            name->mValue = e->mServerName;
            name->mMaxLength = 0x14;
            name->mValueOffX = 40.0f;
            name->mClearOnTap = false;
            mBackAction = 2;
            break;
        }
        case MENU_ENTERIP: {
            title(0x42640000, 0xc3810000, T(0xe6));
            addElement(new CInterfaceElementTextInput2(this, 0x39, "ip_input", 0.0f, -300.0f, false, 0.0f, 0.0f,
                                                       "IP: >", 0, false, 4.0f, white(), 40.0f, 12.0f, -10.0f, 4.0f));
            button(2, "ip_back", 0x42480000, 0xc3ae0000, T(6), 0, 0x42200000, 0xc1200000, 0x40800000);
            mBackAction = 2;
            break;
        }
        case MENU_SERVERLIST: {
            if (mLanServerList == 1) {
                addElement(new CInterfaceElementText(this, bitsf(0x43c12666), bitsf(0xc36ce666), T(0xe7), 1, 5.5f,
                                                     Vec3(colorG.y, colorG.z, colorG.x)));
            } else {
                title(0x43c12666, 0xc36ce666, T(0xe8));
                auto* r = addElement(new CInterfaceElementTextSlider2(
                    this, 0x68, "pub_ranked", bitsf(0x43e26666), bitsf(0xc36b4ccd), T(0x251), 0, 4.0f, white(), 100.0f,
                    20.0f, 0.0f, -8.0f, 50.0f, 20.0f, 5.0f, 2.0f));
                r->addValue(0, CTxtLoc::get().getS(0x229));
                r->addValue(1, CTxtLoc::get().getS(0x252));
                r->mValue = 1;
            }
            // CInterfaceElementServerList (Ghidra 0x199374...): the lobby's list - TODO, networking phase
            addElement(new CInterfaceElementButton2(this, 0x3e, "pub_connec", bitsf(0x43f3a666), bitsf(0xc3bf2666),
                                                    T(0xea), 2, false, 5.0f, white(), 80.0f, 12.0f, bitsf(0x40e9999a),
                                                    3.0f))
                ->mDisabled = true;
            addElement(new CInterfaceElementButton2(this, action, "pub_back", bitsf(0x43af4000), -382.0f, T(6), 0,
                                                    false, 5.0f, white(), 40.0f, 12.0f, bitsf(0xc1b26666), 4.0f));
            mBackAction = action;
            break;
        }
        case MENU_WAITINGROOM: {
            title(0x43ab6666, 0xc3f5c000, T(0xec));
            const std::string server = "Server: " + e->mServerName;
            addElement(new CInterfaceElementText(this, bitsf(0x43d7a666), bitsf(0xc3f5c000), server, 1, 4.0f, white()));
            addElement(new CInterfaceElementText(this, bitsf(0x43e22666), bitsf(0xc3fde666), T(0x22a), 1, 4.0f,
                                                 white()));   // ranked (+0x302) ? 0x229 : 0x22a
            createElementMapSliderFixed("wait_mapda", bitsf(0x43c08000), bitsf(0xc4073333), -3.0f, -6.0f, "waitmap",
                                        e->mMapId);
            createElementsSectorSettings("wait_secto", bitsf(0x439fe666), bitsf(0xc3fde666), 0, true);
            createElementsFOWSelector("wait_fog", bitsf(0x43bbe666), bitsf(0xc3fde666), 0, true);
            createElementsGameTypeSettings("wait_units", bitsf(0x43a50000), bitsf(0xc403cccd), 0, true);
            createElementCrateSelector("wait_crate", bitsf(0x439ae666), bitsf(0xc4087333), 0, true);
            createElementsGameSettings("wait_tbuil", bitsf(0x43a3c000), bitsf(0xc40ca000), 0, true);
            createElementsPlayerSettingsNetworked("wait_playe", bitsf(0x4395a666), -595.0f, e->mNumPlayers, false);
            // CInterfaceElementMultiTextScrollable (the chat) - TODO, networking phase
            auto* chat = addElement(new CInterfaceElementTextInput2(
                this, 0x41, "wait_input", bitsf(0x439b6666), -670.0f, true, -710.0f, -590.0f, T(0x262), 0, false,
                bitsf(0x4019999a), white(), 40.0f, 12.0f, -10.0f, 4.0f));
            chat->mMaxLength = 0xfa;
            mReadyButton = addElement(new CInterfaceElementButton2(
                this, 0x3f, "wait_ready", kStartMesh, bitsf(0x43a3a666), bitsf(0xc4112000),
                T(e->mPlayerSetup[e->mLocalPlayer].ready == 0 ? 0x108 : 0x109), 0, false, 5.0f, white(), 40.0f, 12.0f,
                -10.0f, 4.0f));
            button(0x3c, "wait_back", 0x43b24000, 0xc4295333, T(6), 0, 0x42200000, 0xc1200000, 0x40800000);
            mBackAction = 0x3c;
            break;
        }
        case MENU_UPDATENEWS: {
            title(0xc3e60000, 0x4418e000, T(0xe0));
            auto* news = addElement(
                new CInterfaceElementMultiText(this, bitsf(0xc40ac000), bitsf(0x4414c000), " ", 0, 3.5f, white()));
            news->mLineSpacing = bitsf(0x3fcccccd);
            news->mMaxLines = 0x19;
            news->mNewestLast = false;
            char head[300];
            std::snprintf(head, sizeof head, T(0xdd), CConfig::get().getString("Revision", "").c_str(),
                          CConfig::get().getString("Version", "").c_str(), "Nov  6 2015");   // __DATE__ of the build
            const Vec3& blue = colors::COL_BLUE;
            news->addLine(head, Vec3(blue.x * 0.5f + 0.5f, blue.y * 0.5f + 0.5f, blue.z * 0.5f + 0.5f));
            // update.txt: one line per token; a leading '!' (red), '#' (white),
            // '*' (orange, lightened) or '+' (green) changes the colour of this
            // and the following lines (the marker itself stays in the text)
            std::string text;
            if (CMemoryFile::readAll("update.txt", text)) {
                Vec3 c = colors::COL_WHITE;
                size_t pos = 0;
                while (pos < text.size()) {
                    size_t end = text.find('\n', pos);
                    if (end == std::string::npos) end = text.size();
                    const std::string line = text.substr(pos, end - pos);
                    pos = end + 1;
                    if (line.empty()) continue;
                    switch (line[0]) {
                        case '!': c = colors::COL_RED; break;
                        case '#': c = colors::COL_WHITE; break;
                        case '*': {
                            const Vec3& o = colors::COL_ORANGE;
                            c = Vec3(o.x * 0.5f + 0.5f, o.y * 0.5f + 0.5f, o.z * 0.5f + 0.5f);
                            break;
                        }
                        case '+': c = colors::COL_GREEN; break;
                        default: break;
                    }
                    news->addLine(line, c);
                }
            }
            // any purchased map (0..39, 201..209) whose installed script is
            // older than config's CurrentMapRevision<id> (or missing) -> the
            // Back button becomes "Download" (action 0x75). The campaign
            // (id 1001, CCampaign::isOutdated) is not ported.
            CUtils::log("MENUTYPE_UPDATEINFO: search maps...");
            bool outdated = false;
            for (unsigned id = 0; id < 0x44b; ++id) {
                if (!(id <= 0x27 || (id >= 0xc9 && id <= 0xd1))) continue;
                const unsigned rev = CConfig::get().getUInt("CurrentMapRevision" + std::to_string(id), 0);
                if (static_cast<int>(rev) <= 0 || !e->isMapPurchased(id)) continue;
                if (!e->isMapDownloaded(id)) {
                    outdated = true;
                    continue;
                }
                char file[300];
                std::snprintf(file, sizeof file, "map%u%s.txt", id, e->mLanguage.c_str());
                unsigned have = 0;
                std::string map;
                if (CMemoryFile::readAll(file, map)) {
                    CUtils::log("MENUTYPE_UPDATEINFO: parse map %s", file);
                    size_t p = 0;
                    while (p < map.size()) {
                        size_t q = map.find('\n', p);
                        if (q == std::string::npos) q = map.size();
                        const std::string line = map.substr(p, q - p);
                        p = q + 1;
                        char w0[64] = "", w1[64] = "";
                        if (std::sscanf(line.c_str(), "%63s %63s", w0, w1) == 2 && std::strcmp(w0, "set") == 0 &&
                            std::strcmp(w1, "revision") == 0)
                            std::sscanf(line.c_str(), "%*s %*s %u", &have);
                    }
                }
                if (rev > have) outdated = true;
            }
            CUtils::log("MENUTYPE_UPDATEINFO: check maps done");
            if (outdated) {
                addElement(new CInterfaceElementButton2(this, 0x75, "upd_back", bitsf(0xc3e60000), bitsf(0x43e92666),
                                                        T(0x106), 1, false, 5.0f, white(), 40.0f, 12.0f,
                                                        bitsf(0xbf4ccccd), 3.5f));
                mBackAction = 0x75;
            } else {
                addElement(new CInterfaceElementButton2(this, 7, "upd_back", bitsf(0xc3e60000), bitsf(0x43e92666),
                                                        T(6), 1, false, 5.0f, white(), 40.0f, 12.0f,
                                                        bitsf(0xbf4ccccd), 3.5f));
                mBackAction = 7;
            }
            break;
        }
        case MENU_SURRENDER: {
            // "Do you really want to surrender?" [!] Only a network game
            // surrenders (action 0x6f: cmdPlayerSurrenders + lost); otherwise
            // the button is action 0x21, which only shows the aftermath screen,
            // and since nobody has won or lost it reads "Draw".
            title(0x42640000, 0xc3810000, T(0x27));
            addElement(new CInterfaceElementText(this, bitsf(0xc04ccccd), bitsf(0xc38b6666), T(0x25e), 0, 4.0f,
                                                 colors::COL_GRAY75));
            addElement(new CInterfaceElementButton2(this, e->mNetworkMode == 2 ? 0x6f : 0x21, "ip_input",
                                                    bitsf(0xbfc00000), bitsf(0xc3954000), T(0x25f), 0, false, 5.0f,
                                                    white(), 100.0f, 12.0f, -10.0f, 3.0f));
            button(8, "ip_back", 0x42480000, 0xc3ae0000, T(6), 0, 0x42200000, 0xc1200000, 0x40800000);
            mBackAction = 8;
            break;
        }
        case MENU_CAMPAIGN: {
            // the campaign choice (screen 25): the slider of campaigns, Start /
            // Continue (0x50), Replay episode (0x4e -> screen 26), the difficulty
            e->mGameType = 1;
            title(0x43370000, 0x445b1333, T(0x1b0));
            addElement(new CInterfaceElementButton2(this, 1, "cmp_back", bitsf(0x436ccccd), 739.0f, T(6), 1, false,
                                                    5.0f, white(), 60.0f, 12.0f, bitsf(0xbf4ccccd), 3.5f));
            mCampaignStartButton = addElement(new CInterfaceElementButton2(
                this, 0x50, "cmp_start", bitsf(0x43148000), bitsf(0x443d6000), T(0x1af), 1, false, 5.0f, white(), 60.0f,
                12.0f, bitsf(0xbf4ccccd), 3.5f));
            mCampaignReplayButton = addElement(new CInterfaceElementButton2(
                this, 0x4e, "cmp_reply", bitsf(0x4319cccd), bitsf(0x4438b333), T(0x1ae), 1, false, 5.0f, white(),
                60.0f, 12.0f, bitsf(0xbf4ccccd), 3.5f));
            createElementCampaignSlider("campaignic", bitsf(0x430c0000), bitsf(0x44548000), "campaignic",
                                        e->mCurrentCampaignId);
            createElementDifficultySlider(bitsf(0x4388a666), bitsf(0x443ee000), "cmp_diff");
            mBackAction = 1;
            break;
        }
        case MENU_CAMPAIGNPROGRESS: {
            // the episode list (screen 26): every unlocked episode, pick one and
            // replay it (0x52); Back (0x4d) re-reads the campaign
            title(0xc3848000, 0x445ac000, T(0x1b1));
            CCampaign* cmp = e->mCampaigns[e->mCurrentCampaignId].get();
            if (cmp) {
                addElement(new CInterfaceElementText(this, bitsf(0xc3848000), bitsf(0x4455c000), cmp->mName, 1, 5.0f,
                                                     colors::COL_GRAY75));
                addElement(new CInterfaceElementCampaignButtons(this, 0x51, bitsf(0xc3960000), bitsf(0x44520000), cmp,
                                                                5.0f, colors::COL_GRAY75));
            }
            addElement(new CInterfaceElementButton2(this, 0x52, "cmpe_rerun", bitsf(0xc3848000), bitsf(0x443cb333),
                                                    T(0x1b2), 2, false, 5.0f, white(), 60.0f, 12.0f,
                                                    bitsf(0xbf4ccccd), 3.5f));
            addElement(new CInterfaceElementButton2(this, 0x4d, "cmpe_bk", bitsf(0xc3520000), bitsf(0x443cb99a), T(6),
                                                    0, false, 5.0f, white(), 60.0f, 12.0f, bitsf(0xbf4ccccd), 3.5f));
            mBackAction = 0x4d;
            break;
        }
        default: {
            // [PC] [STAND-IN] a screen that is not ported yet: its number and a way back
            const CVector2 c = screenCenter(menuType);
            char t[64];
            std::snprintf(t, sizeof t, "SCREEN %d", menuType);
            addElement(new CInterfaceElementText(this, c.x, c.y + 40.0f, t, 1, 5.5f, colorG));
            addElement(new CInterfaceElementText(this, c.x, c.y + 20.0f, "(not ported yet)", 1, 4.0f, white()));
            addElement(new CInterfaceElementButton2(this, 7, "", c.x, c.y - 40.0f, T(6), 1, true, 5.0f, white(), 0.0f,
                                                    0.0f, 0.0f, -5.0f));
            mBackAction = 7;
            break;
        }
    }
    mCurrentMenu = menuType;
}

// ---------------------------------------------------------------------------
// Builders

// Ghidra 0x1d2c58: the map slider of the custom game, split screen and host
// screens. Maps 1..19 whose script map<id><Language>.txt exists, in id order;
// each script's "set mapname/mapsize/numplayers/revision" lines.
void CInterfaceMenu::createElementMapSlider(const std::string& /*name*/, float x, float y, float dx, float dy,
                                            const std::string& subMesh, bool disabled) {
    CEngine* e = mEngine;
    auto* s = new CInterfaceElementMapImageSlider(this, 0xf, x, y, x + dx, y + dy, x + 20.0f, y + 13.0f, x + 22.0f,
                                                  y - 45.0f, x + 28.0f, y - 53.0f, x + 28.0f, y - 16.0f, 53.0f, 41.0f,
                                                  T(0x18), subMesh, 5.0f, white(), 80.0f, 80.0f, -3.0f, 20.0f);
    mMapSlider = s;
    CUtils::log("search maps...");
    unsigned numPlayers = 0;   // [!] not reset between maps: a script without the line keeps the previous count
    for (unsigned id = 1; id != 0x14; ++id) {
        char file[300];
        std::snprintf(file, sizeof file, "map%u%s.txt", id, e->mLanguage.c_str());
        std::string text;
        if (!CMemoryFile::readAll(file, text)) continue;
        CUtils::log("parse map %s", file);
        std::string mapName, sizeText, playersText;
        unsigned revision = 0;
        size_t pos = 0;
        while (pos < text.size()) {   // boost::tokenizer over '\n' (empty lines skipped)
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            const std::string line = text.substr(pos, end - pos);
            pos = end + 1;
            if (line.empty()) continue;
            char w0[256] = "", w1[256] = "";
            std::sscanf(line.c_str(), "%255s", w0);
            if (std::strcmp(w0, "set") != 0) continue;
            std::sscanf(line.c_str(), "%*s %255s", w1);
            if (std::strcmp(w1, "mapname") == 0 && line.size() > 12) mapName = line.substr(12);   // keeps the '\r'
            if (std::strcmp(w1, "mapsize") == 0) {
                float w = 0.0f, h = 0.0f;
                std::sscanf(line.c_str(), "%*s %*s %f %f", &w, &h);
                const float km2 = (w * h) / bitsf(0x49742400);   // 1e6
                char buf[52];
                std::snprintf(buf, sizeof buf, "%.2f km\xb2", static_cast<double>(km2));
                sizeText = buf;
            }
            if (std::strcmp(w1, "numplayers") == 0) {
                int n = 0;
                std::sscanf(line.c_str(), "%*s %*s %i", &n);
                numPlayers = static_cast<unsigned>(n);
                char buf[52];
                std::snprintf(buf, sizeof buf, "%i %s", n, T(0x21));
                playersText = buf;
            }
            if (std::strcmp(w1, "revision") == 0) {
                int r = 0;
                std::sscanf(line.c_str(), "%*s %*s %i", &r);
                revision = static_cast<unsigned>(r);
            }
        }
        const unsigned current = CConfig::get().getUInt("CurrentMapRevision" + std::to_string(id), 0);
        s->addEntry(id, mapName, playersText, sizeText, nullptr, false, false, numPlayers, revision, current, "", "");
    }
    CUtils::log("check maps done");
    if (e->mMapId > 0x14) e->mMapId = 2;
    s->mCurrentId = e->mMapId;
    char icon[64];
    std::snprintf(icon, sizeof icon, "map%uicon.png", s->mCurrentId);
    s->changeTexture(icon);
    CInterfaceElementImageSlider::CEntry& en = s->current();
    mMapOutdated = en.currentRevision > en.revision;   // +0x5fc
    en.purchased = e->isMapPurchased(s->mCurrentId);
    en.downloaded = mMapOutdated ? false : e->isMapDownloaded(s->mCurrentId);
    addElement(s);
    processAction(0xf, s);
    e->mNumPlayers = s->current().numPlayers;
    if (mPlayerControl) mPlayerControl->setNumPlayers(s->current().numPlayers);
    s->mDisabled = disabled;
}

// Ghidra 0x1d1de8: the waiting room's map (one entry, not changeable).
// [PC] layout only: the same slider with just the room's map, disabled.
void CInterfaceMenu::createElementMapSliderFixed(const std::string& name, float x, float y, float dx, float dy,
                                                 const std::string& subMesh, unsigned mapId) {
    createElementMapSlider(name, x, y, dx, dy, subMesh, true);
    if (mMapSlider) {
        mMapSlider->mCurrentId = mapId;
        char icon[64];
        std::snprintf(icon, sizeof icon, "map%uicon.png", mapId);
        mMapSlider->changeTexture(icon);
    }
}

namespace {
// The option sliders of the game setup (Ghidra 0x1b70f8..0x1ba02c): label
// size 4, 110 x 20 (p13/p14), value offset (+-5 or +-2, -8 or -9), hit
// 50 x 20 at (0, 2/3/4). Align 2 puts the value to the left of the label.
CInterfaceElementTextSlider2* setupSlider(CInterfaceMenu* m, int action, const std::string& mesh, float x, float y,
                                          const char* label, int align, float valueOffX, float valueOffY,
                                          float hitOffY) {
    return m->addElement(new CInterfaceElementTextSlider2(m, action, mesh, x, y, label, align, 4.0f, colors::COL_WHITE,
                                                          110.0f, 20.0f, valueOffX, valueOffY, 50.0f, 20.0f, 0.0f,
                                                          hitOffY));
}
}  // namespace

// Ghidra 0x1b70f8
void CInterfaceMenu::createElementsFOWSelector(const std::string& subMesh, float x, float y, int align,
                                               bool disabled) {
    auto* s = setupSlider(this, 0x22, subMesh, x, y, T(0x9b), align, 5.0f, -8.0f, 4.0f);
    s->addValue(0, T(0xc));
    s->addValue(1, T(0xb));
    s->mValue = mEngine->mUseFOW ? 1u : 0u;
    s->mDisabled = disabled || mEngine->mGame != nullptr;
}

// Ghidra 0x1b7454
void CInterfaceMenu::createElementCrateSelector(const std::string& subMesh, float x, float y, int align,
                                               bool disabled) {
    auto* s = setupSlider(this, 0x24, subMesh, x, y, T(0x9c), align, 5.0f, -8.0f, 3.0f);
    s->addValue(0, T(0xc));
    s->addValue(1, T(0xb));
    s->mValue = mEngine->mCratesEnabled ? 1u : 0u;
    s->mDisabled = disabled || mEngine->mGame != nullptr;
}

// Ghidra 0x1b77b8: keys 0 Standard, 2 Shared Teams, 3 Resources
void CInterfaceMenu::createElementsGameTypeSettings(const std::string& subMesh, float x, float y, int align,
                                                    bool disabled) {
    auto* s = setupSlider(this, 0x31, subMesh, x, y, T(0xd2), align == 2 ? 2 : align, align == 2 ? -5.0f : 5.0f,
                          -9.0f, 2.0f);
    s->addValue(0, CTxtLoc::get().getS(0xd3));
    s->addValue(2, CTxtLoc::get().getS(0x26f));
    s->addValue(3, CTxtLoc::get().getS(0x271));
    s->mDisabled = disabled;
    s->mValue = static_cast<unsigned>(mEngine->mGameType);
}

// Ghidra 0x1b7d0c: build time (production speed 0..5)
void CInterfaceMenu::createElementsGameSettings(const std::string& subMesh, float x, float y, int align,
                                                bool disabled) {
    auto* s = setupSlider(this, 0x15, subMesh, x, y, T(0x1a), align == 2 ? 2 : align, align == 2 ? -5.0f : 5.0f,
                          -9.0f, 2.0f);
    for (unsigned k = 0; k < 6; ++k) s->addValue(k, T(0x1b + static_cast<int>(k)));
    s->mDisabled = disabled;
    s->mValue = static_cast<unsigned>(mEngine->mProductionSpeed);
}

// Ghidra 0x1b83d4: start sectors (EGameSectorSetupModes 0..5)
void CInterfaceMenu::createElementsSectorSettings(const std::string& subMesh, float x, float y, int align,
                                                  bool disabled) {
    auto* s = setupSlider(this, 0x30, subMesh, x, y, T(0xde), align == 2 ? 2 : align, align == 2 ? -2.0f : 2.0f,
                          -9.0f, 2.0f);
    for (unsigned k = 0; k < 6; ++k) s->addValue(k, CTxtLoc::get().getS(0xcc + static_cast<int>(k)));
    s->mDisabled = disabled;
    s->mValue = static_cast<unsigned>(mEngine->mSectorSetupMode);
}

// Ghidra 0x1ba02c: available units (EGameUnitAvailabilityModes 0..8)
void CInterfaceMenu::createElementsUnitSettings(const std::string& subMesh, float x, float y, int align,
                                                bool disabled) {
    auto* s = setupSlider(this, 0x2f, subMesh, x, y, T(0xdf), align == 2 ? 2 : align, align == 2 ? -5.0f : 5.0f,
                          -9.0f, 2.0f);
    for (unsigned k = 0; k < 8; ++k) s->addValue(k, CTxtLoc::get().getS(0xd4 + static_cast<int>(k)));
    s->addValue(8, CTxtLoc::get().getS(0x193));
    s->mDisabled = disabled;
    s->mValue = static_cast<unsigned>(mEngine->mUnitAvailabilityMode);
}

// Ghidra 0x1b8a78: difficulty 0..6 (align 2, value at (-5, -8), hit 60 x 20 at (3, 3))
void CInterfaceMenu::createElementDifficultySlider(float x, float y, const std::string& subMesh) {
    auto* s = addElement(new CInterfaceElementTextSlider2(this, 0x10, subMesh, x, y, T(0x39), 2, 4.0f, white(), 110.0f,
                                                          20.0f, -5.0f, -8.0f, 60.0f, 20.0f, 3.0f, 3.0f));
    s->addValue(0, T(0x121));
    for (unsigned k = 1; k <= 5; ++k) s->addValue(k, T(0x39 + static_cast<int>(k)));
    s->addValue(6, T(0x122));
    s->mValue = static_cast<unsigned>(mEngine->mDifficulty);
}

// Ghidra 0x1b6e90
void CInterfaceMenu::createElementsPlayerSettings(const std::string& /*name*/, float x, float y, unsigned maxHumans,
                                                  bool disabled) {
    auto* pc = new CInterfaceElementPlayerControl(this, 0x32, x, y, maxHumans, false, 4.0f, white());
    mPlayerControl = pc;
    pc->mDisabled = disabled;
    if (mMapSlider) pc->setNumPlayers(mMapSlider->current().numPlayers);
    addElement(pc);
}

// Ghidra 0x1b6d80
void CInterfaceMenu::createElementsPlayerSettingsNetworked(const std::string& /*name*/, float x, float y,
                                                           unsigned maxHumans, bool disabled) {
    auto* pc = new CInterfaceElementPlayerControl(this, 0x32, x, y, maxHumans, true, 4.0f, white());
    mPlayerControl = pc;
    pc->mDisabled = disabled;
    pc->setNumPlayers(mEngine->mNumPlayers);
    addElement(pc);
}

// Ghidra 0x1b18a4 (options screen 5)
void CInterfaceMenu::createElementsOptionsA() {
    CEngine* e = mEngine;
    auto* lang = addElement(new CInterfaceElementTextSlider2(this, 0x1f, "opt_langua", bitsf(0xc3a1a666),
                                                             bitsf(0xc2d6999a), T(0), 0, 4.0f, white(), 110.0f, 20.0f,
                                                             5.0f, -8.0f, 50.0f, 22.0f, -5.0f, 3.0f));
    lang->addValue(0, "English");
    lang->addValue(1, "Deutsch");
    if (e->mLanguage == "English") lang->mValue = 0;
    if (e->mLanguage == "Deutsch") lang->mValue = 1;
    lang->mDisabled = e->mGame != nullptr;
    auto* gui = addElement(new CInterfaceElementTextSlider2(this, 0x28, "opt_guisca", bitsf(0xc3778000),
                                                            bitsf(0xc3124ccd), T(0xa2), 2, 4.0f, white(), 50.0f, 20.0f,
                                                            -5.0f, -8.0f, 50.0f, 20.0f, 5.0f, 4.0f));
    static const char* kScales[7] = {"0.25x", "0.5x", "0.75x", "1.0x", "1.5x", "2.0x", "3.0x"};
    for (unsigned k = 0; k < 7; ++k) gui->addValue(k, kScales[k]);
    gui->mValue = e->mGUIScalingOption;
    addElement(new CInterfaceElementButton2(this, 5, "opt_eventv", bitsf(0xc3b94000), bitsf(0xc3124ccd), T(0x264), 0,
                                            false, 5.0f, white(), 75.0f, 20.0f, -5.0f, 4.0f));
    auto* speed = addElement(new CInterfaceElementTextSlider2(this, 0x14, "opt_gamesp", bitsf(0xc3a24000), -164.0f,
                                                              T(3), 0, 4.0f, white(), 110.0f, 20.0f, 5.0f, -8.0f, 80.0f,
                                                              20.0f, -5.0f, 4.0f));
    speed->addValue(1, "1x");
    speed->addValue(2, "2x");
    speed->addValue(5, "5x");
    speed->addValue(10, "10x");
    speed->mValue = static_cast<unsigned>(e->mGameSpeedFactor);   // __fixunssfsi
    speed->mDisabled = e->mNetworkMode == 2;
    auto* offline = addElement(new CInterfaceElementTextSlider2(this, 0x72, "opt_asd", bitsf(0xc3a2e666), -127.0f,
                                                                T(0x261), 0, 4.0f, white(), 110.0f, 20.0f, 5.0f, -8.0f,
                                                                70.0f, 20.0f, 5.0f, 3.0f));
    offline->addValue(0, T(0xc));
    offline->addValue(1, T(0xb));
    offline->mValue = e->mOfflineMode ? 1u : 0u;
    auto volume = [&](CInterfaceElementTextSlider2* s) {
        s->addValue(0, T(0xd));
        static const char* kPct[10] = {"10 %%", "20 %%", "30 %%", "40 %%", "50 %%",
                                       "60 %%", "70 %%", "80 %%", "90 %%", "100 %%"};
        for (unsigned k = 1; k <= 10; ++k) s->addValue(k * 10, kPct[k - 1]);
    };
    auto* sfx = addElement(new CInterfaceElementTextSlider2(this, 0x2a, "opt_sfx", bitsf(0xc3764ccd), -182.0f,
                                                            T(0xaf), 2, 4.0f, white(), 42.0f, 20.0f, -5.0f, -8.0f,
                                                            42.0f, 20.0f, 4.0f, 3.0f));
    volume(sfx);
    sfx->mValue = e->mSFXVolume;
    auto* beginner = addElement(new CInterfaceElementTextSlider2(this, 0x2d, "opt_beginn", bitsf(0xc3abe666), -182.0f,
                                                                 T(0xcb), 0, 4.0f, white(), 48.0f, 20.0f, 5.0f, -8.0f,
                                                                 48.0f, 20.0f, 0.0f, 3.0f));
    beginner->addValue(0, T(0xc));
    beginner->addValue(1, T(0xb));
    beginner->mValue = e->mBeginnerMode ? 1u : 0u;
    auto* minimap = addElement(new CInterfaceElementTextSlider2(this, 0x49, "opt_double", bitsf(0xc369cccd),
                                                                bitsf(0xc2d6999a), T(0x195), 2, 4.0f, white(), 48.0f,
                                                                20.0f, -5.0f, -8.0f, 48.0f, 20.0f, 0.0f, 3.0f));
    minimap->addValue(0, T(0xd));
    minimap->addValue(1, T(0x196));
    minimap->addValue(2, T(0x197));
    minimap->addValue(3, T(0x198));
    minimap->mValue = static_cast<unsigned>(e->mMinimapSize);
    auto* autoUp = addElement(new CInterfaceElementTextSlider2(this, 0x45, "opt_levaut", -308.0f, bitsf(0xc35f199a),
                                                               T(0x194), 2, 4.0f, white(), 48.0f, 20.0f, -5.0f, -8.0f,
                                                               48.0f, 20.0f, 0.0f, 3.0f));
    autoUp->addValue(0, T(0xc));
    autoUp->addValue(1, T(0xb));
    autoUp->mValue = e->mUseAutoUpgrade ? 1u : 0u;
    auto* music = addElement(new CInterfaceElementTextSlider2(this, 0x19, "opt_music", bitsf(0xc3864000),
                                                              bitsf(0xc349cccd), T(5), 2, 4.0f, white(), 110.0f, 20.0f,
                                                              -5.0f, -8.0f, 70.0f, 20.0f, 8.0f, 3.0f));
    volume(music);
    music->mValue = e->mMusicVolume;
}

// Ghidra 0x1b3374 (options screen 6)
void CInterfaceMenu::createElementsOptionsB() {
    CEngine* e = mEngine;
    auto* range = addElement(new CInterfaceElementTextSlider2(this, 0x11, "opt_langua", bitsf(0xc3a2e666),
                                                              bitsf(0xc2d6999a), T(0x1c4), 0, 4.0f, white(), 110.0f,
                                                              20.0f, 5.0f, -8.0f, 50.0f, 22.0f, -5.0f, 3.0f));
    range->addValue(0, T(0xc));
    range->addValue(1, T(0xb));
    range->mValue = e->mDrawCircles ? 1u : 0u;
    auto* chat = addElement(new CInterfaceElementTextSlider2(this, 0x73, "opt_guisca", bitsf(0xc3778000),
                                                             bitsf(0xc3124ccd), T(0x266), 2, 4.0f, white(), 50.0f,
                                                             20.0f, -5.0f, -8.0f, 50.0f, 20.0f, 5.0f, 4.0f));
    chat->addValue(0, T(0xc));
    chat->addValue(1, T(0xb));
    chat->mValue = e->mGlobalChatEnabled ? 1u : 0u;
    auto* scroll = addElement(new CInterfaceElementTextSlider2(this, 0x18, "opt_asd", bitsf(0xc3a2e666), -127.0f, T(4),
                                                               0, 4.0f, white(), 110.0f, 20.0f, 5.0f, -8.0f, 70.0f,
                                                               20.0f, 5.0f, 3.0f));
    static const char* kDist[11] = {"0.005", "0.01", "0.015", "0.02", "0.03", "0.05",
                                    "0.1",   "0.15", "0.2",   "0.25", "0.3"};
    for (unsigned k = 0; k < 11; ++k) scroll->addValue(k, kDist[k]);
    scroll->mValue = e->mTouchMinMovedDistanceID;
    auto* events = addElement(new CInterfaceElementTextSlider2(this, 0x2c, "opt_eventv", bitsf(0xc3b94000),
                                                               bitsf(0xc3124ccd), T(0xb4), 0, 4.0f, white(), 750.0f,
                                                               20.0f, -5.0f, -8.0f, 75.0f, 20.0f, -5.0f, 4.0f));
    for (unsigned k = 0; k < 6; ++k) events->addValue(k, T(0xb5 + static_cast<int>(k)));
    events->mValue = static_cast<unsigned>(e->mEventVerboseness);
    auto* compass = addElement(new CInterfaceElementTextSlider2(this, 0x74, "opt_levaut", -308.0f, bitsf(0xc35f199a),
                                                                T(0x267), 2, 4.0f, white(), 48.0f, 20.0f, -5.0f, -8.0f,
                                                                48.0f, 20.0f, 0.0f, 3.0f));
    compass->addValue(0, T(0xc));
    compass->addValue(1, T(0xb));
    compass->mValue = e->mShowCompass ? 1u : 0u;
}

// Ghidra 0x1b4160: the statistics table (screens 11 and 12). Ten rows
// (labels right-aligned at x - 32, 12 apart from y + 2); one column per
// non-neutral player, 28 apart from x - 30: the name (player colour) and a
// ValueBar per row scaled to the best player's value. A player who lost is
// drawn at half (names) / 0.4 (bars) brightness.
void CInterfaceMenu::createElementStatistics(float x, float y) {
    CEngine* e = mEngine;
    CGame* game = e->mGame;
    unsigned maxDestroyed = 0, maxBuilt = 0, maxLevel = 0, maxSpecOps = 0, maxCrates = 0;
    float maxXP = 0.0f, maxDmgTaken = 0.0f, maxDmgDealt = 0.0f, maxSectorTime = 0.0f, maxScore = 0.0f;
    if (game) {
        for (auto& entry : game->players()) {
            CPlayer* p = entry.second.get();
            if (!p || p->isNeutral()) continue;
            if (maxDestroyed < p->mUnitsDestroyed) maxDestroyed = p->mUnitsDestroyed;
            if (maxBuilt < p->mUnitsBuilt) maxBuilt = p->mUnitsBuilt;
            if (maxLevel < p->mHighestBrainLevel) maxLevel = p->mHighestBrainLevel;
            if (static_cast<float>(p->mXPReceived) > maxXP) maxXP = p->mXPReceived;
            if (static_cast<float>(p->mDamageReceived) > maxDmgTaken) maxDmgTaken = p->mDamageReceived;
            if (static_cast<float>(p->mDamageDone) > maxDmgDealt) maxDmgDealt = p->mDamageDone;
            if (maxSpecOps < p->mSpecOpCount) maxSpecOps = p->mSpecOpCount;
            if (maxCrates < p->mCratesPickedUp) maxCrates = p->mCratesPickedUp;
            if (static_cast<float>(p->mSectorHoldTime) > maxSectorTime) maxSectorTime = p->mSectorHoldTime;
            p->updateScore();   // vtable +0x80
            if (static_cast<float>(p->mScore) > maxScore) maxScore = p->mScore;
        }
    }
    const Vec3& w = colors::COL_WHITE;
    const Vec3 labelColor(w.x * 0.75f, w.y * 0.75f, w.z * 0.75f);
    const float lx = x - 32.0f;
    const float ly = y + 3.0f;
    static const int kLabels[10] = {0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x24e};
    static const float kLabelOff[10] = {1.0f, 13.0f, 25.0f, 37.0f, 49.0f, 61.0f, 73.0f, 85.0f, 97.0f, 109.0f};
    for (int i = 0; i < 10; ++i)
        addElement(new CInterfaceElementText(this, lx, ly - kLabelOff[i], T(kLabels[i]), 2, bitsf(0x40700000),
                                             labelColor));
    if (!game) return;
    float off = -30.0f;
    for (auto& entry : game->players()) {
        CPlayer* p = entry.second.get();
        if (!p || p->isNeutral()) continue;
        Vec3 empty = colors::COL_RED, full = colors::COL_GREEN;
        float k = 1.0f;
        if (p->mLost) {
            empty = Vec3(empty.x * bitsf(0x3ecccccd), empty.y * bitsf(0x3ecccccd), empty.z * bitsf(0x3ecccccd));
            full = Vec3(full.x * bitsf(0x3ecccccd), full.y * bitsf(0x3ecccccd), full.z * bitsf(0x3ecccccd));
            k = 0.5f;
        }
        const float cx = x + off;
        const Vec3 pc = p->mColor;
        addElement(new CInterfaceElementText(this, cx + 12.5f, y + 10.0f, p->mName, 1, 3.5f,
                                             Vec3(k * pc.x, k * pc.y, k * pc.z)));
        const Vec3 tc(k * w.x, k * w.y, k * w.z);
        auto bar = [&](float dy) {
            return addElement(new CInterfaceElementValueBar(this, cx, y - dy, 25.0f, bitsf(0x40700000), "", empty, full,
                                                            0, 0, bitsf(0x40700000), tc));
        };
        bar(0.0f)->setValueInt(static_cast<int>(p->mUnitsDestroyed), 0, static_cast<int>(maxDestroyed));
        bar(12.0f)->setValueInt(static_cast<int>(p->mUnitsBuilt), 0, static_cast<int>(maxBuilt));
        bar(24.0f)->setValueInt(static_cast<int>(p->mHighestBrainLevel), 0, static_cast<int>(maxLevel));
        bar(36.0f)->setValueFloat(p->mXPReceived, 0.0f, maxXP);
        bar(48.0f)->setValueFloat(p->mDamageReceived, 0.0f, maxDmgTaken);
        bar(60.0f)->setValueFloat(p->mDamageDone, 0.0f, maxDmgDealt);
        bar(72.0f)->setValueInt(static_cast<int>(p->mSpecOpCount), 0, static_cast<int>(maxSpecOps));
        bar(84.0f)->setValueInt(static_cast<int>(p->mCratesPickedUp), 0, static_cast<int>(maxCrates));
        bar(96.0f)->setValueFloat(p->mSectorHoldTime, 0.0f, maxSectorTime);
        bar(108.0f)->setValueFloat(p->mScore, 0.0f, maxScore);
        off = off + 28.0f;
    }
}

// Ghidra 0x1bb4f4 / 0x1ba838: the slots <prefix>usersave1..4.txt (+ the
// autosave as slot 0 when loading), buttons <load|save>_1.._5. [PC] Savegames
// are a later phase: the file test is the original's (missing file = empty
// slot), so every slot shows "- Empty Slot -"; a save slot still has its
// button (the save itself logs notPorted).
void CInterfaceMenu::createElementsLoadSave(float x, float y, const std::string& prefix, bool load) {
    mSaveSlot = 0;
    const std::string mesh = load ? "load" : "save";
    const Vec3& w = colors::COL_WHITE;
    createElementLoadSave(x, y, "usersave1.txt", prefix, w, 0x1a, 1, load, mesh + "_1");
    createElementLoadSave(x, y - 19.0f, "usersave2.txt", prefix, w, 0x1b, 2, load, mesh + "_2");
    createElementLoadSave(x, y - 38.0f, "usersave3.txt", prefix, w, 0x1c, 3, load, mesh + "_3");
    createElementLoadSave(x, y - 57.0f, "usersave4.txt", prefix, w, 0x1d, 4, load, mesh + "_4");
    // the autosave is only offered for loading (and always gets its button
    // when valid: the last argument is "load" either way)
    if (load) createElementLoadSave(x, y - 76.0f, "autosave.txt", prefix, w, 0x1e, 0, true, mesh + "_5");
}

namespace {

// fgets(n) and drop one trailing '\n' (a '\r' stays: [!] the map name's)
std::string headerLine(FILE* f, int n) {
    char buf[128] = "";
    if (!std::fgets(buf, n, f)) buf[0] = 0;
    std::string s = buf;
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

}  // namespace

// Ghidra 0x1ba838. The slot's file is <prefix><name> (kept in mSlotFiles
// for the slot's action). Without a file, or with one older than config
// MinLoadableSavegameRevision (line 5), the slot reads "Empty Slot"
// (0xdc). A campaign save (line 7) needs the campaign checked and
// installed, any other its map (line 6) downloaded and purchased; else the
// slot is grey and its first line says CTxtLoc 0x1c3 in red. Buttons: a
// usable save always has one; an empty, old or unusable slot only on the
// save screen (to overwrite it).
void CInterfaceMenu::createElementLoadSave(float x, float y, const std::string& name, const std::string& prefix,
                                           const Vec3& color, int action, int slot, bool load,
                                           const std::string& mesh) {
    CEngine* e = mEngine;
    mSlotFiles[slot] = prefix + name;
    const std::string path = e->mSaveDir + "/" + mSlotFiles[slot];
    std::string l1, l2, l3, l4;
    Vec3 c = color;
    bool usable = true;      // bVar2: the first line is the date (not 0x1c3)
    bool hasFile = false;    // the stream pointer of the original (0 for an old revision)
    if (FILE* f = std::fopen(path.c_str(), "r")) {
        hasFile = true;
        l1 = headerLine(f, 0x31);
        l2 = headerLine(f, 99);
        l3 = headerLine(f, 0x31);
        l4 = headerLine(f, 0x31);
        const std::string revision = headerLine(f, 0x31);
        const std::string mapId = headerLine(f, 0x31);
        const std::string campaign = headerLine(f, 0x31);
        std::fclose(f);
        if (static_cast<unsigned>(std::atoi(revision.c_str())) <
            CConfig::get().getUInt("MinLoadableSavegameRevision", 0)) {   // config +0x3c
            hasFile = false;
            l1.clear();
            l2 = T(0xdc);
            l3.clear();
            l4.clear();
        }
        bool available;
        if (std::atoi(campaign.c_str()) != 0) {
            CCampaign* cmp = e->mCampaigns[1].get();   // std::map::operator[](1)
            available = cmp && cmp->wasInitSuccessful() && cmp->mPurchased && cmp->mDownloaded;
        } else {
            const unsigned id = static_cast<unsigned>(std::atoi(mapId.c_str()));
            available = e->isMapDownloaded(id) && e->isMapPurchased(id);
        }
        if (!available) {
            usable = false;
            c = colors::COL_GRAY50;
        }
    } else {
        CUtils::log("Preload game open failed!");
        l2 = T(0xdc);
    }
    const bool button = (usable && hasFile) || !load;
    if (button)
        addElement(new CInterfaceElementButton2(this, action, mesh, x - 15.0f, y, "", 0, false, 5.0f,
                                                colors::COL_WHITE, 100.0f, 20.0f, 0.0f, 2.0f));
    const Vec3 c75(c.x * 0.75f, c.y * 0.75f, c.z * 0.75f);
    if (usable) {
        addElement(new CInterfaceElementText(this, x, y, l1, 0, 3.5f, c75));
    } else {
        const Vec3& r = colors::COL_RED;
        addElement(new CInterfaceElementText(this, x, y, T(0x1c3), 0, 3.5f, Vec3(r.x * 0.75f, r.y * 0.75f, r.z * 0.75f)));
    }
    addElement(new CInterfaceElementText(this, x, y - 6.0f, l2, 0, 4.0f, c));
    addElement(new CInterfaceElementText(this, x + 65.0f, y, l3, 0, 3.5f, c75));
    addElement(new CInterfaceElementText(this, x + 65.0f, y - 8.0f, l4, 0, 3.5f, c75));
}

// Ghidra 0x1af8a0
bool CInterfaceMenu::isSavegameFromCampaign(const std::string& name, const std::string& prefix) const {
    const std::string path = mEngine->mSaveDir + "/" + prefix + name;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    headerLine(f, 0x31);
    headerLine(f, 99);
    headerLine(f, 0x31);
    headerLine(f, 0x31);
    const std::string revision = headerLine(f, 0x31);
    headerLine(f, 0x31);
    const std::string campaign = headerLine(f, 0x31);
    std::fclose(f);
    if (static_cast<unsigned>(std::atoi(revision.c_str())) < CConfig::get().getUInt("MinLoadableSavegameRevision", 0))
        return false;
    return std::atoi(campaign.c_str()) != 0;
}

// Ghidra 0x1afae4
bool CInterfaceMenu::areSavegamesFromCampaign(const std::string& prefix) const {
    return isSavegameFromCampaign("autosave.txt", prefix) || isSavegameFromCampaign("usersave1.txt", prefix) ||
           isSavegameFromCampaign("usersave2.txt", prefix) || isSavegameFromCampaign("usersave3.txt", prefix) ||
           isSavegameFromCampaign("usersave4.txt", prefix);
}

// Ghidra 0x1d1468: the campaign slider of screen 25 (+0x5f4). Its entry
// (the campaign's id): the name, "<n> Missions - <p>% done", the description,
// and while the campaign is not finished "Chapter <c>: <chapter name>" and
// "Episode <e>: <mission name>" of the current episode. Then it shows
// cmp<id>icon.png and sends 0x4f (which sets up the start buttons).
void CInterfaceMenu::createElementCampaignSlider(const std::string& name, float x, float y,
                                                 const std::string& subMesh, unsigned campaignId) {
    (void)name;
    CEngine* e = mEngine;
    auto* slider = new CInterfaceElementCampaignImageSlider(
        this, 0x4f, x, y, x, y, x + 4.0f, y + 10.0f, x + 4.0f, y - 56.0f, x + 4.0f, y - 62.0f, x + 4.0f, y - 72.0f,
        x + 4.0f, y - 77.0f, x + 42.0f, y - 40.0f, 53.0f, 41.0f, "", subMesh, 5.0f, colors::COL_WHITE, 80.0f, 80.0f,
        -3.0f, 20.0f);
    mCampaignSlider = slider;
    slider->mDisabled = false;
    CCampaign* cmp = e->mCampaigns[campaignId].get();
    if (!cmp) {
        // [PC] the original dereferences the missing campaign
        CUtils::log("createElementCampaignSlider: no campaign %u", campaignId);
        addElement(slider);
        return;
    }
    if (cmp->mDefLoaded) {
        cmp->readProgress();
        char missions[100];
        // [!] "%%%%": the line is a format string again when it is drawn
        std::snprintf(missions, sizeof missions, "%.0f %s - %.0f%%%% %s", static_cast<double>(cmp->mMissionsTotal),
                      T(0x1b4), static_cast<double>(cmp->mPercentDone), T(0x1b5));
        std::string chapterLine, episodeLine;
        if (!cmp->mCompleted) {
            char buf[1024];
            const unsigned c = cmp->mCurChapter;
            std::snprintf(buf, sizeof buf, "%s %i: %s", T(0x1b3), static_cast<int>(c), cmp->mChapterNames[c].c_str());
            chapterLine = buf;
            std::snprintf(buf, sizeof buf, "%s %i: %s", T(0x1b6), static_cast<int>(cmp->mCurEpisode),
                          cmp->getCurrentMapInfo()->name.c_str());
            episodeLine = buf;
        }
        const unsigned current = CConfig::get().getUInt("CurrentMapRevision" + std::to_string(campaignId + 1000), 0);
        slider->addEntry(cmp->mId, cmp->mName, missions, cmp->mDesc, nullptr, false, false, 1, cmp->mRevision, current,
                         chapterLine, episodeLine);
    }
    slider->mCurrentId = 1;
    slider->changeTexture(cmp->mIconFile);
    slider->current().purchased = cmp->mPurchased;
    slider->current().downloaded = cmp->isOutdated() ? false : cmp->mDownloaded;
    addElement(slider);
    processAction(0x4f, slider);
}

}  // namespace py
