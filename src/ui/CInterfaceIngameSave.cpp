// CInterfaceIngameSave.cpp - savegames: the HUD's part, the last section of
// the file (CEngine::saveGame / loadGame): per interface player the camera,
// the overlay text (speaker), the info screen, the marker, the mission timer
// and counter, the avatar and the group bar. CActionBar::save/load are empty.
#include <cstdio>
#include <string>

#include "core/CSaveIO.h"
#include "core/CUtils.h"
#include "game/CCampaign.h"
#include "game/CEngine.h"
#include "render/CCamera.h"
#include "ui/CGroupBar.h"
#include "ui/CInterfaceIngame.h"
#include "ui/CInterfaceIngamePerPlayer.h"

namespace py {

using saveio::D;
using saveio::I;
using saveio::writef;

// Ghidra 0x1a01fc / 0x1a0244
void CInterfaceIngame::save(FILE* f) {
    if (mSplitMode == 0) {
        mIPP[0]->save(f);
    } else if (mSplitMode == 1) {
        mIPP[0]->save(f);
        mIPP[1]->save(f);
    }
}
void CInterfaceIngame::load(FILE* f) {
    if (mSplitMode == 0) {
        mIPP[0]->load(f);
    } else if (mSplitMode == 1) {
        mIPP[0]->load(f);
        mIPP[1]->load(f);
    }
}

// Ghidra 0x1a53a8
void CInterfaceIngamePerPlayer::save(FILE* f) {
    writef(f, "CamOffset %f %f\n", D(mCamera->mTarget.x), D(mCamera->mTarget.y));
    writef(f, "Zoomfactor %f\n", D(mCamera->mZoom));
    writef(f, "OverlayTextDisplayed %i\n", mOverlayDisplayed ? 1 : 0);
    writef(f, "OverlayTextNoPause %i\n", mOverlayNoPause ? 1 : 0);
    if (mOverlayText.empty())
        writef(f, "OverlayText No Text\n");
    else
        writef(f, "OverlayText %s\n", mOverlayText.c_str());
    // [!] tests the overlay text, not the command text: an empty command text
    // of a shown text is written as "OverlayCmdText " (and reads back as the
    // overlay text, see CUtils::safeReadFromFile)
    if (mOverlayText.empty())
        writef(f, "OverlayCmdText none\n");
    else
        writef(f, "OverlayCmdText %s\n", mOverlayCmdText.c_str());
    writef(f, "OverlayTextWaitsForTouch %i\n", mOverlayWaitsForTouch ? 1 : 0);
    writef(f, "OverlayTextTTL %f\n", D(mOverlayTTL));
    writef(f, "OverlayTextRuntime %f\n", D(mOverlayRuntime));
    writef(f, "OverlayTextMinimized %i\n", mOverlayMinimized ? 1 : 0);
    writef(f, "OverlayTextCallerScript %i\n", mOverlayCallerScript);
    writef(f, "OverlayTextFullyDisplayed %i\n", mOverlayFullyDisplayed ? 1 : 0);
    writef(f, "ActionSoundAlreadyPlayed %i\n", mActionSoundPlayed ? 1 : 0);
    writef(f, "ActionMarkerAlreadyShown %i\n", mActionMarkerShown ? 1 : 0);
    writef(f, "InfoScreenActive %i\n", mInfoScreenActive ? 1 : 0);
    writef(f, "InfoScreenObject %i\n", I(mInfoScreenObject));
    writef(f, "ZoomBeforeInfo %f\n", D(mZoomBeforeInfo));
    writef(f, "PosBeforeInfo %f %f\n", D(mPosBeforeInfo.x), D(mPosBeforeInfo.y));
    writef(f, "MarkerEnable %i\n", mMarkerEnabled ? 1 : 0);
    writef(f, "MarkerType %i\n", I(mMarkerType));
    writef(f, "MarkerObjID %i\n", I(mMarkerObject));
    // [!] "%i" with a double (ELF 0x195650): the low word of its bits
    writef(f, "TimerRuntime %i\n", saveio::lowWordOfDouble(D(mTimerRuntime)));
    if (mTimerText.empty())
        writef(f, "TimerText none\n");
    else
        writef(f, "TimerText %s\n", mTimerText.c_str());
    writef(f, "CounterType %i\n", I(mCounterType));
    writef(f, "CounterSubtype %i\n", I(mCounterSubtype));
    writef(f, "CounterCount %i\n", I(mCounterCount));
    writef(f, "CounterReq %i\n", I(mCounterRequired));
    writef(f, "OverlayTextShowIcon %i\n", mOverlayShowIcon ? 1 : 0);
    if (!mOverlayShowIcon)
        writef(f, "OverlayTextIconName none\n");
    else
        writef(f, "OverlayTextIconName %s\n", mOverlayIconName.c_str());
    mGroupBar->save(f);
    // CActionBar::save (Ghidra 0x1201fc): empty
}

// Ghidra 0x1a4b90
void CInterfaceIngamePerPlayer::load(FILE* f) {
    if (!CUtils::safeReadFromFile(f, "CamOffset", &mCamera->mTarget)) return;
    Real zoom = mCamera->mZoom;
    const bool zoomOk = CUtils::safeReadFromFile(f, "Zoomfactor", &zoom);
    mCamera->mZoom = zoom;
    if (!zoomOk) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextDisplayed", &mOverlayDisplayed)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextNoPause", &mOverlayNoPause)) return;
    // [!] an empty text was written as "No Text" and reads back as that
    if (!CUtils::safeReadFromFile(f, "OverlayText", &mOverlayText, true)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayCmdText", &mOverlayCmdText, true)) return;
    if (mOverlayCmdText == "none") mOverlayCmdText.clear();
    if (!CUtils::safeReadFromFile(f, "OverlayTextWaitsForTouch", &mOverlayWaitsForTouch)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextTTL", &mOverlayTTL)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextRuntime", &mOverlayRuntime)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextMinimized", &mOverlayMinimized)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextCallerScript", &mOverlayCallerScript)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextFullyDisplayed", &mOverlayFullyDisplayed)) return;
    if (!CUtils::safeReadFromFile(f, "ActionSoundAlreadyPlayed", &mActionSoundPlayed)) return;
    if (!CUtils::safeReadFromFile(f, "ActionMarkerAlreadyShown", &mActionMarkerShown)) return;
    if (!CUtils::safeReadFromFile(f, "InfoScreenActive", &mInfoScreenActive)) return;
    if (!CUtils::safeReadFromFile(f, "InfoScreenObject", &mInfoScreenObject)) return;
    if (!CUtils::safeReadFromFile(f, "ZoomBeforeInfo", &mZoomBeforeInfo)) return;
    if (!CUtils::safeReadFromFile(f, "PosBeforeInfo", &mPosBeforeInfo)) return;
    if (!CUtils::safeReadFromFile(f, "MarkerEnable", &mMarkerEnabled)) return;
    if (!CUtils::safeReadFromFile(f, "MarkerType", &mMarkerType)) return;
    if (!CUtils::safeReadFromFile(f, "MarkerObjID", &mMarkerObject)) return;
    // [!] the value written was an integer (see save): read as a float
    if (!CUtils::safeReadFromFile(f, "TimerRuntime", &mTimerRuntime)) return;
    if (!CUtils::safeReadFromFile(f, "TimerText", &mTimerText, true)) return;
    if (mTimerText == "none") mTimerText.clear();
    unsigned counterType = 0;
    if (!CUtils::safeReadFromFile(f, "CounterType", &counterType)) return;
    mCounterType = counterType;
    if (!CUtils::safeReadFromFile(f, "CounterSubtype", &mCounterSubtype)) return;
    if (!CUtils::safeReadFromFile(f, "CounterCount", &mCounterCount)) return;
    if (!CUtils::safeReadFromFile(f, "CounterReq", &mCounterRequired)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextShowIcon", &mOverlayShowIcon)) return;
    if (!CUtils::safeReadFromFile(f, "OverlayTextIconName", &mOverlayIconName, true)) return;
    if (mOverlayShowIcon) {
        // the avatar of the speaker, as setOverlayText sets it (Ghidra 0x1a7ea8)
        if (CCampaign* cmp = mEngine->getCurCampaign()) {
            mOverlayShowIcon = true;
            mAvatarSubtitle = cmp->getAvatarSubtitle(mOverlayIconName);
            mAvatarTexture = cmp->getAvatarTexture();
            const Vec3 uv = cmp->getAvatarUVs(mOverlayIconName);
            mAvatarUV = CVector2(uv.x, uv.y);
            mAvatarUVSize = CVector2(uv.z, 1.0f);
            mAvatarColor = cmp->getAvatarColor(mOverlayIconName);
        }
    }
    mGroupBar->load(f);
    // CActionBar::load (Ghidra 0x120200): empty
    mStatusText = "Game loaded!";
}

// Ghidra 0x186d34
void CGroupBar::save(FILE* f) {
    writef(f, "NumGroupInfos %i\n", static_cast<int>(mGroups.size()));
    for (const CGroupInfo& g : mGroups) {
        writef(f, "GroupID %i\n", I(g.id));
        writef(f, "Active %i\n", g.active ? 1 : 0);
        writef(f, "Type %i\n", g.type);
        writef(f, "UnderAttackWarningTimeout %f\n", D(g.underAttackTimeout));
        writef(f, "UnderAttackWarningUnit %i\n", I(g.underAttackUnit));
        writef(f, "NumUnits %i\n", static_cast<int>(g.units.size()));
        for (unsigned u : g.units) writef(f, "Unit %i\n", I(u));
    }
}

// Ghidra 0x187f18. The groups are replaced by the loaded ones, which keep
// only what was saved: [!] position, colour and icon UVs are zero (init's
// values are gone).
void CGroupBar::load(FILE* f) {
    mGroups.clear();
    int n = 0;
    if (!CUtils::safeReadFromFile(f, "NumGroupInfos", &n)) return;
    for (int i = 0; i < n; ++i) {
        CGroupInfo g;
        g.pos = CVector2(0.0f, 0.0f);
        g.color = Vec3(0.0f, 0.0f, 0.0f);
        g.uv = CVector2(0.0f, 0.0f);
        if (!CUtils::safeReadFromFile(f, "GroupID", &g.id)) return;
        if (!CUtils::safeReadFromFile(f, "Active", &g.active)) return;
        unsigned type = static_cast<unsigned>(g.type);
        if (!CUtils::safeReadFromFile(f, "Type", &type)) return;
        g.type = static_cast<int>(type);
        if (!CUtils::safeReadFromFile(f, "UnderAttackWarningTimeout", &g.underAttackTimeout)) return;
        if (!CUtils::safeReadFromFile(f, "UnderAttackWarningUnit", &g.underAttackUnit)) return;
        int units = 0;
        if (!CUtils::safeReadFromFile(f, "NumUnits", &units)) return;
        for (int k = 0; k < units; ++k) {
            int u = 0;
            if (!CUtils::safeReadFromFile(f, "Unit", &u)) return;
            g.units.push_back(static_cast<unsigned>(u));
        }
        mGroups.push_back(g);
    }
}

}  // namespace py
