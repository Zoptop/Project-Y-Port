// CSoundMoodAnalyzer.cpp - see CSoundMoodAnalyzer.h.
#include "game/CSoundMoodAnalyzer.h"

#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CPlayer.h"
#include "game/CUnit.h"
#include "render/CTextRender.h"

namespace py {

// Ghidra 0x39aaf8
void CSoundMoodAnalyzer::reset() {
    mMood = SOUNDMOOD_TRANQUIL_NEUTRAL;
    mTimer = 1.0f;
    mMoodTranquil = 1.0f;
    mDamageReceived = 0.0f;
    mDamageDealt = 0.0f;
    mUnitsDestroyed = 0.0f;
    mUnitsLost = 0.0f;
    mSectorsLost = 0.0f;
    mSectorsCaptured = 0.0f;
    mTranquility = 0.0f;
    mSectorFlagCapturing = 0.0f;
    mSectorFlagLoosing = 0.0f;
    mMoodAggressive = 0.0f;
    mOwnInfluence = 0.0f;
    mEnemyInfluence = 0.0f;
    mOwnBattleValue = 0.0f;
    mEnemyBattleValue = 0.0f;
    mSubMoodAggressor = 0.5f;
    mSubMoodDefender = 0.5f;
}

// Ghidra 0x399b00 (damage x 0.01; any damage ends the tranquility)
void CSoundMoodAnalyzer::damageReceived(Real amount) {
    const Real v = mDamageReceived;
    mTranquility = 0.0f;
    mDamageReceived = v + amount * bitsf(0x3c23d70a);
}

// Ghidra 0x399b54
void CSoundMoodAnalyzer::damageDealt(Real amount) {
    const Real v = mDamageDealt;
    mTranquility = 0.0f;
    mDamageDealt = v + amount * bitsf(0x3c23d70a);
}

// Ghidra 0x399ba8
void CSoundMoodAnalyzer::unitDestroyed() {
    const Real v = mUnitsDestroyed + 5.0f;
    mTranquility = 0.0f;
    mUnitsDestroyed = v;
}

// Ghidra 0x399be8
void CSoundMoodAnalyzer::unitLost() {
    const Real v = mUnitsLost + 5.0f;
    mTranquility = 0.0f;
    mUnitsLost = v;
}

// Ghidra 0x399c28
void CSoundMoodAnalyzer::sectorLost() {
    const Real v = mSectorsLost + 10.0f;
    mTranquility = 0.0f;
    mSectorsLost = v;
}

// Ghidra 0x399c68 (does not end the tranquility)
void CSoundMoodAnalyzer::sectorCaptured() { mSectorsCaptured = mSectorsCaptured + 10.0f; }

// Ghidra 0x399ca0 (x 0.1)
void CSoundMoodAnalyzer::sectorFlagCapturing(Real v) {
    mSectorFlagCapturing = mSectorFlagCapturing + v * bitsf(0x3dcccccd);
}

// Ghidra 0x399cec
void CSoundMoodAnalyzer::sectorFlagLoosing(Real v) {
    mSectorFlagLoosing = mSectorFlagLoosing + v * bitsf(0x3dcccccd);
}

// Ghidra 0x399d38
void CSoundMoodAnalyzer::update() {
    CEngine* e = mEngine;
    if (!e) return;
    const Real t = mTimer + Real(e->mFrameDt);
    mTimer = t;
    if (t < 1.0f) return;
    CGame* game = e->mGame;
    if (game) {
        // the counters decay by 20 % per second
        const Real k = 1.0f - t * bitsf(0x3e4ccccd);
        mDamageReceived = mDamageReceived * k;
        mDamageDealt = mDamageDealt * k;
        mUnitsDestroyed = mUnitsDestroyed * k;
        mUnitsLost = mUnitsLost * k;
        mSectorsLost = mSectorsLost * k;
        mSectorsCaptured = mSectorsCaptured * k;
        mTranquility = t + mTranquility;
        mSectorFlagCapturing = mSectorFlagCapturing * k;
        mSectorFlagLoosing = mSectorFlagLoosing * k;
        mOwnInfluence = 0.0f;
        mEnemyInfluence = 0.0f;
        if (CPlayer* local = game->getPlayer(e->mLocalPlayer)) {
            for (unsigned id : game->objectList()) {
                CUnit* u = game->getAsUnit(id);
                if (!u) continue;
                Real influence = 0.0f, falloff = 0.0f, range = 0.0f;
                u->getInfluenceAndFalloff(&influence, &falloff, &range);
                if (!(influence > 0.0f)) continue;
                if (local->isFriend(u->mOwner))
                    mOwnInfluence = mOwnInfluence + influence;
                else if (u->mOwner != game->getNeutralPlayer())
                    mEnemyInfluence = mEnemyInfluence + influence;
            }
        }
        const Real own = mOwnInfluence;
        const Real enemy = mEnemyInfluence;
        const Real total = (own + enemy) + 1.0f;
        const Real ownBV = own / total;
        const Real enemyBV = enemy / total;
        const Real dr = mDamageReceived, dd = mDamageDealt, tranq = mTranquility;
        const Real ud = mUnitsDestroyed, ul = mUnitsLost, sl = mSectorsLost, sc = mSectorsCaptured;
        const Real fc = mSectorFlagCapturing, fl = mSectorFlagLoosing;
        const Real action = ((((((dd + dr) + ud) + ul) + sl) + sc) + fc) + fl;
        const Real all = tranq + action;
        const Real moodTranquil = tranq / all;
        const Real moodAggressive = action / all;
        mMoodTranquil = moodTranquil;
        mMoodAggressive = moodAggressive;
        mOwnBattleValue = ownBV;
        mEnemyBattleValue = enemyBV;
        const Real aggressor = ownBV + (fc + (sc + (dd + ud)));
        const Real defender = enemyBV + (fl + (sl + (dr + ul)));
        const Real sum = aggressor + defender;
        // [!] 0/0 without any units, damage or sectors: NaN (no mood matches it)
        const Real subAggressor = aggressor / sum;
        mSubMoodAggressor = subAggressor;
        const Real subDefender = defender / sum;
        mSubMoodDefender = subDefender;
        const float k06 = bitsf(0x3f19999a);   // 0.6
        if (moodTranquil > moodAggressive) {
            if (subAggressor > k06) mMood = SOUNDMOOD_TRANQUIL_AGGRESSIVE;
            else if (subDefender > k06) mMood = SOUNDMOOD_TRANQUIL_DEFENSIVE;
            else mMood = SOUNDMOOD_TRANQUIL_NEUTRAL;
        } else {
            if (subAggressor > k06) mMood = SOUNDMOOD_ACTION_AGGRESSIVE;
            else if (subDefender > k06) mMood = SOUNDMOOD_ACTION_DEFENSIVE;
            else mMood = SOUNDMOOD_ACTION_NEUTRAL;
        }
    } else {
        mMood = SOUNDMOOD_TRANQUIL_NEUTRAL;
    }
    mTimer = 0.0f;
}

// Ghidra 0x39a2bc
void CSoundMoodAnalyzer::drawDebug() {
    if (!mEngine) return;
    CTextRender* tr = mEngine->getTextRender();
    if (!tr) return;
    struct Line {
        float y;
        float r, g, b;
        const char* fmt;
        Real value;
    };
    const Line lines[] = {
        {50.0f, 1, 0, 1, "mDamageReceived: %.3f", mDamageReceived},
        {70.0f, 1, 0, 1, "mDamageDealt: %.3f", mDamageDealt},
        {90.0f, 1, 0, 1, "mUnitsDestroyed: %.3f", mUnitsDestroyed},
        {110.0f, 1, 0, 1, "mUnitsLost: %.3f", mUnitsLost},
        {130.0f, 1, 0, 1, "mSectorsLost: %.3f", mSectorsLost},
        {150.0f, 1, 0, 1, "mSectorsCaptured: %.3f", mSectorsCaptured},
        {170.0f, 1, 0, 1, "mTranquility: %.3f", mTranquility},
        {190.0f, 1, 0, 1, "mSectorFlagCapturing: %.3f", mSectorFlagCapturing},
        {210.0f, 1, 0, 1, "mSectorFlagLoosing: %.3f", mSectorFlagLoosing},
        {230.0f, 1, 0, 1, "mOwnInfluence: %.3f", mOwnInfluence},
        {250.0f, 1, 0, 1, "mEnemyInfluence: %.3f", mEnemyInfluence},
        {270.0f, 1, 0, 1, "mOwnBattleValue: %.3f", mOwnBattleValue},
        {290.0f, 1, 0, 1, "mEnemyBattleValue: %.3f", mEnemyBattleValue},
        {320.0f, 0, 1, 1, "mMoodTranquil: %.3f", mMoodTranquil},
        {340.0f, 1, 1, 0, "mMoodAggressive: %.3f", mMoodAggressive},
        {360.0f, 1, 0, 0, "mSubMoodAggressor: %.3f", mSubMoodAggressor},
        {380.0f, 0, 1, 0, "mSubMoodDefender: %.3f", mSubMoodDefender},
    };
    for (const Line& l : lines)
        tr->drawStringPixel(400.0f, l.y, 20.0f, l.r, l.g, l.b, 1.0f, 0, l.fmt, static_cast<double>(float(l.value)));
    static const struct {
        const char* text;
        float r, g, b;
    } kMood[6] = {
        {"TRANQUIL AGGRESSIVE MOOD", 1.0f, 0.5f, 0.5f}, {"TRANQUIL NEUTRAL MOOD", 0.5f, 0.5f, 0.5f},
        {"TRANQUIL DEFENSIVE MOOD", 0.5f, 1.0f, 0.5f},  {"ACTION AGGRESSIVE MOOD", 1.0f, 0.0f, 0.0f},
        {"ACTION NEUTRAL MOOD", 1.0f, 1.0f, 1.0f},      {"ACTION DEFENSIVE MOOD", 0.0f, 1.0f, 0.0f},
    };
    if (mMood >= 0 && mMood < 6)
        tr->drawStringPixel(400.0f, 430.0f, 30.0f, kMood[mMood].r, kMood[mMood].g, kMood[mMood].b, 1.0f, 0, "%s",
                            kMood[mMood].text);
    tr->draw();
}

}  // namespace py
