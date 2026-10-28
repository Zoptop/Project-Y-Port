// SDLAudio.h - [PC] the audio output: an SDL2 audio device whose callback
// mixes the Cricket stand-in (audio/Ck.h, ck::render). Replaces Cricket's
// OpenSL ES output. Without a device the sound logic still runs (CkUpdate
// keeps the time), so a missing device is never an error.
#pragma once

namespace py::sdlaudio {

// Opens the default device (44.1 kHz stereo float) and starts it. Returns
// false (and logs why) when there is no audio driver or device.
// SDL_AUDIODRIVER chooses the driver (e.g. "dummy", or "disk" with
// SDL_DISKAUDIOFILE=<file> to record the mix).
bool open();
void close();
bool isOpen();

}  // namespace py::sdlaudio
