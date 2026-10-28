// SDLAudio.cpp - see SDLAudio.h.
#include "platform/SDLAudio.h"

#include <SDL.h>

#include "audio/Ck.h"
#include "core/CUtils.h"

namespace py::sdlaudio {

namespace {
SDL_AudioDeviceID gDevice = 0;
bool gInitedSubsystem = false;

void SDLCALL callback(void*, Uint8* stream, int len) {
    // interleaved stereo float frames
    ck::render(reinterpret_cast<float*>(stream), len / static_cast<int>(2 * sizeof(float)));
}
}  // namespace

bool open() {
    if (gDevice) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        CUtils::log("audio: no audio driver (%s) - sound runs without output", SDL_GetError());
        return false;
    }
    gInitedSubsystem = true;
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;   // ~23 ms
    want.callback = &callback;
    gDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!gDevice) {
        CUtils::log("audio: driver '%s' has no usable device (%s) - sound runs without output",
                    SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        gInitedSubsystem = false;
        return false;
    }
    CUtils::log("audio: driver '%s', %d Hz, %d channels, %d frames per buffer, started at %.0f ms",
                SDL_GetCurrentAudioDriver(), have.freq, have.channels, have.samples,
                static_cast<double>(CUtils::getTimer()));
    ck::setOutput(have.freq);
    SDL_PauseAudioDevice(gDevice, 0);
    return true;
}

void close() {
    if (gDevice) {
        SDL_CloseAudioDevice(gDevice);   // waits for the callback
        gDevice = 0;
        ck::setOutput(0);
    }
    if (gInitedSubsystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        gInitedSubsystem = false;
    }
}

bool isOpen() { return gDevice != 0; }

}  // namespace py::sdlaudio
