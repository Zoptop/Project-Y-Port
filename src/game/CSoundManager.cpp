// CSoundManager.cpp - see CSoundManager.h.
#include "game/CSoundManager.h"

#include <cstdio>
#include <string>

#include "audio/Ck.h"
#include "core/CMemoryFile.h"
#include "core/CRng.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "game/CEngine.h"

namespace py {

unsigned CSoundManager::mNextSoundSourceID = 1;
bool CSoundManager::sLogPlays = false;
CSoundManager* CSoundManager::mSingleton = nullptr;

// Ghidra 0x39bc20
CSoundManager::CSoundManager() {
    mSounds.max_load_factor(1.0f);
    mInitialized = false;
    mUnknown8c = 0;
    mNextBGMQueued = false;
    mCurrentBGM = 0;
    mNextBGM = 0;
    mEngine = CEngine::getSingletonPtr();
    mMoodAnalyzer.mEngine = mEngine;
    mMoodAnalyzer.reset();
    mMixerSFX = CkMixer::newMixer("SFX", CkMixer::getMaster());
    mMixerMusic = CkMixer::newMixer("Music", CkMixer::getMaster());
    setMixerMasterVolume(100);
    setMixerMusicVolume(0x28);
    setMixerSFXVolume(100);
    mAllVolumeZero = false;
    CkSound::set3dListenerVelocity(0.0f, 0.0f, 0.0f);
    CkSound::set3dAttenuation(kCkAttenuationMode_InvDistanceSquared, 200.0f, 600.0f, 0.0f);
    CkSound::set3dSoundSpeed(343.2f);
    const std::string bank = CMemoryFile::resolve("game.ckb");
    mBank = CkBank::newBank(bank.c_str());
    // ten tracks per mood, in this order (getRandomSoundForMood counts in it)
    char name[20];
    for (int i = 1; i != 11; ++i) {
        std::snprintf(name, sizeof(name), "ta_%i", i);
        addBGMTrack(name, SOUNDMOOD_TRANQUIL_AGGRESSIVE);
        std::snprintf(name, sizeof(name), "tn_%i", i);
        addBGMTrack(name, SOUNDMOOD_TRANQUIL_NEUTRAL);
        std::snprintf(name, sizeof(name), "td_%i", i);
        addBGMTrack(name, SOUNDMOOD_TRANQUIL_DEFENSIVE);
        std::snprintf(name, sizeof(name), "aa_%i", i);
        addBGMTrack(name, SOUNDMOOD_ACTION_AGGRESSIVE);
        std::snprintf(name, sizeof(name), "an_%i", i);
        addBGMTrack(name, SOUNDMOOD_ACTION_NEUTRAL);
        std::snprintf(name, sizeof(name), "ad_%i", i);
        addBGMTrack(name, SOUNDMOOD_ACTION_DEFENSIVE);
    }
    mInitialized = true;
    if (!mSingleton) mSingleton = this;
}

// Ghidra 0x39b89c
CSoundManager::~CSoundManager() {
    CUtils::log("sound manager shutdown");
    clearSFX(false);
    // [PC] the music tracks too (the original leaks them at shutdown)
    clearSFX(true);
    mBGMTracks.clear();
    if (mMixerSFX) mMixerSFX->destroy();
    if (mMixerMusic) mMixerMusic->destroy();
    if (mBank) mBank->destroy();
    mMixerSFX = mMixerMusic = nullptr;
    mBank = nullptr;
    CkShutdown();
    if (mSingleton == this) mSingleton = nullptr;
}

// Ghidra 0x39bf58
CSoundManager* CSoundManager::getSingletonPtr() {
    if (mSingleton) return mSingleton;
    mSingleton = new CSoundManager();
    return mSingleton;
}

// Ghidra 0x39b978
void CSoundManager::release() {
    if (CSoundManager* s = mSingleton) delete s;
    mSingleton = nullptr;
}

// Ghidra 0x399964
void CSoundManager::setMixerMasterVolume(unsigned v) {
    mMasterVolume = v;
    const float f = u2f(v) / 100.0f;
    CUtils::log("setMixerMasterVolume %f", static_cast<double>(f));
    CkMixer::getMaster()->setVolume(f);
}

// Ghidra 0x39b9b8. Turning both volumes to 0 stops the music and the
// effects; turning one back on restarts the music.
void CSoundManager::setMixerSFXVolume(unsigned v) {
    mSFXVolume = v;
    const bool wasZero = mAllVolumeZero;
    mAllVolumeZero = v == 0 && mMusicVolume == 0;
    CUtils::log("mAllVolumeZero: %i", mAllVolumeZero ? 1 : 0);
    if (!mAllVolumeZero) {
        if (wasZero) {
            CUtils::log("Enable sound");
            if (mInitialized) startBackgroundMusic();
            setMixerMasterVolume(mMasterVolume);
        }
    } else if (!wasZero) {
        CUtils::log("Disable sound");
        stopBackgroundMusic();
        clearSFX(false);
    }
    const float f = u2f(v) / 100.0f;
    CUtils::log("setMixerSFXVolume %f", static_cast<double>(f));
    if (mMixerSFX) mMixerSFX->setVolume(f);
}

// Ghidra 0x39bae4. [!] The music mixer gets volume / 100 x 3, and each
// track also keeps the volume / 100 it was created with (addBGMTrack, in the
// constructor: 40 -> 0.4), so the default music gain is 0.4 x 1.2.
void CSoundManager::setMixerMusicVolume(unsigned v) {
    mMusicVolume = v;
    const bool wasZero = mAllVolumeZero;
    mAllVolumeZero = mSFXVolume == 0 && v == 0;
    CUtils::log("mAllVolumeZero: %i", mAllVolumeZero ? 1 : 0);
    if (!mAllVolumeZero) {
        if (wasZero) {
            CUtils::log("Enable sound");
            if (mInitialized) startBackgroundMusic();
            setMixerMasterVolume(mMasterVolume);
        }
    } else if (!wasZero) {
        CUtils::log("Disable sound");
        stopBackgroundMusic();
        clearSFX(false);
    }
    const float f = u2f(v) / 100.0f;
    CUtils::log("setMixerMusicVolume %f", static_cast<double>(f));
    if (mMixerMusic) mMixerMusic->setVolume(f * 3.0f);
}

// Ghidra 0x39ab58
CSound* CSoundManager::getSound(unsigned id) {
    if (mAllVolumeZero) return nullptr;
    auto it = mSounds.find(id);
    return it == mSounds.end() ? nullptr : it->second;
}

void CSoundManager::destroySound(CSound* s) {
    if (s->sound) {
        s->sound->setMixer(nullptr);
        s->sound->destroy();
    }
    delete s;
}

// Ghidra 0x39b1e4
unsigned CSoundManager::playSound(const char* name, int type, bool loop, float volume, bool play) {
    if (mAllVolumeZero) return 0;
    auto* s = new CSound();
    s->type = type;
    s->id = mNextSoundSourceID++;   // the id is used up even if the sound fails
    if (type == SOUNDTYPE_BANK) {
        s->sound = CkSound::newBankSound(mBank, name);
        if (!s->sound) {
            CUtils::log("CSoundManager could not create bank sound '%s' - NULL returned", name);
            delete s;   // [PC] (leaked in the original)
            return 0;
        }
        if (s->sound->isFailed()) {
            CUtils::log("CSoundManager could not create bank sound '%s' - isFailed()", name);
            destroySound(s);
            return 0;
        }
        s->sound->setMixer(mMixerSFX);
    } else if (type == SOUNDTYPE_STREAM) {
        char file[512];
        std::snprintf(file, sizeof(file), "%s.ogg", name);
        CUtils::log("CSoundManager create stream sound from file '%s'", file);
        const std::string path = CMemoryFile::resolve(file);
        s->sound = CkSound::newStreamSound(path.empty() ? file : path.c_str());
        if (!s->sound) {
            CUtils::log("CSoundManager could not create stream sound '%s' - NULL returned", name);
            delete s;
            return 0;
        }
        if (s->sound->isFailed()) {
            CUtils::log("CSoundManager could not create stream sound '%s' - isFailed()", name);
            destroySound(s);
            return 0;
        }
        s->sound->setMixer(mMixerMusic);
    } else {
        delete s;   // [PC] (a null sound in the original)
        return 0;
    }
    s->sound->setLoopCount(loop ? -1 : 0);
    s->sound->setVolume(volume);
    if (sLogPlays)
        CUtils::log("sound '%s' id %u%s at %.0f ms, audio %.3f s", name, s->id, play ? "" : " (created)",
                    static_cast<double>(CUtils::getTimer()), ck::getOutputSeconds());
    if (play) s->sound->play();
    mSounds.emplace(s->id, s);   // insert_unique (the id is new)
    return s->id;
}

// Ghidra 0x39b448
unsigned CSoundManager::playSound(const char* name, int type, const CVector3& pos, const CVector3& vel, bool loop,
                                  float volume, bool play) {
    if (mAllVolumeZero) return 0;
    const unsigned id = playSound(name, type, loop, volume, false);
    if (CSound* s = getSound(id)) {
        s->sound->set3dPosition(pos.x, pos.y, pos.z);
        s->sound->set3dVelocity(vel.x, vel.y, vel.z);
        s->sound->set3dEnabled(true);
        if (play) s->sound->play();
        if (sLogPlays)
            CUtils::log("sound '%s' id %u 3D (%.0f %.0f %.0f) volume %.3f pan %.3f at %.0f ms, audio %.3f s", name, id,
                        static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z),
                        static_cast<double>(s->sound->get3dVolume()), static_cast<double>(s->sound->get3dPan()),
                        static_cast<double>(CUtils::getTimer()), ck::getOutputSeconds());
    }
    return id;
}

// Ghidra 0x39b210. [!] One CRng draw per call (with sound on).
unsigned CSoundManager::playRandomSound(int kind) {
    if (mAllVolumeZero) return 0;
    if (kind == RANDOMSOUND_ACKNOWLEDGE) {
        switch (f2i(CRng::GetUniform() * 5.0f)) {
            case 0: return playSound("txtroger", SOUNDTYPE_BANK, false, 1.0f, true);
            case 1: return playSound("txtaffirmative", SOUNDTYPE_BANK, false, 1.0f, true);
            case 2: return playSound("txtok", SOUNDTYPE_BANK, false, 1.0f, true);
            case 3: return playSound("txtyessir", SOUNDTYPE_BANK, false, 1.0f, true);
            case 4: return playSound("txtonmyway", SOUNDTYPE_BANK, false, 1.0f, true);
            default: return 0;   // GetUniform() == 1.0
        }
    }
    if (kind != RANDOMSOUND_ATTACK) return 0;
    switch (f2i(CRng::GetUniform() * 3.0f)) {
        case 0: return playSound("txtengage", SOUNDTYPE_BANK, false, 1.0f, true);
        case 1: return playSound("txtattacking", SOUNDTYPE_BANK, false, 1.0f, true);
        case 2: return playSound("txtapproachingtarget", SOUNDTYPE_BANK, false, 1.0f, true);
        default: return 0;
    }
}

// Ghidra 0x39b664
void CSoundManager::stopSound(unsigned id) {
    if (mAllVolumeZero) return;
    CSound* s = getSound(id);
    if (!s) return;
    destroySound(s);
    mSounds.erase(id);
}

// Ghidra 0x39abac
void CSoundManager::updateSound3D(unsigned id, const CVector3& pos, const CVector3& vel) {
    if (mAllVolumeZero) return;
    if (CSound* s = getSound(id)) {
        s->sound->set3dPosition(pos.x, pos.y, pos.z);
        s->sound->set3dVelocity(vel.x, vel.y, vel.z);
    }
}

// Ghidra 0x39ac4c
void CSoundManager::setSoundVolume(unsigned id, float volume) {
    if (mAllVolumeZero) return;
    if (CSound* s = getSound(id)) s->sound->setVolume(volume);
}

// Ghidra 0x39b7dc: frees the bank sounds (and the streams too if asked)
void CSoundManager::clearSFX(bool includeStreams) {
    CUtils::log("Clear sounds");
    for (auto it = mSounds.begin(); it != mSounds.end();) {
        CSound* s = it->second;
        if (!s || (!includeStreams && s->type != SOUNDTYPE_BANK)) {
            ++it;
            continue;
        }
        destroySound(s);
        it = mSounds.erase(it);
    }
    if (includeStreams) {   // [PC] the track list must not keep freed tracks
        mBGMTracks.clear();
        mCurrentBGM = mNextBGM = 0;
    }
}

// Ghidra 0x39b700
void CSoundManager::update() {
    mMoodAnalyzer.update();
    if (mAllVolumeZero) return;
    CkUpdate();
    // the bank sounds that have ended are freed
    for (auto it = mSounds.begin(); it != mSounds.end();) {
        CSound* s = it->second;
        if (s->sound->isPlaying() || !s->sound->isReady() || s->type != SOUNDTYPE_BANK) {
            ++it;
            continue;
        }
        s->sound->destroy();
        delete s;
        it = mSounds.erase(it);
    }
    updateBackgroundMusic();
}

// Ghidra 0x3999cc. [!] Draws CRng (once, if the mood has tracks).
CSound* CSoundManager::getRandomSoundForMood(int mood) {
    CUtils::log("getRandomSoundForMood %i ", mood);
    unsigned count = 0;
    for (CSound* s : mBGMTracks)
        if (s->mood == mood) ++count;
    if (count == 0) return nullptr;
    const unsigned r = f2u(CRng::GetUniform() * u2f(count));
    const unsigned pick = r < count - 1u ? r : count - 1u;
    CUtils::log("mood: %i -> count: %i randomcount: %i", mood, static_cast<int>(count), static_cast<int>(pick));
    unsigned n = 0;
    for (CSound* s : mBGMTracks) {
        if (s->mood != mood) continue;
        if (n == pick) {
            CUtils::log("sid %i", static_cast<int>(s->id));
            return s;
        }
        ++n;
    }
    CUtils::log("none!!");
    return nullptr;
}

// Ghidra 0x39ac94. [!] With all volumes 0 getSound is null, so the track
// keeps playing (silently) and only the id is forgotten.
void CSoundManager::stopBackgroundMusic() {
    if (mCurrentBGM == 0) return;
    if (CSound* s = getSound(mCurrentBGM)) {
        s->sound->setLoopCount(0);
        s->sound->stop();
    }
    mCurrentBGM = 0;
}

// Ghidra 0x39ace8 (CEngine::init after the menu, CGame::init, the menu's
// "leave game"). Not gated by mAllVolumeZero: always draws.
void CSoundManager::startBackgroundMusic() {
    stopBackgroundMusic();
    if (CSound* s = getRandomSoundForMood(mMoodAnalyzer.mMood)) {
        mCurrentBGM = s->id;
        s->sound->play();
        if (sLogPlays)
            CUtils::log("music '%s' at %.0f ms, audio %.3f s", s->sound->getName().c_str(),
                        static_cast<double>(CUtils::getTimer()), ck::getOutputSeconds());
    }
}

// Ghidra 0x39ad24: 5 s before the end of the track the next track of the
// current mood is queued (once): the same track loops once more, another
// one is chained with CkSound::setNextSound. When the track has ended (or
// has wrapped into its extra loop) the queued one becomes current.
void CSoundManager::updateBackgroundMusic() {
    if (mCurrentBGM == 0) return;
    CSound* cur = getSound(mCurrentBGM);
    if (!cur) return;
    CkSound* snd = cur->sound;
    const int loops = snd->getLoopCount();
    const float remaining = snd->getLengthMs() - snd->getPlayPositionMs();
    const bool keep = (!snd->isReady() || snd->isPlaying()) &&
                      (loops < 1 || !mNextBGMQueued || !(remaining > 5000.0f));
    if (!keep) {
        if (sLogPlays)
            CUtils::log("music: %u -> %u (loops %d, %.0f ms left, playing %d) at %.0f ms, audio %.3f s", mCurrentBGM,
                        mNextBGM, loops, static_cast<double>(remaining), snd->isPlaying() ? 1 : 0,
                        static_cast<double>(CUtils::getTimer()), ck::getOutputSeconds());
        mCurrentBGM = mNextBGM;
        mNextBGM = 0;
        mNextBGMQueued = false;
        return;
    }
    if (!snd->isReady() || !(remaining < 5000.0f) || mNextBGMQueued) return;
    mNextBGMQueued = true;
    CSound* next = getRandomSoundForMood(mMoodAnalyzer.mMood);
    if (sLogPlays)
        CUtils::log("music: next after %u: %u at %.0f ms, audio %.3f s", mCurrentBGM, next ? next->id : 0u,
                    static_cast<double>(CUtils::getTimer()), ck::getOutputSeconds());
    if (!next) return;
    if (next->id == cur->id) {
        mNextBGM = next->id;
        snd->setLoopCount(loops > 0 ? loops + 1 : 1);
    } else {
        next->sound->setLoopCount(0);
        snd->setNextSound(next->sound);
        mNextBGM = next->id;
    }
}

// Ghidra 0x39b54c. [!] The track's own volume is the music volume of the
// moment (the constructor's default 40 -> 0.4); the option only changes the
// mixer later.
void CSoundManager::addBGMTrack(const char* name, int mood) {
    char file[1024];
    std::snprintf(file, sizeof(file), "%s.ogg", name);
    if (CMemoryFile::resolve(file).empty()) return;   // CEngine::fileExists
    const float vol = u2f(mMusicVolume) / 100.0f;
    const unsigned id = playSound(name, SOUNDTYPE_STREAM, false, vol, false);
    CSound* s = getSound(id);
    if (!s) return;   // [PC] the original pushes the null and crashes in getRandomSoundForMood
    s->mood = mood;
    mBGMTracks.push_back(s);
}

void CSoundManager::suspend() { CkSuspend(); }
void CSoundManager::resume() { CkResume(); }

// Ghidra 0x39aaf0
void CSoundManager::drawDebug() { mMoodAnalyzer.drawDebug(); }

}  // namespace py
