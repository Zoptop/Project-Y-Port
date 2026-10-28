// CSoundManager.h - port of ProjectY::CSoundManager (0xa4 bytes, singleton,
// engine +0x17c) on top of the Cricket stand-in (audio/Ck.h).
//
// Sounds are identified by a running id (mNextSoundSourceID, from 1; 0 is
// "no sound"). Bank sounds (ESoundType 0) come from game.ckb and are freed by
// update() when they end; stream sounds (1) are the 60 music tracks
// "<mood>_<1..10>.ogg" made in the constructor and kept for the session.
//
// The music: startBackgroundMusic plays a random track of the current mood
// (CSoundMoodAnalyzer, +0x18); updateBackgroundMusic queues the next random
// track of the then current mood 5 s before the end (CkSound::setNextSound,
// or one more loop if the same track was drawn).
//
// [!] Randomness: getRandomSoundForMood and playRandomSound draw from CRng,
// the unsynchronised RNG that also drives the AI. These draws are part of the
// original's behaviour and are kept exactly; they never touch CRngSafe. The
// draws do not depend on the audio device: the logic runs the same with
// --no-sound or without a device (only the mixing is skipped).
//
// [!] All volumes 0 (SFX and music; "mAllVolumeZero", +0x85) turns most of
// the manager off: getSound returns null, so playSound/updateSound3D/... do
// nothing and the music is not advanced. startBackgroundMusic still draws.
//
// Sound never changes the simulation: the simulation calls playSound and
// the mood counters, but no simulation code reads anything back except the
// returned sound id, which the objects only keep to stop/move their sound.
#pragma once

#include <unordered_map>
#include <vector>

#include "core/CVector3.h"
#include "game/CSoundMoodAnalyzer.h"

namespace py {

class CEngine;
class CkBank;
class CkMixer;
class CkSound;

enum ESoundType {
    SOUNDTYPE_BANK = 0,     // a sound of game.ckb (mixer "SFX")
    SOUNDTYPE_STREAM = 1,   // "<name>.ogg" (mixer "Music")
};

enum ERandomSounds {
    RANDOMSOUND_ACKNOWLEDGE = 0,   // roger / affirmative / ok / yes sir / on my way
    RANDOMSOUND_ATTACK = 1,        // engage / attacking / approaching target
};

// ProjectY::CSound (0x14 bytes)
struct CSound {
    unsigned id = 0;                 // +0x00
    int type = SOUNDTYPE_BANK;       // +0x04 ESoundType
    bool flag8 = false;              // +0x08 (always 0)
    int mood = 0;                    // +0x0c ESoundMood of a music track
    CkSound* sound = nullptr;        // +0x10
};

class CSoundManager {
public:
    CSoundManager();                 // Ghidra 0x39bc20 (CkInit must have run)
    ~CSoundManager();                // Ghidra 0x39b89c
    static CSoundManager* getSingletonPtr();   // Ghidra 0x39bf58 (creates it)
    static CSoundManager* get() { return mSingleton; }   // [PC] null when sound was never set up
    static void release();           // Ghidra 0x39b978

    // Ghidra 0x39b1e4: a new sound; returns its id (0 on failure or with
    // all volumes 0). loop: forever; play: start now.
    unsigned playSound(const char* name, int type, bool loop, float volume, bool play);
    // Ghidra 0x39b448: the same, positioned in 3D (the camera is the listener)
    unsigned playSound(const char* name, int type, const CVector3& pos, const CVector3& vel, bool loop,
                       float volume, bool play);
    unsigned playRandomSound(int kind);                        // Ghidra 0x39b210 (ERandomSounds)
    void stopSound(unsigned id);                               // Ghidra 0x39b664
    void updateSound3D(unsigned id, const CVector3& pos, const CVector3& vel);   // Ghidra 0x39abac
    void setSoundVolume(unsigned id, float volume);            // Ghidra 0x39ac4c
    CSound* getSound(unsigned id);                             // Ghidra 0x39ab58
    void clearSFX(bool includeStreams);                        // Ghidra 0x39b7dc

    void update();                                             // Ghidra 0x39b700 (CEngine::draw, every frame)
    void startBackgroundMusic();                               // Ghidra 0x39ace8
    void stopBackgroundMusic();                                // Ghidra 0x39ac94
    void updateBackgroundMusic();                              // Ghidra 0x39ad24
    CSound* getRandomSoundForMood(int mood);                   // Ghidra 0x3999cc
    void addBGMTrack(const char* name, int mood);              // Ghidra 0x39b54c

    void setMixerMasterVolume(unsigned v);                     // Ghidra 0x399964 (0..100)
    void setMixerSFXVolume(unsigned v);                        // Ghidra 0x39b9b8
    void setMixerMusicVolume(unsigned v);                      // Ghidra 0x39bae4 (mixer = v / 100 x 3)
    unsigned getMixerSFXVolume() const { return mSFXVolume; }
    unsigned getMixerMusicVolume() const { return mMusicVolume; }
    unsigned getMixerMasterVolume() const { return mMasterVolume; }

    // [PC] the app is in the background (the original's AppOnPause called
    // CkSuspend; it never resumed explicitly)
    void suspend();
    void resume();

    void drawDebug();                                          // Ghidra 0x39aaf0
    CSoundMoodAnalyzer& getMoodAnalyzer() { return mMoodAnalyzer; }

    // ---- fields (original offsets)
    std::unordered_map<unsigned, CSound*> mSounds;   // +0x00 (std::unordered_map, max load 1.0)
    CSoundMoodAnalyzer mMoodAnalyzer;                // +0x18 (its mMood is +0x64)
    CEngine* mEngine = nullptr;                      // +0x68
    CkBank* mBank = nullptr;                         // +0x6c "game.ckb"
    CkMixer* mMixerMusic = nullptr;                  // +0x70
    CkMixer* mMixerSFX = nullptr;                    // +0x74
    unsigned mSFXVolume = 0;                         // +0x78
    unsigned mMusicVolume = 0;                       // +0x7c
    unsigned mMasterVolume = 0;                      // +0x80
    bool mInitialized = false;                       // +0x84 the music tracks are loaded
    bool mAllVolumeZero = false;                     // +0x85
    unsigned mCurrentBGM = 0;                        // +0x88 the playing track
    unsigned mUnknown8c = 0;                         // +0x8c
    unsigned mNextBGM = 0;                           // +0x90 the queued track
    bool mNextBGMQueued = false;                     // +0x94
    std::vector<CSound*> mBGMTracks;                 // +0x98

    static unsigned mNextSoundSourceID;              // starts at 1
    static bool sLogPlays;                           // [PC] --log-sounds: log every new sound (timer ms)
    static CSoundManager* mSingleton;

private:
    void destroySound(CSound* s);
};

}  // namespace py
