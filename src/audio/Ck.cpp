// Ck.cpp - see Ck.h.
#include "audio/Ck.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>

#include "audio/CkVorbis.h"
#include "core/CUtils.h"
#include "core/FloatBits.h"
#include "core/fdlibm.h"

namespace py {

// ---------------------------------------------------------------------------
// global state (Cki::Sound statics, Cki::Mixer::s_master, the audio graph)

struct CkSystem {
    std::recursive_mutex lock;
    CkMixer* master = nullptr;
    std::vector<CkSound*> sounds;   // Cki::Sound::s_soundList
    // listener (Cki::Sound::s_3dListenerPos / Vel / LookAt / Up)
    float listenerPos[3] = {0.0f, 0.0f, 0.0f};
    float listenerVel[3] = {0.0f, 0.0f, 0.0f};
    float listenerLookAt[3] = {0.0f, 0.0f, -1.0f};
    float listenerUp[3] = {0.0f, 1.0f, 0.0f};
    CkAttenParams atten;     // Cki::Sound::s_3dAtten (AttenParams(): set(3, 1, 100, 0))
    float soundSpeed = 0.0f;   // Cki::Sound::s_3dSoundSpeed (0: no Doppler)
    int outputRate = 0;      // 0: no output, CkUpdate keeps the time
    bool suspended = false;
    float (*clock)() = &CUtils::getTimer;
    bool clockValid = false;
    float lastClock = 0.0f;
    double pendingFrames = 0.0;
    unsigned mixSerial = 0;   // one per mixing pass (render or a CkUpdate without output)
    unsigned long long renderedFrames = 0;   // since setOutput
    CkSystem() { atten.set(kCkAttenuationMode_InvDistanceSquared, 1.0f, 100.0f, 0.0f); }
};

namespace {
CkSystem& sys() {
    static CkSystem* s = new CkSystem();   // never destroyed: static destructors may still release sounds
    return *s;
}
using Lock = std::lock_guard<std::recursive_mutex>;
constexpr int kVirtualRate = 44100;           // the time base without an output
constexpr float kVolumeRampMs = 40.0f;        // Ck_volumeRampTimeMsDefault
}  // namespace

// ---------------------------------------------------------------------------
// gain matrices

// Cki::VolumeMatrix::setPan (0x3ecc58)
CkVolumeMatrix CkVolumeMatrix::pan(float p) {
    float a;
    if (p < -1.0f) {
        a = 0.0f;
    } else {
        if (p > 1.0f) p = 1.0f;
        a = (p + 1.0f) * bitsf(0x3f490fdb);   // pi/4
    }
    CkVolumeMatrix m;
    m.ll = std::cos(a);
    m.lr = 0.0f;
    m.rl = 0.0f;
    m.rr = std::sin(a);
    return m;
}

// Cki::VolumeMatrix::setStereoPan (0x3ecd40)
CkVolumeMatrix CkVolumeMatrix::stereoPan(float p) {
    float a;
    bool negative;
    if (p < -1.0f) {
        a = bitsf(0xbfc90fdb);   // -pi/2
        negative = true;
    } else {
        if (p > 1.0f) p = 1.0f;
        a = p * bitsf(0x3fc90fdb);   // pi/2
        negative = p < 0.0f;
    }
    CkVolumeMatrix m;
    if (negative) {
        m.ll = 1.0f;
        m.lr = -std::sin(a);
        m.rl = 0.0f;
        m.rr = std::cos(a);
    } else {
        m.ll = std::cos(a);
        m.lr = 0.0f;
        m.rl = std::sin(a);
        m.rr = 1.0f;
    }
    m.ll = m.ll * 0.5f;
    m.rl = m.rl * 0.5f;
    m.lr = m.lr * 0.5f;
    m.rr = m.rr * 0.5f;
    return m;
}

// Cki::Sound::AttenParams::set (0x3eac94)
void CkAttenParams::set(int m, float nearD, float farD, float farV) {
    const float eps = bitsf(0x2b8cbccc);   // 1e-12
    float n = nearD > eps ? nearD : eps;
    float f = n + eps;
    if (f < farD) f = farD;
    mode = m;
    nearDist = n;
    farDist = f;
    float v;
    if (farV < 0.0f) v = 0.0f;
    else v = farV > 1.0f ? 1.0f : farV;
    farVol = v;
    float a, b;
    if (m == kCkAttenuationMode_InvDistanceSquared) {
        a = n * n;
        b = f * f;
    } else if (m == kCkAttenuationMode_InvDistance) {
        a = n;
        b = f;
    } else {
        invScale = 0.0f;
        invOffset = 0.0f;
        return;
    }
    const float ia = 1.0f / a;
    const float ib = 1.0f / b;
    const float scale = (v + -1.0f) / (ib - ia);
    invScale = scale;
    invOffset = 1.0f - ia * scale;
}

// ---------------------------------------------------------------------------
// CkMixer

CkMixer* CkMixer::getMaster() {
    Lock l(sys().lock);
    if (!sys().master) {
        sys().master = new CkMixer();
        sys().master->mName = "Master";
    }
    return sys().master;
}

CkMixer* CkMixer::newMixer(const char* name, CkMixer* parent) {
    Lock l(sys().lock);
    if (!parent) parent = getMaster();
    auto* m = new CkMixer();
    m->mName = name ? name : "";
    m->mParent = parent;
    parent->mChildren.push_back(m);
    m->update();
    return m;
}

// Cki::Mixer::update (0x3e991c)
void CkMixer::update() {
    mMixed = mParent ? mVolume * mParent->mMixed : mVolume;
    for (CkMixer* c : mChildren) c->update();
}

// Cki::Mixer::setVolume (0x3e9a60)
void CkMixer::setVolume(float v) {
    Lock l(sys().lock);
    if (mVolume != v) {
        mVolume = v;
        update();
    }
}

float CkMixer::getVolume() const {
    Lock l(sys().lock);
    return mVolume;
}

float CkMixer::getMixedVolume() const {
    Lock l(sys().lock);
    return mMixed;
}

void CkMixer::destroy() {
    Lock l(sys().lock);
    if (this == sys().master) {
        CUtils::log("CkMixer: the master mixer cannot be destroyed");
        return;
    }
    for (CkMixer* c : mChildren) {   // the children move to our parent
        c->mParent = mParent;
        if (mParent) mParent->mChildren.push_back(c);
        c->update();
    }
    if (mParent) {
        auto& v = mParent->mChildren;
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
    for (CkSound* s : sys().sounds)
        if (s->getMixer() == this) s->setMixer(nullptr);
    delete this;
}

// ---------------------------------------------------------------------------
// CkBank

void CkBank::decodeAdpcmBlock(const uint8_t* b, int bytes, std::vector<int16_t>& out) {
    static const int kAdapt[16] = {230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230};
    static const int kC1[7] = {256, 512, 0, 192, 240, 460, 392};
    static const int kC2[7] = {0, -256, 0, 64, 0, -208, -232};
    if (bytes < 7) return;
    auto s16 = [&](int o) { return static_cast<int>(static_cast<int16_t>(b[o] | (b[o + 1] << 8))); };
    const int p = b[0] % 7;
    int delta = s16(1);
    int s2 = s16(3);   // the earlier sample
    int s1 = s16(5);
    out.push_back(static_cast<int16_t>(s2));
    out.push_back(static_cast<int16_t>(s1));
    const int c1 = kC1[p], c2 = kC2[p];
    for (int i = 7; i < bytes; ++i) {
        const int nib[2] = {b[i] & 15, (b[i] >> 4) & 15};   // low nibble first
        for (int n : nib) {
            const int sn = (n & 8) ? n - 16 : n;
            int pred = ((s1 * c1 + s2 * c2) >> 8) + sn * delta;   // arithmetic shift
            pred = pred < -32768 ? -32768 : (pred > 32767 ? 32767 : pred);
            out.push_back(static_cast<int16_t>(pred));
            s2 = s1;
            s1 = pred;
            delta = (kAdapt[n] * delta) >> 8;
            if (delta < 16) delta = 16;
        }
    }
}

bool CkBank::parse(const uint8_t* d, size_t n) {
    if (n < 0x48 || std::memcmp(d, "ckmk", 4) != 0) return false;
    auto u16 = [&](size_t o) { return static_cast<unsigned>(d[o] | (d[o + 1] << 8)); };
    auto u32 = [&](size_t o) { return static_cast<unsigned>(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (static_cast<unsigned>(d[o + 3]) << 24)); };
    const unsigned count = u32(0x38);
    size_t entry = 0x48;
    size_t data = entry + static_cast<size_t>(count) * 72;
    if (data > n) return false;
    mSamples.clear();
    mSamples.reserve(count);
    for (unsigned i = 0; i < count; ++i, entry += 72) {
        CkSample s;
        char name[33] = {};
        std::memcpy(name, d + entry, 32);
        s.name = name;
        const unsigned format = d[entry + 32];
        s.channels = d[entry + 33];
        s.sampleRate = static_cast<int>(u16(entry + 34));
        const unsigned blocks = u32(entry + 36);
        const unsigned blockBytes = u16(entry + 40);
        const size_t size = static_cast<size_t>(blocks) * blockBytes;
        if (s.channels < 1 || s.channels > 2 || data + size > n) return false;
        const uint8_t* p = d + data;
        if (format == 2) {   // MS-ADPCM, one block per channel side by side
            const unsigned cb = blockBytes / static_cast<unsigned>(s.channels);
            std::vector<int16_t> ch[2];
            for (unsigned k = 0; k < blocks; ++k)
                for (int c = 0; c < s.channels; ++c)
                    decodeAdpcmBlock(p + static_cast<size_t>(k) * blockBytes + c * cb, static_cast<int>(cb), ch[c]);
            const size_t frames = ch[0].size();
            s.pcm.resize(frames * static_cast<size_t>(s.channels));
            for (size_t f = 0; f < frames; ++f)
                for (int c = 0; c < s.channels; ++c) s.pcm[f * s.channels + c] = c < 2 && f < ch[c].size() ? ch[c][f] : 0;
        } else if (format == 0) {   // PCM16 (not used by game.ckb)
            s.pcm.resize(size / 2);
            for (size_t k = 0; k < s.pcm.size(); ++k) s.pcm[k] = static_cast<int16_t>(p[2 * k] | (p[2 * k + 1] << 8));
        } else if (format == 1) {   // PCM8 (not used by game.ckb)
            s.pcm.resize(size);
            for (size_t k = 0; k < size; ++k) s.pcm[k] = static_cast<int16_t>(static_cast<int8_t>(p[k]) * 256);
        } else {
            CUtils::log("CkBank: sound '%s' has the unsupported format %u", s.name.c_str(), format);
        }
        data += size;
        mSamples.push_back(std::move(s));
    }
    return true;
}

CkBank* CkBank::newBankFromMemory(const void* data, size_t size) {
    auto* b = new CkBank();
    b->mLoaded = b->parse(static_cast<const uint8_t*>(data), size);
    return b;
}

CkBank* CkBank::newBank(const char* path) {
    std::ifstream f(path ? path : "", std::ios::binary);
    std::vector<uint8_t> bytes;
    if (f) bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    CkBank* b = newBankFromMemory(bytes.data(), bytes.size());
    if (!b->mLoaded) CUtils::log("CkBank: could not load '%s'", path ? path : "");
    return b;
}

const CkSample* CkBank::findSample(const char* name) const {
    if (!name) return nullptr;
    for (const CkSample& s : mSamples)
        if (s.name == name) return &s;
    return nullptr;
}

void CkBank::destroy() {
    Lock l(sys().lock);
    // sounds made from this bank stop and lose their data
    for (CkSound* s : sys().sounds)
        if (s->mSample >= mSamples.data() && s->mSample < mSamples.data() + mSamples.size()) {
            s->stopLocked();
            s->mSample = nullptr;
            s->mTotalFrames = 0;
            s->mFailed = true;
        }
    delete this;
}

// ---------------------------------------------------------------------------
// CkSound

struct CkStreamDecoder {
    void* handle = nullptr;
    std::vector<float> buf;   // interleaved, starting at source frame `start`
    long start = 0;
    ~CkStreamDecoder() { ckvorbis::close(handle); }
};

CkSound::~CkSound() = default;

CkSound* CkSound::newBankSound(CkBank* bank, const char* name) {
    if (!bank) return nullptr;
    const CkSample* s = bank->findSample(name);
    if (!s) return nullptr;
    Lock l(sys().lock);
    auto* snd = new CkSound();
    snd->mName = name;
    snd->mSample = s;
    snd->mChannels = s->channels;
    snd->mRate = s->sampleRate > 0 ? s->sampleRate : 44100;
    snd->mTotalFrames = s->frames();
    snd->mMixer = CkMixer::getMaster();
    sys().sounds.push_back(snd);
    return snd;
}

CkSound* CkSound::newStreamSound(const char* path) {
    Lock l(sys().lock);
    auto* snd = new CkSound();
    snd->mName = path ? path : "";
    snd->mPath = snd->mName;
    snd->mIsStream = true;
    snd->mMixer = CkMixer::getMaster();
    int ch = 0, rate = 0;
    long frames = 0;
    void* h = ckvorbis::open(snd->mPath.c_str(), &ch, &rate, &frames);
    if (h) {
        snd->mChannels = ch;
        snd->mRate = rate;
        snd->mTotalFrames = frames;
        ckvorbis::close(h);   // reopened when it plays
    } else {
        snd->mFailed = true;
    }
    sys().sounds.push_back(snd);
    return snd;
}

void CkSound::destroy() {
    Lock l(sys().lock);
    auto& v = sys().sounds;
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
    for (CkSound* s : v)
        if (s->mNext == this) s->mNext = nullptr;
    delete this;
}

void CkSound::setMixer(CkMixer* m) {
    Lock l(sys().lock);
    mMixer = m ? m : CkMixer::getMaster();
}

bool CkSound::isReady() const {
    Lock l(sys().lock);
    return !mFailed;
}

bool CkSound::isFailed() const {
    Lock l(sys().lock);
    return mFailed;
}

void CkSound::playLocked() {
    if (mFailed) {
        CUtils::log("CkSound: play() on a failed sound '%s'", mName.c_str());
        return;
    }
    stopLocked();
    mPos = 0.0;
    mCurrentLoop = 0;
    mPlaying = true;
    mRampValid = false;
    if (m3d) update3dLocked();
}

// Cki::Sound::play (0x3eafc8): stop, then start from the beginning
void CkSound::play() {
    Lock l(sys().lock);
    playLocked();
}

void CkSound::stopLocked() {
    mPlaying = false;
    mPos = 0.0;
    mCurrentLoop = 0;
    closeDecoder();
}

void CkSound::stop() {
    Lock l(sys().lock);
    stopLocked();
}

bool CkSound::isPlaying() const {
    Lock l(sys().lock);
    return mPlaying;
}

void CkSound::setPaused(bool p) {
    Lock l(sys().lock);
    mPaused = p;
}

bool CkSound::isPaused() const {
    Lock l(sys().lock);
    return mPaused;
}

void CkSound::setLoopCount(int n) {
    Lock l(sys().lock);
    mLoopCount = n;
}

int CkSound::getLoopCount() const {
    Lock l(sys().lock);
    return mLoopCount;
}

int CkSound::getCurrentLoop() const {
    Lock l(sys().lock);
    return mCurrentLoop;
}

int CkSound::getPlayPosition() const {
    Lock l(sys().lock);
    return static_cast<int>(mPos);
}

float CkSound::getPlayPositionMs() const {
    Lock l(sys().lock);
    return static_cast<float>(static_cast<double>(static_cast<long>(mPos)) * 1000.0 / mRate);
}

int CkSound::getLength() const {
    Lock l(sys().lock);
    return static_cast<int>(mTotalFrames);
}

float CkSound::getLengthMs() const {
    Lock l(sys().lock);
    return static_cast<float>(static_cast<double>(mTotalFrames) * 1000.0 / mRate);
}

int CkSound::getSampleRate() const { return mRate; }
int CkSound::getChannels() const { return mChannels; }

void CkSound::setVolume(float v) {
    Lock l(sys().lock);
    mVolume = v;
}

float CkSound::getVolume() const {
    Lock l(sys().lock);
    return mVolume;
}

float CkSound::getMixedVolume() const {
    Lock l(sys().lock);
    return mVolume * (mMixer ? mMixer->getMixedVolume() : 1.0f);
}

void CkSound::setNextSound(CkSound* next) {
    Lock l(sys().lock);
    mNext = next;
}

CkSound* CkSound::getNextSound() const {
    Lock l(sys().lock);
    return mNext;
}

void CkSound::set3dEnabled(bool on) {
    Lock l(sys().lock);
    m3d = on;
    if (on && mPlaying) update3dLocked();
}

bool CkSound::is3dEnabled() const {
    Lock l(sys().lock);
    return m3d;
}

bool CkSound::isVirtual() const {
    Lock l(sys().lock);
    return m3d && mVirtual;
}

void CkSound::set3dPosition(float x, float y, float z) {
    Lock l(sys().lock);
    m3dPos[0] = x;
    m3dPos[1] = y;
    m3dPos[2] = z;
}

void CkSound::set3dVelocity(float x, float y, float z) {
    Lock l(sys().lock);
    m3dVel[0] = x;
    m3dVel[1] = y;
    m3dVel[2] = z;
}

float CkSound::get3dVolume() const {
    Lock l(sys().lock);
    return m3dVolume;
}

float CkSound::get3dPan() const {
    Lock l(sys().lock);
    return m3dPan;
}

float CkSound::get3dSpeed() const {
    Lock l(sys().lock);
    return m3dSpeed;
}

// Cki::Sound::getFinalVolumeMatrix (0x3ebff4): the pan matrix (setPan for
// mono, setStereoPan for stereo sources) times volume x mixer x 3D volume
CkVolumeMatrix CkSound::getFinalVolumeMatrix() const {
    Lock l(sys().lock);
    const float pan = m3d ? m3dPan : 0.0f;
    CkVolumeMatrix m = mChannels <= 1 ? CkVolumeMatrix::pan(pan) : CkVolumeMatrix::stereoPan(pan);
    float v = mVolume * (mMixer ? mMixer->getMixedVolume() : 1.0f);
    if (m3d) v = v * m3dVolume;
    m.ll = v * m.ll;
    m.rl = v * m.rl;
    m.lr = v * m.lr;
    m.rr = v * m.rr;
    return m;
}

// the listener: CkSound::set3dListenerPosition -> Cki::Sound (0x3eb93c)
void CkSound::set3dListenerPosition(float eyeX, float eyeY, float eyeZ, float lookAtX, float lookAtY, float lookAtZ,
                                    float upX, float upY, float upZ) {
    Lock l(sys().lock);
    CkSystem& s = sys();
    s.listenerPos[0] = eyeX;
    s.listenerPos[1] = eyeY;
    s.listenerPos[2] = eyeZ;
    s.listenerLookAt[0] = lookAtX;
    s.listenerLookAt[1] = lookAtY;
    s.listenerLookAt[2] = lookAtZ;
    const float len = fdlibm::sqrtf(upY * upY + upX * upX + upZ * upZ);
    if (len < bitsf(0x26901d7d)) {   // 1e-15: keeps the previous up vector
        CUtils::log("CkSound::set3dListenerPosition: up vector is zero");
        return;
    }
    const float inv = 1.0f / (len + bitsf(0x02081cea));   // + 1e-37
    s.listenerUp[0] = inv * upX;
    s.listenerUp[1] = inv * upY;
    s.listenerUp[2] = inv * upZ;
}

void CkSound::set3dListenerVelocity(float x, float y, float z) {
    Lock l(sys().lock);
    sys().listenerVel[0] = x;
    sys().listenerVel[1] = y;
    sys().listenerVel[2] = z;
}

void CkSound::set3dAttenuation(int mode, float nearDist, float farDist, float farVol) {
    Lock l(sys().lock);
    sys().atten.set(mode, nearDist, farDist, farVol);
}

void CkSound::set3dSoundSpeed(float s) {
    Lock l(sys().lock);
    sys().soundSpeed = s;
}

CkAttenParams CkSound::get3dAttenuation() {
    Lock l(sys().lock);
    return sys().atten;
}

// Cki::Sound::update3d (0x3eb080), operation by operation
CkSound::Result3d CkSound::compute3d(const float pos[3], const float vel[3], const float lpos[3],
                                     const float lookAt[3], const float up[3], const float lvel[3],
                                     const CkAttenParams& a, float soundSpeed) {
    const float dx = pos[0] - lpos[0];
    const float dy = pos[1] - lpos[1];
    const float dz = pos[2] - lpos[2];
    const float dist = fdlibm::sqrtf(dx * dx + dy * dy + dz * dz);
    const float invDist = 1.0f / dist;
    const float nearD = a.nearDist;

    // volume
    float vol = 1.0f;
    if (dist > nearD) {
        if (dist >= a.farDist) {
            vol = a.farVol;
        } else {
            float v;
            switch (a.mode) {
                case kCkAttenuationMode_None: v = 1.0f; break;
                case kCkAttenuationMode_Linear: {
                    const float t = (dist - nearD) / (a.farDist - nearD);
                    v = (a.farVol + -1.0f) * t;
                    v = v + 1.0f;
                    break;
                }
                case kCkAttenuationMode_InvDistance: v = invDist * a.invScale + a.invOffset; break;
                case kCkAttenuationMode_InvDistanceSquared: v = a.invOffset + invDist * (invDist * a.invScale); break;
                default: v = 0.0f; break;
            }
            vol = a.farVol;
            if (!(v < a.farVol)) vol = v > 1.0f ? 1.0f : v;
        }
    }

    // pan: the direction to the sound, projected onto the plane normal to
    // the up vector, against the listener's right vector (forward x up)
    float pan;
    const float nearTenth = nearD * bitsf(0x3dcccccd);   // near * 0.1
    if (dist < nearTenth) {
        pan = 0.0f;
    } else {
        const float u0 = up[0], u1 = up[1], u2 = up[2];
        const float cz = dx * u1 - dy * u0;   // c = d x up
        const float cx = dy * u2 - dz * u1;
        const float cy = dz * u0 - dx * u2;
        const float ey = u2 * cx - u0 * cz;   // e = up x c
        const float ex = u1 * cz - u2 * cy;
        const float ez = u0 * cy - u1 * cx;
        const float lenE = fdlibm::sqrtf(ez * ez + (ex * ex + ey * ey));
        if (lenE < bitsf(0x38d1b717)) {   // 1e-4
            pan = 0.0f;
        } else {
            const float fx = lookAt[0] - lpos[0];
            const float fy = lookAt[1] - lpos[1];
            const float fz = lookAt[2] - lpos[2];
            const float ry = fz * u0 - fx * u2;   // r = forward x up
            const float rx = fy * u2 - fz * u1;
            const float rz = fx * u1 - fy * u0;
            const float lenR = fdlibm::sqrtf(rz * rz + (rx * rx + ry * ry));
            const float ia = 1.0f / lenE;
            const float ib = 1.0f / (lenR + bitsf(0x02081cea));   // + 1e-37
            const float t1 = (ey * ia) * (ib * ry);
            const float t2 = (ex * ia) * (rx * ib);
            const float t3 = (ez * ia) * (ib * rz);
            pan = t3 + (t2 + t1);
        }
        if (dist < nearD) {   // fades to the centre inside the near distance
            const float k = (dist - nearTenth) / (nearD - nearTenth);
            pan = pan * k + 0.0f;
        }
    }

    // Doppler. [!] Cricket limits both radial speeds with max(0.99 c, v)
    // instead of min, so the factor is 1 unless something moves faster than
    // 0.99 x the speed of sound.
    float speed = 1.0f;
    const float c = soundSpeed;
    if (c > 0.0f) {
        const float vs = invDist * ((dx * vel[0] + dy * vel[1]) + dz * vel[2]);
        const float vl = invDist * ((-(dy * lvel[1]) - dx * lvel[0]) - dz * lvel[2]);
        const float lim = c * bitsf(0x3f7d70a4);   // 0.99
        const float a1 = lim < vs ? vs : lim;
        const float b1 = lim < vl ? vl : lim;
        speed = (c + b1) / (c + a1);
    }
    return Result3d{vol, pan, speed, vol < bitsf(0x3ba3d70a)};   // virtual below 0.005
}

void CkSound::update3dLocked() {
    const CkSystem& s = sys();
    const Result3d r = compute3d(m3dPos, m3dVel, s.listenerPos, s.listenerLookAt, s.listenerUp, s.listenerVel,
                                 s.atten, s.soundSpeed);
    m3dVolume = r.volume;
    m3dPan = r.pan;
    if (r.speed == r.speed) m3dSpeed = r.speed;   // (a sound at the listener: 0 * inf)
    mVirtual = r.isVirtual;
}

void CkSound::closeDecoder() { mDecoder.reset(); }

float CkSound::frameAt(long idx, int ch) {
    if (idx < 0 || idx >= mTotalFrames) return 0.0f;
    if (!mIsStream) {
        if (!mSample) return 0.0f;
        return static_cast<float>(mSample->pcm[static_cast<size_t>(idx) * mChannels + ch]) * (1.0f / 32768.0f);
    }
    if (!mDecoder) {
        mDecoder = std::make_unique<CkStreamDecoder>();
        mDecoder->handle = ckvorbis::open(mPath.c_str(), nullptr, nullptr, nullptr);
        if (!mDecoder->handle) {
            mFailed = true;
            return 0.0f;
        }
        mDecoder->start = 0;
    }
    CkStreamDecoder& d = *mDecoder;
    if (!d.handle) return 0.0f;
    long have = static_cast<long>(d.buf.size()) / mChannels;
    if (idx < d.start || idx > d.start + have + 65536) {   // jump (a loop or time without mixing)
        ckvorbis::seek(d.handle, idx);
        d.buf.clear();
        d.start = idx;
        have = 0;
    }
    while (idx >= d.start + have) {
        if (have > 16384) {   // drop what was played
            const long drop = idx - d.start - 4;
            if (drop > 0) {
                d.buf.erase(d.buf.begin(), d.buf.begin() + drop * mChannels);
                d.start += drop;
                have -= drop;
            }
        }
        constexpr int kChunk = 4096;
        const size_t old = d.buf.size();
        d.buf.resize(old + static_cast<size_t>(kChunk) * mChannels);
        const int got = ckvorbis::decode(d.handle, d.buf.data() + old, mChannels, kChunk);
        d.buf.resize(old + static_cast<size_t>(got > 0 ? got : 0) * mChannels);
        if (got <= 0) return 0.0f;   // the file ended early
        have += got;
    }
    return d.buf[static_cast<size_t>(idx - d.start) * mChannels + ch];
}

void CkSound::mix(float* out, int frames, int outRate) {
    if (!mPlaying || mPaused || frames <= 0 || outRate <= 0) return;
    if (mMixSerial == sys().mixSerial) return;   // a next sound already mixed in this pass
    mMixSerial = sys().mixSerial;
    mixImpl(out, frames, outRate);
}

void CkSound::mixImpl(float* out, int frames, int outRate) {
    if (!mPlaying || mPaused || frames <= 0) return;
    if (mTotalFrames <= 0) {
        finish(out, 0, frames, outRate);
        return;
    }
    const double speed = m3d ? static_cast<double>(m3dSpeed) : 1.0;
    const double step = static_cast<double>(mRate) * speed / outRate;
    const double total = static_cast<double>(mTotalFrames);
    // not audible: no output, a virtual sound; only the time passes
    const bool audible = out && !(m3d && mVirtual);
    CkVolumeMatrix target, from;
    int ramp = 1;
    if (audible) {
        target = getFinalVolumeMatrix();
        from = mRampValid ? mRamp : target;
        ramp = std::max(1, static_cast<int>(static_cast<float>(outRate) * kVolumeRampMs / 1000.0f));
    }
    for (int i = 0; i < frames; ++i) {
        if (audible) {
            const float t = i >= ramp ? 1.0f : static_cast<float>(i + 1) / static_cast<float>(ramp);
            CkVolumeMatrix g;
            g.ll = from.ll + (target.ll - from.ll) * t;
            g.lr = from.lr + (target.lr - from.lr) * t;
            g.rl = from.rl + (target.rl - from.rl) * t;
            g.rr = from.rr + (target.rr - from.rr) * t;
            const long i0 = static_cast<long>(mPos);
            const float frac = static_cast<float>(mPos - static_cast<double>(i0));
            if (mChannels == 1) {
                const float a = frameAt(i0, 0), b = frameAt(i0 + 1, 0);
                const float x = a + (b - a) * frac;
                out[2 * i] += x * g.ll;
                out[2 * i + 1] += x * g.rr;
            } else {
                const float al = frameAt(i0, 0), bl = frameAt(i0 + 1, 0);
                const float ar = frameAt(i0, 1), br = frameAt(i0 + 1, 1);
                const float l = al + (bl - al) * frac;
                const float r = ar + (br - ar) * frac;
                out[2 * i] += l * g.ll + r * g.lr;
                out[2 * i + 1] += l * g.rl + r * g.rr;
            }
        }
        mPos += step;
        if (mPos >= total) {
            if (mLoopCount < 0 || mCurrentLoop < mLoopCount) {
                ++mCurrentLoop;
                mPos -= total;
            } else {
                finish(out, i + 1, frames, outRate);
                return;
            }
        }
    }
    mRampValid = audible;
    if (audible) mRamp = target;
}

// the end of the sound after `done` of `frames` frames: the next sound
// (CkSound::setNextSound) starts right there and fills the rest of the buffer
void CkSound::finish(float* out, int done, int frames, int outRate) {
    CkSound* next = mNext != this ? mNext : nullptr;
    stopLocked();
    if (!next) return;
    next->playLocked();
    next->mMixSerial = sys().mixSerial;
    next->mixImpl(out ? out + 2 * done : nullptr, frames - done, outRate);
}

// ---------------------------------------------------------------------------
// the system

void CkInit() {
    Lock l(sys().lock);
    CkMixer::getMaster();
    sys().clockValid = false;
    sys().suspended = false;
    CUtils::log("CkInit (software mixer)");
}

void CkShutdown() {
    Lock l(sys().lock);
    CkSystem& s = sys();
    while (!s.sounds.empty()) s.sounds.back()->destroy();
    // the mixers (children first)
    std::vector<CkMixer*> stack;
    if (s.master) stack.push_back(s.master);
    std::vector<CkMixer*> all;
    while (!stack.empty()) {
        CkMixer* m = stack.back();
        stack.pop_back();
        all.push_back(m);
        for (CkMixer* c : m->mChildren) stack.push_back(c);
    }
    for (CkMixer* m : all) delete m;
    s.master = nullptr;
    s.clockValid = false;
    s.pendingFrames = 0.0;
}

void CkUpdate() {
    Lock l(sys().lock);
    CkSystem& s = sys();
    const std::vector<CkSound*> list = s.sounds;   // a finishing sound may start another one
    if (s.outputRate == 0) {   // no output: the clock advances the sounds
        const float now = s.clock ? s.clock() : 0.0f;
        if (s.clockValid && !s.suspended && now > s.lastClock) {
            s.pendingFrames += static_cast<double>(now - s.lastClock) * (kVirtualRate / 1000.0);
            const int frames = static_cast<int>(s.pendingFrames);
            s.pendingFrames -= frames;
            ++s.mixSerial;
            for (CkSound* snd : list) snd->mix(nullptr, frames, kVirtualRate);
        }
        s.lastClock = now;
        s.clockValid = true;
    }
    // Cki::Sound::update: 3D sounds that play are updated from the listener
    for (CkSound* snd : list)
        if (snd->m3d && snd->mPlaying) snd->update3dLocked();
}

void CkSuspend() {
    Lock l(sys().lock);
    sys().suspended = true;
}

void CkResume() {
    Lock l(sys().lock);
    sys().suspended = false;
    sys().clockValid = false;
}

namespace ck {

void setOutput(int sampleRate) {
    Lock l(sys().lock);
    sys().outputRate = sampleRate > 0 ? sampleRate : 0;
    sys().clockValid = false;
    sys().renderedFrames = 0;
}

int getOutputRate() {
    Lock l(sys().lock);
    return sys().outputRate;
}

void render(float* out, int frames) {
    if (!out || frames <= 0) return;
    std::fill(out, out + static_cast<size_t>(frames) * 2, 0.0f);
    Lock l(sys().lock);
    CkSystem& s = sys();
    if (s.outputRate <= 0) return;
    s.renderedFrames += static_cast<unsigned long long>(frames);   // silence while suspended counts too
    if (s.suspended) return;
    const std::vector<CkSound*> list = s.sounds;
    ++s.mixSerial;
    for (CkSound* snd : list) snd->mix(out, frames, s.outputRate);
    for (int i = 0; i < frames * 2; ++i) out[i] = out[i] > 1.0f ? 1.0f : (out[i] < -1.0f ? -1.0f : out[i]);
}

double getOutputSeconds() {
    Lock l(sys().lock);
    return sys().outputRate > 0 ? static_cast<double>(sys().renderedFrames) / sys().outputRate : 0.0;
}

void setClock(float (*nowMs)()) {
    Lock l(sys().lock);
    sys().clock = nowMs;
    sys().clockValid = false;
}

int numSounds() {
    Lock l(sys().lock);
    return static_cast<int>(sys().sounds.size());
}

int numPlaying() {
    Lock l(sys().lock);
    int n = 0;
    for (CkSound* s : sys().sounds) n += s->mPlaying ? 1 : 0;
    return n;
}

bool isSuspended() {
    Lock l(sys().lock);
    return sys().suspended;
}

}  // namespace ck

}  // namespace py
