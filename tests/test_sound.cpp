// Phase 8 tests: the Cricket stand-in (audio/Ck.h), CSoundMoodAnalyzer and
// CSoundManager.
//
// Expected values: the 3D gains come from an independent float32 Python
// reimplementation of Cricket's AttenParams::set / Sound::update3d (ELF
// 0x3eac94 / 0x3eb080, numpy, op by op); the ADPCM values from the RE
// package's tools/ckdec.py (the decoder that reproduces the reference WAVs);
// the mood values by hand from the decompile's formulas. Tests that need the
// game data skip themselves when data/ is missing (CI).
#include <cmath>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "audio/Ck.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"
#include "game/CGame.h"
#include "game/CSoundManager.h"
#include "game/CSoundMoodAnalyzer.h"
#include "test_engine_reset.h"
#include "test_main.h"
#include "ui/CUISound.h"

using namespace py;

namespace {

bool haveData(const char* file) {
    if (CMemoryFile::resolve(file).empty()) {
        std::printf("    (skipped: %s not found - run tools/extract_assets.py)\n", file);
        return false;
    }
    return true;
}

bool approx(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

uint32_t crc32(const void* data, size_t n) {
    uint32_t c = 0xffffffffu;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return c ^ 0xffffffffu;
}

// a bank with one PCM16 sound (format 0) of `frames` frames of `value`
std::vector<uint8_t> makeBank(const char* name, int channels, int rate, int frames, int16_t value) {
    std::vector<uint8_t> b(0x48 + 72, 0);
    std::memcpy(b.data(), "ckmk", 4);
    b[0x38] = 1;
    uint8_t* e = b.data() + 0x48;
    std::strncpy(reinterpret_cast<char*>(e), name, 31);
    e[32] = 0;   // PCM16
    e[33] = static_cast<uint8_t>(channels);
    e[34] = static_cast<uint8_t>(rate & 0xff);
    e[35] = static_cast<uint8_t>(rate >> 8);
    const unsigned blocks = static_cast<unsigned>(frames);
    std::memcpy(e + 36, &blocks, 4);
    const uint16_t bb = static_cast<uint16_t>(2 * channels);
    std::memcpy(e + 40, &bb, 2);
    for (int i = 0; i < frames * channels; ++i) {
        b.push_back(static_cast<uint8_t>(value & 0xff));
        b.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    }
    return b;
}

float gClock = 0.0f;
float testClock() { return gClock; }

// the Ck system with a fresh master mixer, the test clock, no output
void freshCk() {
    CkShutdown();
    CkInit();
    ck::setOutput(0);
    ck::setClock(&testClock);
    gClock = 0.0f;
    CkUpdate();   // starts the clock
}

void endCk() {
    CkShutdown();
    ck::setOutput(0);
    ck::setClock(&CUtils::getTimer);
}

void advance(float ms, float stepMs = 50.0f) {
    for (float t = 0.0f; t < ms; t += stepMs) {
        gClock += stepMs;
        CkUpdate();
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Cricket: attenuation, pan, Doppler

TEST(ck_atten_params) {
    CkAttenParams a;
    a.set(kCkAttenuationMode_InvDistanceSquared, 200.0f, 600.0f, 0.0f);   // CSoundManager's setting
    CHECK(fbits(a.nearDist) == 0x43480000u && fbits(a.farDist) == 0x44160000u && fbits(a.farVol) == 0u);
    CHECK(fbits(a.invScale) == 0x472fc800u);    // 45000
    CHECK(fbits(a.invOffset) == 0xbe000000u);   // -0.125: 45000 / 200^2 - 0.125 = 1
    CkAttenParams b;
    b.set(kCkAttenuationMode_InvDistance, 200.0f, 600.0f, 0.0f);
    CHECK(fbits(b.invScale) == 0x43960000u && fbits(b.invOffset) == 0xbf000000u);   // 300 / d - 0.5
    CkAttenParams c;
    c.set(kCkAttenuationMode_Linear, -5.0f, 0.0f, 3.0f);   // clamps: near >= 1e-12, far >= near + 1e-12, farVol <= 1
    CHECK(fbits(c.nearDist) == 0x2b8cbcccu && c.farDist > c.nearDist && c.farVol == 1.0f);
    CHECK(c.invScale == 0.0f && c.invOffset == 0.0f);
}

TEST(ck_update3d_formulas) {
    // listener 500 above the origin looking down, up = +y (so +x is right)
    const float L[3] = {0, 0, 500}, look[3] = {0, 0, 0}, up[3] = {0, 1, 0}, zero[3] = {0, 0, 0};
    CkAttenParams a3, a1, a2;
    a3.set(3, 200, 600, 0);
    a1.set(1, 200, 600, 0.25f);
    a2.set(2, 200, 600, 0);
    struct Case {
        const char* name;
        const CkAttenParams* a;
        float pos[3], vel[3];
        uint32_t vol, pan, speed;
    } cases[] = {
        {"below", &a3, {0, 0, 0}, {0, 0, 0}, 0x3d6147b4u, 0x00000000u, 0x3f800000u},
        {"right", &a3, {300, 0, 0}, {0, 0, 0}, 0x3bf0f0e0u, 0x3f03b5feu, 0x3f800000u},
        {"leftnear", &a3, {-150, 30, 400}, {0, 0, 0}, 0x3f800000u, 0xbf40998du, 0x3f800000u},
        {"beyond", &a3, {0, 0, -200}, {0, 0, 0}, 0x00000000u, 0x00000000u, 0x3f800000u},
        {"linear", &a1, {300, 100, 200}, {0, 0, 0}, 0x3f0ec5dau, 0x3f3504f3u, 0x3f800000u},
        {"invdist", &a2, {-250, -100, 300}, {0, 0, 0}, 0x3ec9f25eu, 0xbf47e705u, 0x3f800000u},
        // [!] Doppler only beyond 0.99 c (Cricket's max instead of min)
        {"doppler", &a3, {300, 0, 0}, {900, 0, -1500}, 0x3bf0f0e0u, 0x3f03b5feu, 0x3ea71cb1u},
    };
    for (const Case& c : cases) {
        const CkSound::Result3d r = CkSound::compute3d(c.pos, c.vel, L, look, up, zero, *c.a, 343.2f);
        CHECK_MSG(fbits(r.volume) == c.vol, "%s volume %08x", c.name, fbits(r.volume));
        CHECK_MSG(fbits(r.pan) == c.pan, "%s pan %08x", c.name, fbits(r.pan));
        CHECK_MSG(fbits(r.speed) == c.speed, "%s speed %08x", c.name, fbits(r.speed));
        CHECK_MSG(r.isVirtual == (r.volume < 0.005f), "%s virtual", c.name);
    }
    // a slow bomber (180 m/s) has no Doppler
    const float pos[3] = {300, 0, 0}, vel[3] = {-180, 0, 0};
    CHECK(CkSound::compute3d(pos, vel, L, look, up, zero, a3, 343.2f).speed == 1.0f);
}

TEST(ck_pan_matrices) {
    const CkVolumeMatrix m = CkVolumeMatrix::pan(0.0f);   // mono centre: cos/sin(pi/4)
    CHECK(approx(m.ll, 0.70710677f, 1e-6f) && approx(m.rr, 0.70710677f, 1e-6f) && m.lr == 0.0f && m.rl == 0.0f);
    const CkVolumeMatrix r = CkVolumeMatrix::pan(1.0f);
    CHECK(approx(r.ll, 0.0f, 1e-6f) && approx(r.rr, 1.0f, 1e-6f));
    const CkVolumeMatrix l = CkVolumeMatrix::pan(-2.0f);   // below -1: angle 0
    CHECK(l.ll == 1.0f && l.rr == 0.0f);
    const CkVolumeMatrix s = CkVolumeMatrix::stereoPan(0.0f);   // stereo: halved
    CHECK(s.ll == 0.5f && s.lr == 0.0f && s.rl == 0.0f && s.rr == 0.5f);
    const CkVolumeMatrix sl = CkVolumeMatrix::stereoPan(-1.0f);   // right channel folded into the left
    CHECK(sl.ll == 0.5f && approx(sl.lr, 0.5f, 1e-6f) && sl.rl == 0.0f && approx(sl.rr, 0.0f, 1e-6f));
    const CkVolumeMatrix sr = CkVolumeMatrix::stereoPan(0.5f);
    CHECK(approx(sr.ll, 0.5f * 0.70710677f, 1e-6f) && approx(sr.rl, 0.5f * 0.70710677f, 1e-6f) && sr.rr == 0.5f);
}

// ---------------------------------------------------------------------------
// Cricket: the bank

TEST(ck_adpcm_block) {
    // predictor 3, delta 300, samples -1200 (first) and 2500, then 12 nibbles
    const uint8_t blk[] = {0x03, 0x2c, 0x01, 0x50, 0xfb, 0xc4, 0x09, 0x1f, 0x80, 0x7a, 0xc4, 0x09, 0xff};
    std::vector<int16_t> out;
    CkBank::decodeAdpcmBlock(blk, sizeof(blk), out);
    const int16_t expect[] = {-1200, 2500, 1275, 1850, 1706, 14, -3451, 6487, 16434, -961, -27896, -21163, -32476, -32768};
    CHECK(out.size() == sizeof(expect) / sizeof(expect[0]));
    for (size_t i = 0; i < out.size() && i < 14; ++i) CHECK_MSG(out[i] == expect[i], "sample %d: %d", static_cast<int>(i), out[i]);
}

TEST(ck_bank_game_ckb) {
    if (!haveData("game.ckb")) return;
    CkBank* b = CkBank::newBank(CMemoryFile::resolve("game.ckb").c_str());
    CHECK(b->isLoaded() && b->getNumSounds() == 49);
    struct E {
        const char* name;
        int ch, rate, frames;
        uint32_t crc;
    } es[] = {
        {"txtroger", 1, 44100, 27360, 0x72c80217u},
        {"click", 2, 44100, 16128, 0x954e5ce3u},
        {"shooting1", 1, 44100, 62028, 0xb2b93daau},
        {"explosion", 2, 44100, 178956, 0x81a91fd9u},
    };
    for (const E& e : es) {
        const CkSample* s = b->findSample(e.name);
        CHECK_MSG(s != nullptr, "%s missing", e.name);
        if (!s) continue;
        CHECK_MSG(s->channels == e.ch && s->sampleRate == e.rate && s->frames() == e.frames, "%s format", e.name);
        CHECK_MSG(crc32(s->pcm.data(), s->pcm.size() * 2) == e.crc, "%s crc %08x", e.name,
                  crc32(s->pcm.data(), s->pcm.size() * 2));
    }
    b->destroy();
}

// ---------------------------------------------------------------------------
// Cricket: playing, time, mixing

TEST(ck_time_without_output) {
    freshCk();
    const std::vector<uint8_t> bank = makeBank("dc", 1, 44100, 4410, 16384);   // 100 ms
    CkBank* b = CkBank::newBankFromMemory(bank.data(), bank.size());
    CHECK(b->isLoaded());
    CkSound* s = CkSound::newBankSound(b, "dc");
    CHECK(s && s->isReady() && !s->isPlaying() && approx(s->getLengthMs(), 100.0f, 1e-3f));
    CHECK(CkSound::newBankSound(b, "nope") == nullptr);
    s->play();
    advance(50.0f);
    CHECK(s->isPlaying() && approx(s->getPlayPositionMs(), 50.0f, 0.1f));
    advance(100.0f);
    CHECK(!s->isPlaying());
    // loop forever
    s->setLoopCount(-1);
    s->play();
    advance(1000.0f);
    CHECK(s->isPlaying() && s->getCurrentLoop() >= 9);
    // one more loop, then the next sound starts by itself
    CkSound* n = CkSound::newBankSound(b, "dc");
    s->setLoopCount(1);
    s->play();
    s->setNextSound(n);
    advance(150.0f);
    CHECK(s->isPlaying() && s->getCurrentLoop() == 1);
    advance(100.0f);
    CHECK(!s->isPlaying() && n->isPlaying());
    // stop() does not start the next sound
    s->play();
    n->stop();
    s->stop();
    CHECK(!n->isPlaying());
    // suspended: no time
    s->play();
    CkSuspend();
    advance(500.0f);
    CHECK(s->isPlaying() && s->getPlayPosition() == 0);
    CkResume();
    s->destroy();
    n->destroy();
    b->destroy();
    endCk();
}

TEST(ck_mix_gains) {
    freshCk();
    const std::vector<uint8_t> bank = makeBank("dc", 1, 22050, 22050, 16384);   // 0.5, 1 s at 22.05 kHz
    CkBank* b = CkBank::newBankFromMemory(bank.data(), bank.size());
    CkMixer* sfx = CkMixer::newMixer("SFX", CkMixer::getMaster());
    sfx->setVolume(0.5f);
    CkMixer::getMaster()->setVolume(0.8f);
    CHECK(approx(sfx->getMixedVolume(), 0.4f, 1e-7f));
    CkSound* s = CkSound::newBankSound(b, "dc");
    s->setMixer(sfx);
    s->setVolume(0.5f);
    s->play();
    ck::setOutput(44100);
    std::vector<float> buf(2 * 512);
    ck::render(buf.data(), 512);
    // 0.5 (sample) x 0.5 (sound) x 0.5 (SFX) x 0.8 (master) x cos(pi/4)
    const float expect = 0.5f * 0.5f * 0.4f * 0.70710677f;
    CHECK(approx(buf[0], expect, 1e-5f) && approx(buf[1], expect, 1e-5f) && approx(buf[1022], expect, 1e-5f));
    CHECK(approx(s->getPlayPositionMs(), 512.0f * 1000.0f / 44100.0f, 0.1f));   // resampled 22.05 -> 44.1 kHz
    // 3D: the "right" case of ck_update3d_formulas
    CkSound::set3dListenerPosition(0, 0, 500, 0, 0, 0, 0, 2, 0);   // up is normalised
    CkSound::set3dAttenuation(3, 200, 600, 0);
    s->set3dPosition(300, 0, 0);
    s->set3dEnabled(true);
    CHECK(fbits(s->get3dVolume()) == 0x3bf0f0e0u && fbits(s->get3dPan()) == 0x3f03b5feu);
    for (int i = 0; i < 4; ++i) ck::render(buf.data(), 512);   // past the 40 ms volume ramp
    const float v = 0.5f * 0.5f * 0.4f * bitsf(0x3bf0f0e0u);
    const float ang = (bitsf(0x3f03b5feu) + 1.0f) * 0.78539819f;
    CHECK(approx(buf[1022], v * std::cos(ang), 1e-6f) && approx(buf[1023], v * std::sin(ang), 1e-6f));
    CHECK(buf[1023] > buf[1022]);   // louder on the right
    // a virtual sound (beyond the far distance) is not mixed but keeps time
    s->set3dPosition(0, 0, -500);
    CkUpdate();
    CHECK(s->isVirtual());
    const int before = s->getPlayPosition();
    ck::render(buf.data(), 512);
    CHECK(buf[0] == 0.0f && buf[1023] == 0.0f && s->getPlayPosition() > before);
    // clipping
    s->set3dEnabled(false);
    s->setVolume(100.0f);
    ck::render(buf.data(), 512);
    CHECK(buf[1022] == 1.0f);
    s->destroy();
    b->destroy();
    endCk();
}

// ---------------------------------------------------------------------------
// CSoundMoodAnalyzer

TEST(sound_mood_transitions) {
    CEngine* e = resetTestEngine();
    CSoundMoodAnalyzer m;
    m.mEngine = e;
    m.reset();
    CHECK(m.mMood == SOUNDMOOD_TRANQUIL_NEUTRAL && m.mTimer == 1.0f && m.mSubMoodAggressor == 0.5f);
    // no game: the mood is "tranquil neutral" once a second
    e->mGame = nullptr;
    e->mFrameDt = 0.5f;
    m.mMood = SOUNDMOOD_ACTION_AGGRESSIVE;
    m.update();   // timer 1.5 >= 1
    CHECK(m.mMood == SOUNDMOOD_TRANQUIL_NEUTRAL && m.mTimer == 0.0f);
    m.update();   // 0.5: nothing
    CHECK(m.mTimer == 0.5f);

    CGame game;   // an empty game (no players): only the counters decide
    e->mGame = &game;
    e->mFrameDt = 0.0f;
    // nothing happened: tranquil, 0/0 sub-moods (NaN) -> neutral
    m.reset();
    m.update();
    CHECK(m.mMood == SOUNDMOOD_TRANQUIL_NEUTRAL && m.mTranquility == 1.0f && m.mMoodTranquil == 1.0f);
    CHECK(m.mSubMoodAggressor != m.mSubMoodAggressor);   // NaN
    // damage dealt: 1000 x 0.01 = 10, decayed by 1 - 0.2 x 1 -> 8; tranquility reset
    m.reset();
    m.damageDealt(1000.0f);
    CHECK(m.mDamageDealt == 10.0f && m.mTranquility == 0.0f);
    m.update();
    CHECK(m.mDamageDealt == 8.0f && m.mTranquility == 1.0f);
    CHECK(m.mMoodAggressive == 8.0f / 9.0f && m.mSubMoodAggressor == 1.0f);
    CHECK(m.mMood == SOUNDMOOD_ACTION_AGGRESSIVE);
    // damage received / units lost: defensive action
    m.reset();
    m.damageReceived(500.0f);
    m.unitLost();
    m.update();
    CHECK(m.mMood == SOUNDMOOD_ACTION_DEFENSIVE && m.mSubMoodDefender == 1.0f);
    // both sides: neutral action
    m.reset();
    m.unitDestroyed();
    m.unitLost();
    m.update();
    CHECK(m.mMood == SOUNDMOOD_ACTION_NEUTRAL && m.mSubMoodAggressor == 0.5f);
    // a long calm with a captured sector (does not end the calm): tranquil aggressive
    m.reset();
    m.mTranquility = 100.0f;
    m.sectorCaptured();
    m.update();
    CHECK(m.mTranquility == 101.0f && m.mSectorsCaptured == 8.0f);
    CHECK(m.mMood == SOUNDMOOD_TRANQUIL_AGGRESSIVE);
    // a calm while a flag is being lost: tranquil defensive
    m.reset();
    m.mTranquility = 100.0f;
    m.sectorFlagLoosing(100.0f);   // x 0.1
    CHECK(m.mSectorFlagLoosing == 10.0f);
    m.update();
    CHECK(m.mMood == SOUNDMOOD_TRANQUIL_DEFENSIVE);
    // the counters fade: after enough calm seconds the action mood ends
    m.reset();
    m.damageDealt(1000.0f);
    e->mFrameDt = 1.0f;   // timer 2 per update: k = 0.6
    int mood = -1, updates = 0;
    for (; updates < 20 && mood != SOUNDMOOD_TRANQUIL_AGGRESSIVE; ++updates) {
        m.update();
        mood = m.mMood;
    }
    CHECK(mood == SOUNDMOOD_TRANQUIL_AGGRESSIVE && updates > 1);
    e->mGame = nullptr;
}

// ---------------------------------------------------------------------------
// CSoundManager

TEST(sound_manager_volumes_and_gating) {
    resetTestEngine();
    freshCk();
    CSoundManager* sm = CSoundManager::getSingletonPtr();
    CHECK(CSoundManager::get() == sm && sm->mInitialized && !sm->mAllVolumeZero);
    CHECK(sm->getMixerSFXVolume() == 100u && sm->getMixerMusicVolume() == 40u && sm->getMixerMasterVolume() == 100u);
    CHECK(approx(sm->mMixerMusic->getVolume(), CSoundManager::musicMixerGain(0.4f), 1e-6f) && sm->mMixerSFX->getVolume() == 1.0f);
    CHECK(CkMixer::getMaster()->getVolume() == 1.0f);
    sm->setMixerMasterVolume(50);
    CHECK(CkMixer::getMaster()->getVolume() == 0.5f && approx(sm->mMixerSFX->getMixedVolume(), 0.5f, 0.0f));
    // one volume 0: still on
    sm->setMixerSFXVolume(0);
    CHECK(!sm->mAllVolumeZero && sm->mMixerSFX->getVolume() == 0.0f);
    // both 0: off; nothing plays and playRandomSound draws nothing
    sm->setMixerMusicVolume(0);
    CHECK(sm->mAllVolumeZero);
    const uint32_t w = CRng::mW, z = CRng::mZ;
    CHECK(sm->playSound("click", SOUNDTYPE_BANK, false, 1.0f, true) == 0u);
    CHECK(sm->playRandomSound(RANDOMSOUND_ACKNOWLEDGE) == 0u && CRng::mW == w && CRng::mZ == z);
    CHECK(sm->getSound(1) == nullptr);
    // on again
    sm->setMixerMusicVolume(70);
    CHECK(!sm->mAllVolumeZero && approx(sm->mMixerMusic->getVolume(), CSoundManager::musicMixerGain(0.7f), 1e-6f));
    CSoundManager::release();
    CHECK(CSoundManager::get() == nullptr);
    endCk();
}

TEST(sound_manager_engine_options) {
    CEngine* e = resetTestEngine();
    // no sound manager: the setters only keep the values
    e->setMusicVolume(20);
    e->setSFXVolume(30);
    CHECK(e->mMusicVolume == 20u && e->mSFXVolume == 30u && e->getSoundManager() == nullptr);
    uisound::play("click");   // no manager: nothing happens
    uisound::playRandom(0);
    freshCk();
    e->initSound();
    CSoundManager* sm = e->getSoundManager();
    CHECK(sm && sm->getMixerMusicVolume() == 20u && sm->getMixerSFXVolume() == 30u);
    e->setMasterVolume(60);
    CHECK(sm->getMixerMasterVolume() == 60u && e->mMasterVolume == 60u);
    e->releaseSound();
    CHECK(e->getSoundManager() == nullptr && CSoundManager::get() == nullptr);
    endCk();
}

TEST(sound_manager_play_and_free) {
    if (!haveData("game.ckb")) return;
    resetTestEngine();
    freshCk();
    CSoundManager* sm = CSoundManager::getSingletonPtr();
    const unsigned first = CSoundManager::mNextSoundSourceID;
    const unsigned id = sm->playSound("click", SOUNDTYPE_BANK, false, 1.0f, true);
    CHECK(id == first && CSoundManager::mNextSoundSourceID == first + 1);
    CSound* s = sm->getSound(id);
    CHECK(s && s->type == SOUNDTYPE_BANK && s->sound->isPlaying() && s->sound->getMixer() == sm->mMixerSFX);
    // an unknown name fails but uses up an id
    CHECK(sm->playSound("nosuchsound", SOUNDTYPE_BANK, false, 1.0f, true) == 0u);
    CHECK(CSoundManager::mNextSoundSourceID == first + 2);
    // 3D, looping: positioned, kept
    const unsigned loop = sm->playSound("staticself", SOUNDTYPE_BANK, CVector3(10, 20, 30), CVector3::ZERO, true,
                                        0.7f, true);
    CSound* l = sm->getSound(loop);
    CHECK(l && l->sound->is3dEnabled() && l->sound->getLoopCount() == -1 && l->sound->getVolume() == 0.7f);
    sm->setSoundVolume(loop, 0.3f);
    CHECK(l->sound->getVolume() == 0.3f);
    // the click (0.37 s) ends and update() frees it; the loop stays
    advance(500.0f);
    sm->update();
    CHECK(sm->getSound(id) == nullptr && sm->getSound(loop) != nullptr);
    sm->stopSound(loop);
    CHECK(sm->getSound(loop) == nullptr);
    // clearSFX(false) keeps the music tracks
    sm->playSound("click", SOUNDTYPE_BANK, false, 1.0f, true);
    const size_t tracks = sm->mBGMTracks.size();
    sm->clearSFX(false);
    CHECK(sm->mSounds.size() == tracks);
    CSoundManager::release();
    endCk();
}

TEST(sound_manager_random_acknowledgements) {
    if (!haveData("game.ckb")) return;
    resetTestEngine();
    freshCk();
    CSoundManager* sm = CSoundManager::getSingletonPtr();
    static const char* kAck[5] = {"txtroger", "txtaffirmative", "txtok", "txtyessir", "txtonmyway"};
    static const char* kAttack[3] = {"txtengage", "txtattacking", "txtapproachingtarget"};
    CRng::SetSeed(0x1234, 0x5678);
    for (int i = 0; i < 20; ++i) {
        const int kind = i % 2;
        // the expected choice from a copy of the RNG state: exactly one draw
        const uint32_t w = CRng::mW, z = CRng::mZ;
        const float u = CRng::GetUniform();
        const uint32_t w1 = CRng::mW, z1 = CRng::mZ;
        CRng::mW = w;
        CRng::mZ = z;
        const int pick = f2i(u * (kind == 0 ? 5.0f : 3.0f));
        const unsigned id = sm->playRandomSound(kind);
        CHECK(CRng::mW == w1 && CRng::mZ == z1);
        CSound* s = sm->getSound(id);
        CHECK(s != nullptr);
        if (s) CHECK(s->sound->getName() == (kind == 0 ? kAck[pick] : kAttack[pick]));
    }
    // other kinds: no draw, no sound
    const uint32_t w = CRng::mW;
    CHECK(sm->playRandomSound(2) == 0u && CRng::mW == w);
    CSoundManager::release();
    endCk();
}

TEST(sound_manager_music) {
    if (!haveData("tn_1.ogg") || !haveData("ad_10.ogg")) return;
    CEngine* e = resetTestEngine();
    freshCk();
    CSoundManager* sm = CSoundManager::getSingletonPtr();
    CHECK(sm->mBGMTracks.size() == 60u);
    // the order: ta tn td aa an ad for 1..10; each stream keeps volume 0.4
    CHECK(sm->mBGMTracks[1]->mood == SOUNDMOOD_TRANQUIL_NEUTRAL && sm->mBGMTracks[59]->mood == SOUNDMOOD_ACTION_DEFENSIVE);
    CHECK(sm->mBGMTracks[0]->sound->getVolume() == 0.4f && sm->mBGMTracks[0]->sound->getMixer() == sm->mMixerMusic);
    auto expectedPick = [&](int mood) {   // one draw, as getRandomSoundForMood
        const uint32_t w = CRng::mW, z = CRng::mZ;
        const float u = CRng::GetUniform();
        CRng::mW = w;
        CRng::mZ = z;
        unsigned r = f2u(u * 10.0f);
        if (r > 9u) r = 9u;
        return sm->mBGMTracks[static_cast<size_t>(mood) + 6u * r];
    };
    CRng::SetSeed(0x4321, 0x8765);
    e->mGame = nullptr;
    CSound* want = expectedPick(SOUNDMOOD_TRANQUIL_NEUTRAL);
    sm->startBackgroundMusic();
    CHECK(sm->mCurrentBGM == want->id && want->sound->isPlaying());
    // 5 s before the end the next track (of the then mood) is queued, once
    sm->mMoodAnalyzer.mMood = SOUNDMOOD_ACTION_AGGRESSIVE;
    CkSound* cur = want->sound;
    const float len = cur->getLengthMs();
    CHECK(len > 10000.0f);
    advance(len - 6000.0f, 100.0f);
    sm->updateBackgroundMusic();
    CHECK(!sm->mNextBGMQueued);
    advance(1500.0f, 100.0f);
    CSound* next = expectedPick(SOUNDMOOD_ACTION_AGGRESSIVE);
    const uint32_t w = CRng::mW;
    sm->updateBackgroundMusic();
    CHECK(sm->mNextBGMQueued && sm->mNextBGM == next->id && CRng::mW != w);
    const uint32_t w2 = CRng::mW;
    sm->updateBackgroundMusic();   // no second draw
    CHECK(CRng::mW == w2);
    CHECK(cur->getNextSound() == next->sound);
    // the track ends, the next one plays and becomes current
    advance(5000.0f, 100.0f);
    CHECK(!cur->isPlaying() && next->sound->isPlaying());
    sm->updateBackgroundMusic();
    CHECK(sm->mCurrentBGM == next->id && sm->mNextBGM == 0u && !sm->mNextBGMQueued);
    // the same track drawn again: one more loop instead
    sm->mMoodAnalyzer.mMood = SOUNDMOOD_ACTION_AGGRESSIVE;
    CkSound* s2 = next->sound;
    int guard = 0;
    while (guard++ < 200) {   // seed until the next draw repeats the current track
        CRng::SetSeed(0x9999, static_cast<uint32_t>(0x1000 + guard * 7919));
        if (expectedPick(SOUNDMOOD_ACTION_AGGRESSIVE) == next) break;
    }
    CHECK(guard < 200);
    advance(s2->getLengthMs() - 4500.0f - s2->getPlayPositionMs(), 100.0f);
    sm->updateBackgroundMusic();
    CHECK(sm->mNextBGM == next->id && s2->getLoopCount() == 1);
    advance(5000.0f, 100.0f);   // wrapped into the extra loop
    CHECK(s2->isPlaying() && s2->getCurrentLoop() == 1);
    sm->updateBackgroundMusic();
    CHECK(sm->mCurrentBGM == next->id && !sm->mNextBGMQueued);
    // stopBackgroundMusic
    sm->stopBackgroundMusic();
    CHECK(sm->mCurrentBGM == 0u && !s2->isPlaying());
    CSoundManager::release();
    endCk();
}

// ---------------------------------------------------------------------------
// every sound name the code uses exists in the data (tools/check_sounds.py
// prints the same list)

#ifndef PROJECTY_SOURCE_DIR
#define PROJECTY_SOURCE_DIR "."
#endif

TEST(sound_names_resolve) {
    if (!haveData("game.ckb")) return;
    std::set<std::string> names;
    const std::string src = PROJECTY_SOURCE_DIR;
    static const char* kFiles[] = {
        "src/game/CBomber.cpp", "src/game/CUnit.cpp", "src/game/CUnitSpotter.cpp",
        "src/game/CProjectileLaserBlastA.cpp", "src/game/CProjectileLaserBlastB.cpp",
        "src/game/CProjectileRocket.cpp", "src/game/CProjectileSniperBullet.cpp", "src/game/CSoundManager.cpp",
        "src/ui/CEventBar.cpp", "src/ui/CInterfaceIngame.cpp", "src/ui/CInterfaceIngamePerPlayer.cpp",
        "src/ui/CInterfaceMenu.cpp",
    };
    const std::regex call(R"re((playSound|uisound::play)\(\s*"([a-z0-9_]+)")re");
    const std::regex ternary(R"re(uisound::play\([^;]*\?\s*"([a-z0-9_]+)"\s*:\s*"([a-z0-9_]+)")re");
    const std::regex table(R"re(\{[^{}]*,\s*"([a-z0-9_]+)"\},)re");
    int files = 0;
    for (const char* f : kFiles) {
        std::ifstream in(src + "/" + f);
        if (!in) continue;
        ++files;
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();
        for (std::sregex_iterator it(text.begin(), text.end(), call), end; it != end; ++it) names.insert((*it)[2]);
        for (std::sregex_iterator it(text.begin(), text.end(), ternary), end; it != end; ++it) {
            names.insert((*it)[1]);
            names.insert((*it)[2]);
        }
        if (std::strstr(f, "CEventBar"))
            for (std::sregex_iterator it(text.begin(), text.end(), table), end; it != end; ++it) names.insert((*it)[1]);
    }
    if (files == 0) {
        std::printf("    (skipped: sources not found at %s)\n", src.c_str());
        return;
    }
    // the weapons' configured sounds (config.txt <Weapon>SoundSourceName of the weapons that play one)
    for (const char* w : {"shooting1", "shooting2"}) names.insert(w);
    CHECK(names.size() >= 44u);
    CkBank* b = CkBank::newBank(CMemoryFile::resolve("game.ckb").c_str());
    int missing = 0;
    for (const std::string& n : names)
        if (!b->findSample(n.c_str())) {
            std::printf("    missing sound: %s\n", n.c_str());
            ++missing;
        }
    CHECK(missing == 0);
    b->destroy();
    // the music tracks
    static const char* kMood[6] = {"ta", "tn", "td", "aa", "an", "ad"};
    for (int i = 1; i <= 10; ++i)
        for (const char* m : kMood) {
            const std::string f = std::string(m) + "_" + std::to_string(i) + ".ogg";
            CHECK_MSG(!CMemoryFile::resolve(f).empty(), "missing music %s", f.c_str());
        }
}

// [PC] the quieter music curve: half the original's gain (x 3 linear) at the
// default volume 40, a quarter at 10, 0 at 0 (values by hand: 2.37 v^1.5).
TEST(sound_music_volume_curve) {
    CHECK(CSoundManager::musicMixerGain(0.0f) == 0.0f);
    CHECK(approx(CSoundManager::musicMixerGain(0.4f), 0.5995f, 1e-3f));   // 0.5 x 1.2
    CHECK(approx(CSoundManager::musicMixerGain(0.1f), 0.07495f, 1e-4f));  // 0.25 x 0.3
    CHECK(approx(CSoundManager::musicMixerGain(1.0f), 2.37f, 1e-6f));
    for (int v = 1; v <= 100; ++v)   // never louder than the original
        CHECK(CSoundManager::musicMixerGain(v / 100.0f) <= v / 100.0f * 3.0f);
}
