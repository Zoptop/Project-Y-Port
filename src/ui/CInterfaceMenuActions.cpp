// CInterfaceMenuActions.cpp - CInterfaceMenu::processAction (Ghidra
// 0x1cd3f8), the menu's EMENUACTIONS switch. Screen changes set the
// requested screen (+0x90) and scroll the menu camera to its spot at speed
// 10; the caller (processEventActionUp & co.) then runs setupElements.
//
// Not ported here (later phases), each logged as "notPorted": the lobby and
// game clients (multiplayer, friends, chat, rankings), downloads, purchases
// and savegames. The option actions are
// complete and call CEngine::saveOptions like the original.
#include "CInterfaceMenu.h"

#include "core/CConfig.h"
#include "core/CTxtLoc.h"
#include "core/CUtils.h"
#include "game/CCampaign.h"
#include "game/CCommandManager.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CSoundManager.h"
#include "render/CCameraMenu.h"
#include "render/Colors.h"
#include "ui/CInterfaceElementBarSlider.h"
#include "ui/CInterfaceElementButton2.h"
#include "ui/CInterfaceElementCampaignButtons.h"
#include "ui/CInterfaceElementImageSlider.h"
#include "ui/CInterfaceElementPlayerControl.h"
#include "ui/CInterfaceElementTextInput2.h"
#include "ui/CInterfaceElementTextSlider2.h"

namespace py {

// The slowScrollTo targets of processAction, by screen (Ghidra 0x1cd3f8)
CVector2 CInterfaceMenu::screenCenter(int menuType) {
    switch (menuType) {
        case MENU_SINGLEPLAYER: return CVector2(bitsf(0x43050000), bitsf(0x43870000));   // (133, 270)
        case MENU_MULTIPLAYER: return CVector2(bitsf(0x43a02666), bitsf(0xc17ccccd));    // (320.3, -15.8)
        case MENU_PROFILE:
        case 0x1c: return CVector2(bitsf(0xc4600000), bitsf(0x44124000));                 // (-896, 585)
        case MENU_INGAME: return CVector2(bitsf(0xc415c000), bitsf(0xc30f0000));          // (-599, -143)
        case MENU_OPTIONS:
        case MENU_OPTIONS2: return CVector2(bitsf(0xc3988000), bitsf(0xc32a0000));        // (-305, -170)
        case MENU_SAVEGAME: return CVector2(bitsf(0xc40e0000), bitsf(0x433d0000));        // (-568, 189)
        case MENU_LOADGAME: return CVector2(bitsf(0x43fd0000), bitsf(0x43868000));        // (506, 269)
        case MENU_TUTORIAL: return CVector2(bitsf(0xc2500000), bitsf(0x440b0000));        // (-52, 556)
        case MENU_CUSTOMGAME: return CVector2(bitsf(0x43bf0000), bitsf(0x4403c000));      // (382, 527)
        case MENU_STATISTICS:
        case MENU_AFTERMATH:
        case 0x1f: return CVector2(bitsf(0xc4508000), bitsf(0xc3c20000));                 // (-834, -388)
        case MENU_DOWNLOAD: return CVector2(bitsf(0x44300000), bitsf(0x44048000));        // (704, 530)
        case MENU_CREDITS: return CVector2(bitsf(0xc3770000), bitsf(0x43850000));         // (-247, 266)
        case MENU_SPLITSCREEN: return CVector2(bitsf(0x44270000), bitsf(0x41e80000));     // (668, 29)
        case MENU_HOSTSERVER: return CVector2(bitsf(0x44360000), bitsf(0xc3840000));      // (728, -264)
        case MENU_ENTERIP:
        case 0x1e: return CVector2(bitsf(0x42680000), bitsf(0xc39d0000));                 // (58, -314)
        case MENU_SERVERLIST: return CVector2(bitsf(0x43ce8000), bitsf(0xc3a00000));      // (413, -320)
        case MENU_LOBBYCHAT: return CVector2(bitsf(0x44368000), bitsf(0xc4198000));       // (730, -614)
        case MENU_WAITINGROOM: return CVector2(bitsf(0x43be0000), bitsf(0xc4138000));     // (380, -590)
        case MENU_UPDATENEWS:
        case 0x1b: return CVector2(bitsf(0xc3e80000), bitsf(0x44060000));                 // (-464, 536)
        case 0x16: return CVector2(bitsf(0xc4478000), bitsf(0x41e00000));                 // (-798, 28)
        case 0x17:
        case 0x1d: return CVector2(bitsf(0xc46c0000), bitsf(0x43a38000));                 // (-944, 327)
        case 0x18: return CVector2(bitsf(0xc3af0000), bitsf(0xc3e10000));                 // (-350, -450)
        case MENU_CAMPAIGN: return CVector2(bitsf(0x433e0000), bitsf(0x44490000));        // (190, 804)
        case 0x1a: return CVector2(bitsf(0xc3810000), bitsf(0x44490000));                 // (-258, 804)
        default: return CVector2(0.0f, 0.0f);
    }
}

namespace {
void notPorted(const char* what, int action) {
    CUtils::log("CInterfaceMenu::processAction 0x%x (%s): notPorted", action, what);
}
}  // namespace

// The common tail of processAction 0x50, 0x52 and 0x53 (Ghidra 0x1cd818):
// campaign 1 must be purchased (always) and installed (else the download
// screen), then the fixed player slots of a campaign mission (0 neutral, 1
// the human, 2..4 computer players; names from the config, id = team =
// colour = slot), the current episode's map and player count, production
// speed 2, game speed 1, FOW on, crates off, no sector or unit presets; the
// loading screen (draw mode 1) follows and draw mode 2 calls
// initSingleplayerGame. The game type stays 1 (set by screen 25).
void CInterfaceMenu::startCurrentCampaignEpisode() {
    CEngine* e = mEngine;
    CCampaign* c1 = e->mCampaigns[1].get();
    if (!c1 || !c1->mPurchased) {
        CUtils::log("purchase required for campaign");
        mNextMenu = MENU_SINGLEPLAYER;
        mCamera->slowScrollTo(screenCenter(MENU_SINGLEPLAYER), 10.0f);
        return;
    }
    if (!c1->mDownloaded) {
        CUtils::log("download required for campaign");
        mNextMenu = MENU_DOWNLOAD;
        mCamera->slowScrollTo(screenCenter(MENU_DOWNLOAD), 10.0f);
        CUtils::log("CEngine::downloadMap %u: notPorted (no downloads on PC)", 0x3e9u);
        mDownloadMapName = CConfig::get().getString("MapName1001", "");
        mDownloadInProgress = true;
        return;
    }
    CConfig& cfg = CConfig::get();
    CPlayerSetup* s = e->mPlayerSetup;
    static const int kType[5] = {1, 0, 2, 2, 2};
    for (int i = 0; i < 5; ++i) {
        s[i].name = cfg.getString("Player" + std::to_string(i) + "Name", "");
        s[i].type = kType[i];
    }
    for (int i = 0; i < 5; ++i) {
        s[i].id = i;
        s[i].team = static_cast<unsigned>(i);
        s[i].colorIndex = i;
    }
    CCampaign* cur = e->getCurCampaign();
    if (!cur) return;   // [PC] the original dereferences the missing campaign
    e->mNumPlayers = cur->getCurrentMapInfo()->numPlayers;
    e->mMapId = cur->getCurrentMapInfo()->mapId;
    e->mProductionSpeed = 2;
    e->mGameSpeedFactor = 1.0f;
    e->mLocalPlayer = 1;
    e->mUseFOW = true;
    e->mCratesEnabled = false;
    e->mUnitAvailabilityMode = 0;
    e->mSectorSetupMode = 0;
    mStartGameMode = 1;
    mDrawMode = 1;
}

// Ghidra 0x1cd3f8
void CInterfaceMenu::processAction(int action, CInterfaceElement* element) {
    CEngine* e = mEngine;
    auto go = [&](int screen) {
        mNextMenu = screen;
        mCamera->slowScrollTo(screenCenter(screen), 10.0f);
    };
    // the action that rebuilds a screen at once (the LAN/public server list)
    auto rebuild = [&](int screen, int arg) {
        mForceSetup = false;
        mNextMenu = screen;
        mCurrentMenu = screen;
        setupElements(screen, arg);
        mCamera->slowScrollTo(screenCenter(screen), 10.0f);
    };
    auto slider = [&]() { return dynamic_cast<CInterfaceElementTextSlider2*>(element); };
    auto sliderValue = [&]() -> unsigned {
        auto* s = slider();
        return s ? s->mValue : 0u;
    };
    auto downloadRequired = [&](unsigned mapId) {
        CUtils::log("download required %i", static_cast<int>(mapId));
        go(MENU_DOWNLOAD);
        // CEngine::downloadMap: [PC] maps come from data/, nothing to download
        CUtils::log("CEngine::downloadMap %u: notPorted (no downloads on PC)", mapId);
        mDownloadMapName = CConfig::get().getString("MapName" + std::to_string(mapId), "");
        mDownloadInProgress = true;
    };
    switch (action) {
        case 1:   // Singleplayer
            e->mGameType = 0;
            go(MENU_SINGLEPLAYER);
            break;
        case 2:   // Multiplayer (the lobby's online status: not ported)
            e->mGameSpeedFactor = 1.0f;
            go(MENU_MULTIPLAYER);
            break;
        case 3: go(MENU_PROFILE); break;
        case 4: go(MENU_OPTIONS); break;
        case 5: go(MENU_OPTIONS2); break;
        case 6: e->quit(); break;   // CEngine::quit: [PC] ends the frame loop
        case 7: go(MENU_MAIN); break;
        case 8: go(MENU_INGAME); break;
        case 9: go(MENU_TUTORIAL); break;
        case 10: go(MENU_CUSTOMGAME); break;
        case 0xb:   // Load game
            // from the single player screen: with a campaign save among the
            // slots the campaign is checked first (their slots need it), then
            // 0x5c opens the screen
            if (mCurrentMenu == MENU_SINGLEPLAYER && areSavegamesFromCampaign("single")) {
                if (CCampaign* cmp = e->mCampaigns[1].get()) cmp->startInitAsync(false);   // operator[](1)
                mModalState = 1;
                mActionAfterLongOp = 0x5c;
                break;
            }
            go(MENU_LOADGAME);
            break;
        case 0xc:   // Save game (the pause screen); the same campaign check, then 0x5d
            if (e->mNetworkMode != 1 && areSavegamesFromCampaign("single")) {
                if (CCampaign* cmp = e->mCampaigns[1].get()) cmp->startInitAsync(false);
                mModalState = 1;
                mActionAfterLongOp = 0x5d;
                break;
            }
            go(MENU_SAVEGAME);
            break;
        case 0xd:   // start the custom game
            if (!e->isMapPurchased(e->mMapId)) {
                CUtils::log("purchase required %i", static_cast<int>(e->mMapId));
                go(MENU_SINGLEPLAYER);
            } else if (!mMapOutdated && e->isMapDownloaded(e->mMapId)) {
                mStartGameMode = 1;
                mDrawMode = 1;
            } else {
                downloadRequired(e->mMapId);
            }
            break;
        case 0xe: {   // start the tutorial (map 201)
            CConfig& cfg = CConfig::get();
            CPlayerSetup* s = e->mPlayerSetup;
            s[0].name = cfg.getString("Player0Name", "");
            s[0].type = 1;
            s[1].name = cfg.getString("Player1Name", "");
            s[1].type = 0;
            s[2].name = cfg.getString("Player2Name", "");
            s[2].type = 2;
            for (int i = 0; i < 3; ++i) {
                s[i].id = i;
                s[i].team = static_cast<unsigned>(i);
                s[i].colorIndex = i;
            }
            e->mNumPlayers = 2;
            e->mMapId = 0xc9;
            e->mProductionSpeed = 2;
            e->mGameSpeedFactor = 1.0f;
            e->mLocalPlayer = 1;
            e->mUseFOW = true;
            e->mUseAutoUpgrade = false;
            e->mCratesEnabled = false;
            e->mUnitAvailabilityMode = 0;
            e->mSectorSetupMode = 0;
            mStartGameMode = 1;
            mDrawMode = 1;
            break;
        }
        case 0xf: {   // the map slider changed
            auto* ms = dynamic_cast<CInterfaceElementImageSlider*>(element);
            if (!ms) {
                CUtils::log("MENUACTION_MAPSLIDERCHANGED slider is NULL");
                break;
            }
            e->mMapId = ms->mCurrentId;
            char icon[64];
            std::snprintf(icon, sizeof icon, "map%uicon.png", ms->mCurrentId);
            ms->changeTexture(icon);
            if (mStartButton) {
                const CInterfaceElementImageSlider::CEntry& en = ms->current();
                if (!en.purchased) {
                    mStartButton->mTextColor = Vec3(1.0f, bitsf(0x3e99999a), bitsf(0x3e99999a));
                } else if (!en.downloaded) {
                    mStartButton->mText = CTxtLoc::get().getC(0x106);
                    mStartButton->mTextColor = Vec3(bitsf(0x3ecccccd), bitsf(0x3ecccccd), 1.0f);
                } else {
                    const bool host = mCurrentMenu == MENU_HOSTSERVER || mNextMenu == MENU_HOSTSERVER;
                    mStartButton->mText = CTxtLoc::get().getC(host ? 0x107 : 0x17);
                    const Vec3& w = colors::COL_WHITE;
                    const Vec3& g = colors::COL_GREEN;
                    const float a = bitsf(0x3f333333), b = bitsf(0x3e99999a);
                    mStartButton->mTextColor = Vec3(w.x * a + g.x * b, w.y * a + g.y * b, w.z * a + g.z * b);
                }
            }
            if (mMapSlider) e->mNumPlayers = mMapSlider->current().numPlayers;
            if (mPlayerControl) mPlayerControl->setNumPlayers(ms->current().numPlayers);
            mSelectedMapName = ms->current().name;
            e->saveOptions();
            break;
        }
        case 0x10: e->mDifficulty = static_cast<int>(sliderValue()); e->saveOptions(); break;
        case 0x11: e->mDrawCircles = sliderValue() == 1; e->saveOptions(); break;
        case 0x12: e->mDrawWater = sliderValue() == 1; e->saveOptions(); break;
        case 0x13:
            e->mShowDebug = sliderValue() == 1;   // (+0x1a0 = 0)
            e->saveOptions();
            break;
        case 0x14:
            e->mGameSpeedFactor = static_cast<float>(sliderValue());
            CUtils::log("GameSpeedFactor: %f", static_cast<double>(e->mGameSpeedFactor));
            e->saveOptions();
            break;
        case 0x15: e->mProductionSpeed = static_cast<int>(sliderValue()); e->saveOptions(); break;
        case 0x16:   // back to the game
            e->mCurrentInterface = e->mIngameInterface.get() ? e->mIngameInterface.get() : e->mCurrentInterface;
            if (e->mPaused) {
                if (e->mGame && e->mGame->getCommandManager() && e->mGame->getCommandManager()->isForcedPause()) break;
                e->resumeGame(e->mFrameTime);
            }
            break;
        case 0x17: {   // leave the game for the main menu
            // killGameClient / killLocalServer: not ported
            if (!e->mLoadingSavegame && e->mGameTime > 0.0f && e->mGame && e->mGame->mStarted &&
                e->mNetworkMode != 2) {
                CPlayer* p = e->mGame->getPlayer(e->mLocalPlayer);
                // the autosave of a game left for the main menu
                if (p && !p->mLost && !p->mWon)
                    e->saveGame(e->mNetworkMode == 1 ? "multiautosave.txt" : "singleautosave.txt");
            }
            e->resetGame();
            go(MENU_MAIN);
            if (CSoundManager* sm = e->getSoundManager()) sm->startBackgroundMusic();
            break;
        }
        case 0x18: e->mTouchMinMovedDistanceID = sliderValue(); e->saveOptions(); break;
        case 0x19: e->setMusicVolume(sliderValue()); e->saveOptions(); break;   // CSoundManager::setMixerMusicVolume
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e: {   // a save/load slot
            static const int kSlot[5] = {1, 2, 3, 4, 0};
            mSaveSlot = kSlot[action - 0x1a];
            if (mCurrentMenu == MENU_SAVEGAME) {
                // save over the slot, back to the pause screen
                mSaveFile = mSlotFiles[mSaveSlot];
                if (e->mGame)
                    e->saveGame(mSaveFile);
                else
                    CUtils::log("CEngine::saveGame %s: no game", mSaveFile.c_str());   // [PC] the original crashes
                go(MENU_INGAME);
            } else if (mCurrentMenu == MENU_LOADGAME) {
                mLoadFile = mSlotFiles[mSaveSlot];
                mStartGameMode = 3;
                mDrawMode = 1;
            }
            break;
        }
        case 0x1f: {   // language
            const unsigned v = sliderValue();
            if (v == 0) e->mLanguage = "English";
            else if (v == 1) e->mLanguage = "Deutsch";
            CTxtLoc::get().load(e->mLanguage);
            e->saveOptions();
            // CTechManager::init (the unit names): not ported here
            // the campaign in the new language
            e->initCampaign(1)->startInitAsync(false);
            mModalState = 1;
            mActionAfterLongOp = 0x5b;
            break;
        }
        case 0x20: go(MENU_STATISTICS); break;
        case 0x21: go(MENU_AFTERMATH); break;
        case 0x22: e->mUseFOW = sliderValue() == 1; e->saveOptions(); break;
        case 0x23: go(MENU_CREDITS); break;
        case 0x24: e->mCratesEnabled = sliderValue() == 1; e->saveOptions(); break;
        case 0x25: e->openURL("https://www.luminencelabs.com/"); break;
        case 0x26: e->openURL("https://www.luminencelabs.com/gameguide/"); break;
        case 0x27: e->openURL("https://www.luminencelabs.com/forums/forum/feedback-suggestions/"); break;
        case 0x28:
            e->mGUIScalingOption = sliderValue();
            e->updateGUIScaling();
            e->saveOptions();
            break;
        case 0x29: e->openURL("https://www.luminencelabs.com/database/dbmaps/"); break;
        case 0x2a: e->setSFXVolume(sliderValue()); e->saveOptions(); break;   // CSoundManager::setMixerSFXVolume
        case 0x2b: e->mPublishServer = sliderValue() == 1; break;
        case 0x2c: e->mEventVerboseness = static_cast<int>(sliderValue()); e->saveOptions(); break;
        case 0x2d: e->mBeginnerMode = sliderValue() == 1; e->saveOptions(); break;
        case 0x2e:
            CUtils::log("download required 201");
            downloadRequired(0xc9);
            break;
        case 0x2f: e->mUnitAvailabilityMode = static_cast<int>(sliderValue()); e->saveOptions(); break;
        case 0x30: e->mSectorSetupMode = static_cast<int>(sliderValue()); e->saveOptions(); break;
        case 0x31: {   // game type: colours and teams back to the slot numbers
            e->mGameType = static_cast<int>(sliderValue());
            for (int i = 0; i < CEngine::kMaxPlayers; ++i) {
                e->mPlayerSetup[i].team = static_cast<unsigned>(i);
                e->mPlayerSetup[i].colorIndex = i;
            }
            break;
        }
        case 0x32:   // the player table changed: the ready button of the waiting room
            if (mReadyButton)
                mReadyButton->mText =
                    CTxtLoc::get().getC(e->mPlayerSetup[e->mLocalPlayer].ready == 0 ? 0x108 : 0x109);
            break;
        case 0x33: go(MENU_SPLITSCREEN); break;
        case 0x34: go(MENU_HOSTSERVER); break;
        case 0x35:
            CUtils::log("Network: start lan discovery");
            mLanServerList = 1;
            rebuild(MENU_SERVERLIST, 2);
            notPorted("CLobbyClient::discoverLANServer", action);
            break;
        case 0x36: go(MENU_ENTERIP); break;
        case 0x37:
            mLanServerList = 0;
            rebuild(MENU_SERVERLIST, mCurrentMenu == MENU_LOBBYCHAT ? 0x38 : 2);
            CUtils::log("Network: start query public");
            CUtils::log("Network: not connected");
            break;
        case 0x38: notPorted("lobby chat: needs the lobby connection", action); break;
        case 0x39: {
            auto* in = dynamic_cast<CInterfaceElementTextInput2*>(element);
            std::string ip = in ? in->mValue : std::string();
            if (in) in->mValue.clear();
            mTextInput = nullptr;
            if (!ip.empty()) {
                mLanServerList = 1;
                rebuild(MENU_SERVERLIST, 2);
                CUtils::log("Network: check direct ip");
                notPorted("CLobbyClient::pingServer", action);
            }
            break;
        }
        case 0x3a:   // start the split screen game
            if (!mMapOutdated && e->isMapDownloaded(e->mMapId)) {
                mStartGameMode = 2;
                mDrawMode = 1;
            } else {
                downloadRequired(e->mMapId);
            }
            break;
        case 0x3b:
            if (!mMapOutdated && e->isMapDownloaded(e->mMapId))
                notPorted("CInterfaceMenu::hostNetworkGame", action);
            else
                downloadRequired(e->mMapId);
            break;
        case 0x3c:   // leave the waiting room
            if (mLanServerList != 0) {
                go(MENU_MULTIPLAYER);
            } else {
                rebuild(MENU_SERVERLIST, 2);
                CUtils::log("Network: start query public");
                CUtils::log("Network: not connected");
            }
            break;
        case 0x3d: CUtils::log("Network: ping server"); break;
        case 0x3e: CUtils::log("Network: connect serverlist server"); break;
        case 0x3f:
            CUtils::log("MENUACTION_TOGGLEREADY: set ready");
            CUtils::log("MENUACTION_TOGGLEREADY: set ready done");
            break;
        case 0x40:   // the server starts the network game
            e->mGameSpeedFactor = 1.0f;
            mStartGameMode = 4;
            mDrawMode = 1;
            break;
        case 0x41: {   // a chat line in the waiting room
            if (auto* in = dynamic_cast<CInterfaceElementTextInput2*>(element)) {
                if (!in->mValue.empty()) notPorted("CGameClient::sendChat", action);
                in->mValue.clear();
            }
            mTextInput = nullptr;
            break;
        }
        case 0x42:   // the version button: update news (after a campaign check)
            if (CCampaign* c = e->mCampaigns[1].get()) c->startInitAsync(true);
            mModalState = 1;
            mActionAfterLongOp = 0x77;
            break;
        case 0x43: {   // profile: name
            auto* in = dynamic_cast<CInterfaceElementTextInput2*>(element);
            if (!in) break;
            mTextInput = nullptr;
            CUtils::log("Profile: playername changed");
            e->mPlayerName = in->mValue;
            e->validateNameAndClan();
            e->saveOptions();
            in->mValue = e->mPlayerName;
            break;
        }
        case 0x44: e->mUseDoubleTouch = sliderValue() == 1; e->saveOptions(); break;
        case 0x45: e->mUseAutoUpgrade = sliderValue() == 1; e->saveOptions(); break;
        case 0x46:   // profile: account ([PC] the only account is "none")
            e->saveOptions();
            mCurrentMenu = 0;
            mNextMenu = MENU_PROFILE;
            break;
        case 0x47: go(0x16); break;
        case 0x48: go(0x17); break;
        case 0x49:
            e->mMinimapSize = static_cast<int>(sliderValue());
            e->saveOptions();
            // the ingame interface's setResolution follows (it places the minimap)
            break;
        case 0x4a: go(0x18); break;
        case 0x4b:
        case 0x4c: notPorted("CUnitFactory auto upgrade probabilities (screen 24)", action); break;
        case 0x4d:   // campaign (after the campaign file check)
            if (CCampaign* c = e->mCampaigns[1].get()) c->startInitAsync(false);
            mModalState = 1;
            mActionAfterLongOp = 0x5a;
            break;
        case 0x4e: go(0x1a); break;
        case 0x4f: {   // the campaign slider changed: the start buttons follow the campaign
            if (mCampaignReplayButton) mCampaignReplayButton->mDisabled = true;
            auto* cs = dynamic_cast<CInterfaceElementImageSlider*>(element);
            if (!cs) {
                CUtils::log("MENUACTION_CAMPAIGNSLIDERCHANGED slider is NULL");
                break;
            }
            e->mCurrentCampaignId = cs->mCurrentId;
            CCampaign* cmp = e->mCampaigns[cs->mCurrentId].get();
            if (!cmp) break;   // [PC] the original dereferences the missing campaign
            cs->changeTexture(cmp->mIconFile);
            if (!mCampaignStartButton) break;
            mCampaignStartButton->mDisabled = false;
            const CInterfaceElementImageSlider::CEntry& en = cs->current();
            if (!en.purchased) {
                mCampaignStartButton->mTextColor = Vec3(1.0f, bitsf(0x3e99999a), bitsf(0x3e99999a));
            } else if (!en.downloaded) {
                mCampaignStartButton->mText = CTxtLoc::get().getC(0x106);
                mCampaignStartButton->mTextColor = Vec3(bitsf(0x3ecccccd), bitsf(0x3ecccccd), 1.0f);
            } else {
                if (!cmp->mProgressLoaded) {
                    mCampaignStartButton->mText = CTxtLoc::get().getC(0x1af);   // "Start Campaign"
                } else {
                    mCampaignStartButton->mText = CTxtLoc::get().getC(0x1b7);   // "Continue"
                    if (mCampaignReplayButton) mCampaignReplayButton->mDisabled = false;
                }
                const Vec3& w = colors::COL_WHITE;
                const Vec3& g = colors::COL_GREEN;
                const float a = bitsf(0x3f333333), b = bitsf(0x3e99999a);
                mCampaignStartButton->mTextColor = Vec3(w.x * a + g.x * b, w.y * a + g.y * b, w.z * a + g.z * b);
                if (cmp->mCompleted) mCampaignStartButton->mDisabled = true;
            }
            break;
        }
        case 0x51:   // an episode of the progress screen was picked
            if (auto* cb = dynamic_cast<CInterfaceElementCampaignButtons*>(element)) {
                if (CCampaign* cmp = e->getCurCampaign()) {
                    cmp->mCurChapter = cb->mSelectedChapter;
                    cmp->mCurEpisode = cb->mSelectedEpisode;
                }
            } else {
                CUtils::log("MENUACTION_CAMPAIGNEPISODECHANGED cmpbtns is NULL");
            }
            break;
        case 0x53:   // aftermath "Continue": the game is dropped, the current episode starts
            e->resetGame();
            startCurrentCampaignEpisode();
            break;
        case 0x50:   // start / continue the campaign
        case 0x52:   // replay the episode picked on the progress screen
            startCurrentCampaignEpisode();
            break;
        case 0x54: {   // the end of the campaign: the credits roll (draw mode 3)
            mCreditsScroll = 0.0f;
            mDrawMode = 3;
            CCampaign* cmp = e->getCurCampaign();
            mCreditsText = cmp ? cmp->getCampaignText("final") : std::vector<CTextListing>();
            mBackAction = 0;
            break;
        }
        case 0x55: {   // profile: clan
            auto* in = dynamic_cast<CInterfaceElementTextInput2*>(element);
            if (!in) break;
            mTextInput = nullptr;
            CUtils::log("Profile: playerclan changed");
            e->mPlayerClan = in->mValue;
            e->validateNameAndClan();
            e->saveOptions();
            in->mValue = e->mPlayerClan;
            break;
        }
        case 0x56: e->openURL("https://play.google.com/store/apps/details?id=com.luminencelabs.ProjectY.gf"); break;
        case 0x57: e->openURL("https://twitter.com/LuminenceLabs/"); break;
        case 0x59: e->openURL("https://www.facebook.com/LuminenceLabs/"); break;
        case 0x5a:
            mModalState = 0;
            go(MENU_CAMPAIGN);
            break;
        case 0x5b:
            mModalState = 0;
            go(mBackAction == 7 ? MENU_MAIN : MENU_INGAME);
            break;
        case 0x5c:
            mModalState = 0;
            go(MENU_LOADGAME);
            break;
        case 0x5d:
            mModalState = 0;
            go(MENU_SAVEGAME);
            break;
        case 0x5e: go(0x1c); break;
        case 0x5f:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x6d:
        case 0x6e: notPorted("lobby friends/chat", action); break;
        case 0x65:
            e->mProfilePrivate = sliderValue() == 0;   // '\x01' - value for 0/1
            e->saveOptions();
            break;
        case 0x66:
            e->mOnlineStatusPrivate = static_cast<int>(sliderValue()) == -1;   // [!] the slider has keys 0/1
            e->saveOptions();
            break;
        case 0x67:
            mRankedOption = static_cast<int>(sliderValue());
            mCurrentMenu = 0;
            mNextMenu = MENU_PROFILE;
            break;
        case 0x68: notPorted("CInterfaceElementServerList::setShowOnlyRanked", action); break;
        case 0x69:
            mTop10Option = static_cast<int>(sliderValue());
            mCurrentMenu = 0;
            mNextMenu = 0x16;
            break;
        case 0x6a: break;
        case 0x6b: go(0x1d); break;
        case 0x6c: {   // the host's server name
            auto* in = dynamic_cast<CInterfaceElementTextInput2*>(element);
            if (!in) break;
            std::string name = in->mValue;
            for (char& c : name) {   // CUtils::removeForbiddenChar subset + trim + spaces -> '_'
                if (c == ' ') c = '_';
            }
            if (name.empty()) name = "IP";
            mTextInput = nullptr;
            CUtils::log("Host server: servername changed");
            e->mServerName = name;
            in->mValue = e->mServerName;
            e->saveOptions();
            break;
        }
        case 0x6f: {   // surrender
            if (CGame* game = e->mGame) {
                if (CCommandManager* cm = game->getCommandManager()) {
                    cm->cmdPlayerSurrenders(e->mLocalPlayer);
                    // [?] the original adds the command manager's +8 to the game time here
                    cm->timeUpdate();
                }
                if (CPlayer* p = game->getPlayer(e->mLocalPlayer)) p->lost(true);
            }
            go(MENU_AFTERMATH);
            break;
        }
        case 0x70: go(MENU_SURRENDER); break;
        case 0x71:   // after the intro (CAchievementManager/CFriendList::init: not ported)
            // the lobby is never connected on PC: the offline branch
            if (e->mLastVersion < static_cast<unsigned>(CConfig::get().getInt("Revision", 0))) {
                CUtils::log("sync result was false, version updated");
                if (CCampaign* c = e->mCampaigns[1].get()) c->startInitAsync(true);
                mModalState = 1;
                mActionAfterLongOp = 0x77;
            } else {
                mCamera->slowScrollTo(CVector2(0.0f, 0.0f), 10.0f);
                CUtils::log("sync result was false, no version info available");
            }
            break;
        case 0x72:
            e->saveOptions();
            e->mOfflineMode = sliderValue() == 1;
            break;
        case 0x73: e->mGlobalChatEnabled = sliderValue() == 1; e->saveOptions(); break;
        case 0x74: e->mShowCompass = sliderValue() == 1; e->saveOptions(); break;
        case 0x75: go(0x1f); break;
        case 0x76: notPorted("download queue", action); break;
        case 0x77: go(MENU_UPDATENEWS); break;
        default: CUtils::log("MENU ACTION %i NOT IMPLEMENTED", action); break;
    }
}

}  // namespace py
