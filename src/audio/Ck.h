// Ck.h - [PC] a small stand-in for the parts of Cricket Audio that
// CSoundManager uses (CkInit/CkUpdate/CkShutdown/CkSuspend/CkResume,
// CkMixer, CkBank, CkSound with its 3D model).
//
// The original links Cricket Audio 1.x statically (namespace Cki, C++ API
// CkSound/CkMixer/CkBank). Cricket is closed source, so this is a
// reimplementation of the behaviour the game depends on, with the gain
// formulas taken from its disassembly (ELF addresses, Thumb-2 + VFP; the
// Cricket part of the library was compiled with hardware floats):
//   - Cki::Sound::AttenParams::set          0x3eac94
//   - Cki::Sound::update3d                  0x3eb080  (distance attenuation,
//     the 3D pan and the Doppler factor)
//   - Cki::Sound::set3dListenerPosition     0x3eb93c  (the up vector is normalised)
//   - Cki::Sound::getFinalVolumeMatrix      0x3ebff4  (volume x mixer x 3D volume)
//   - Cki::VolumeMatrix::setPan / setStereoPan 0x3ecc58 / 0x3ecd40
//   - Cki::AudioUtil::monoPan_default / stereoPan_default 0x3f4d8c / 0x3f4740
//   - Cki::Mixer::update / setVolume        0x3e991c / 0x3e9a60
//   - Cki::Sound::play                      0x3eafc8 (play restarts a sound)
//
// Sounds are mixed in software into an interleaved stereo float buffer by
// ck::render(), which the platform layer calls from its audio callback
// (platform/SDLAudio.cpp). Without an output (no audio device, --no-sound,
// the tests) CkUpdate advances the playing sounds by the clock instead, the
// way Cricket's dummy output keeps time, so music still changes tracks.
//
// Bank sounds come from the .ckb bank (MS-ADPCM, decoded at load), stream
// sounds from Ogg Vorbis files (stb_vorbis, decoded while mixing).
//
// Threading: every function locks one global mutex; ck::render runs on the
// audio thread, everything else on the main thread.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace py {

void CkUpdate();
void CkShutdown();
namespace ck {
void render(float* out, int frames);
int numPlaying();
}  // namespace ck

enum CkAttenuationMode {
    kCkAttenuationMode_None = 0,
    kCkAttenuationMode_Linear = 1,
    kCkAttenuationMode_InvDistance = 2,
    kCkAttenuationMode_InvDistanceSquared = 3,
};

// Cricket's 2x2 gain matrix (Cki::VolumeMatrix): outL = inL*ll + inR*lr,
// outR = inL*rl + inR*rr; a mono source uses ll (left) and rr (right).
struct CkVolumeMatrix {
    float ll = 1.0f, lr = 0.0f, rl = 0.0f, rr = 1.0f;
    // Cki::VolumeMatrix::setPan (mono): constant power, angle (p + 1) * pi/4
    static CkVolumeMatrix pan(float p);
    // Cki::VolumeMatrix::setStereoPan: the far channel is folded in; the
    // whole matrix is halved
    static CkVolumeMatrix stereoPan(float p);
};

// The 3D attenuation parameters (Cki::Sound::AttenParams, 0x18 bytes)
struct CkAttenParams {
    int mode = kCkAttenuationMode_InvDistanceSquared;
    float nearDist = 1.0f;
    float farDist = 100.0f;
    float farVol = 0.0f;
    float invScale = 0.0f;    // +0x10 the inverse modes: vol = invScale * (1/d or 1/d^2) + invOffset
    float invOffset = 0.0f;   // +0x14
    void set(int mode, float nearDist, float farDist, float farVol);   // Cki AttenParams::set 0x3eac94
};

class CkMixer {
public:
    static CkMixer* getMaster();
    static CkMixer* newMixer(const char* name, CkMixer* parent = nullptr);   // parent null: the master
    void setVolume(float v);   // not clamped (the music mixer is set to up to 3.0)
    float getVolume() const;
    float getMixedVolume() const;   // volume x the parent's mixed volume
    const std::string& getName() const { return mName; }
    void destroy();

private:
    friend struct CkSystem;
    friend void CkShutdown();
    void update();   // Cki::Mixer::update: recompute this and the children
    std::string mName;
    float mVolume = 1.0f;
    float mMixed = 1.0f;
    CkMixer* mParent = nullptr;
    std::vector<CkMixer*> mChildren;
};

// One decoded bank sample
struct CkSample {
    std::string name;
    int channels = 1;
    int sampleRate = 44100;
    std::vector<int16_t> pcm;   // interleaved
    int frames() const { return channels > 0 ? static_cast<int>(pcm.size()) / channels : 0; }
};

class CkBank {
public:
    // The .ckb file (Cricket "ckmk" bank): a 0x48-byte header (sound count at
    // +0x38), then 72 bytes per sound (name[32], format, channels, rate,
    // block count, block bytes, block frames, volume, pan, loop data), then
    // the sample data in the same order. Format 2 is MS-ADPCM.
    static CkBank* newBank(const char* path);   // a file path (the caller resolves it)
    static CkBank* newBankFromMemory(const void* data, size_t size);
    bool isLoaded() const { return mLoaded; }
    bool isFailed() const { return !mLoaded; }
    int getNumSounds() const { return static_cast<int>(mSamples.size()); }
    const CkSample* getSample(int i) const { return &mSamples[static_cast<size_t>(i)]; }
    const CkSample* findSample(const char* name) const;
    void destroy();

    // One channel's MS-ADPCM block as the bank stores it: predictor index,
    // delta, sample 1, sample 2 (the earlier one first), then low nibble first.
    static void decodeAdpcmBlock(const uint8_t* block, int bytes, std::vector<int16_t>& out);

private:
    bool parse(const uint8_t* d, size_t n);
    bool mLoaded = false;
    std::vector<CkSample> mSamples;
};

struct CkStreamDecoder;   // Ck.cpp (stb_vorbis)

class CkSound {
public:
    static CkSound* newBankSound(CkBank* bank, const char* name);   // null if the bank has no such sound
    static CkSound* newStreamSound(const char* path);               // an Ogg Vorbis file path
    void destroy();

    void setMixer(CkMixer* m);   // null: the master mixer
    CkMixer* getMixer() const { return mMixer; }
    bool isReady() const;
    bool isFailed() const;
    void play();   // restarts the sound if it is playing
    void stop();
    bool isPlaying() const;
    void setPaused(bool p);
    bool isPaused() const;
    void setLoopCount(int n);   // -1: forever; n: n repeats after the first time
    int getLoopCount() const;
    int getCurrentLoop() const;
    int getPlayPosition() const;   // frames
    float getPlayPositionMs() const;
    int getLength() const;   // frames (-1 unknown)
    float getLengthMs() const;
    int getSampleRate() const;
    int getChannels() const;
    void setVolume(float v);
    float getVolume() const;
    float getMixedVolume() const;   // volume x mixer
    void setNextSound(CkSound* next);   // starts when this one ends by itself
    CkSound* getNextSound() const;
    void set3dEnabled(bool on);
    bool is3dEnabled() const;
    bool isVirtual() const;
    void set3dPosition(float x, float y, float z);
    void set3dVelocity(float x, float y, float z);
    bool isStream() const { return mIsStream; }
    const std::string& getName() const { return mName; }

    // [PC] the results of the last update3d (tests)
    float get3dVolume() const;
    float get3dPan() const;
    float get3dSpeed() const;
    // [PC] the gain matrix the mixer applies (final volume x pan matrix)
    CkVolumeMatrix getFinalVolumeMatrix() const;

    // The listener and the global 3D settings
    static void set3dListenerPosition(float eyeX, float eyeY, float eyeZ, float lookAtX, float lookAtY,
                                      float lookAtZ, float upX, float upY, float upZ);
    static void set3dListenerVelocity(float x, float y, float z);
    static void set3dAttenuation(int mode, float nearDist, float farDist, float farVol);
    static void set3dSoundSpeed(float s);
    static CkAttenParams get3dAttenuation();

    // Cki::Sound::update3d (0x3eb080) on plain numbers [PC]: the sound at
    // `pos` with velocity `vel`, the listener at `lpos` looking at `lookAt`
    // with the (normalised) up vector `up` and velocity `lvel`.
    struct Result3d {
        float volume, pan, speed;
        bool isVirtual;
    };
    static Result3d compute3d(const float pos[3], const float vel[3], const float lpos[3], const float lookAt[3],
                              const float up[3], const float lvel[3], const CkAttenParams& a, float soundSpeed);

private:
    friend struct CkSystem;
    friend class CkBank;
    friend void CkUpdate();
    friend void CkShutdown();
    friend void ck::render(float* out, int frames);
    friend int ck::numPlaying();
    CkSound() = default;
    ~CkSound();
    void update3dLocked();
    void playLocked();
    void stopLocked();
    void finish(float* out, int done, int frames, int outRate);
    // the frame `idx` of channel `ch` (0 beyond the end)
    float frameAt(long idx, int ch);
    void closeDecoder();
    // advance by `frames` output frames, mixing into out (null: just time)
    void mix(float* out, int frames, int outRate);
    void mixImpl(float* out, int frames, int outRate);

    std::string mName;
    bool mIsStream = false;
    bool mFailed = false;
    const CkSample* mSample = nullptr;   // bank sounds
    std::string mPath;                   // stream sounds
    std::unique_ptr<CkStreamDecoder> mDecoder;
    int mChannels = 1;
    int mRate = 44100;
    long mTotalFrames = 0;
    double mPos = 0.0;   // source frame
    bool mPlaying = false;
    bool mPaused = false;
    int mLoopCount = 0;
    int mCurrentLoop = 0;
    float mVolume = 1.0f;
    CkMixer* mMixer = nullptr;
    CkSound* mNext = nullptr;
    bool m3d = false;
    float m3dPos[3] = {0.0f, 0.0f, 0.0f};
    float m3dVel[3] = {0.0f, 0.0f, 0.0f};
    float m3dVolume = 1.0f;   // +0x50
    float m3dPan = 0.0f;      // +0x54
    float m3dSpeed = 1.0f;    // +0x58
    bool mVirtual = false;
    unsigned mMixSerial = 0;  // the mixing pass this sound was last mixed in
    bool mRampValid = false;
    CkVolumeMatrix mRamp;     // the gains at the end of the last mixed buffer
};

void CkInit();
void CkShutdown();
void CkUpdate();    // the 3D update of playing 3D sounds; without an output also advances time
void CkSuspend();   // no mixing, no time (Android onPause)
void CkResume();

namespace ck {
// [PC] the output side. setOutput(rate) with a device's sample rate makes
// render() the clock; setOutput(0) detaches it (CkUpdate keeps time).
void setOutput(int sampleRate);
int getOutputRate();
// mixes `frames` interleaved stereo frames (overwrites `out`)
void render(float* out, int frames);
// the clock CkUpdate uses without an output (milliseconds). The default
// is CUtils::getTimer; the engine passes its frame time.
void setClock(float (*nowMs)());
// [PC] the output position: seconds rendered since setOutput (0 without output)
double getOutputSeconds();
// [PC] diagnostics: how many sounds exist / are playing
int numSounds();
int numPlaying();
bool isSuspended();
}  // namespace ck

}  // namespace py
