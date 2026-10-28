// CUISound.h - the interface's CSoundManager calls (engine +0x17c):
// playSound(name, 0, false, 1.0, true) and playRandomSound(kind). Nothing
// happens without a sound manager (tests, tools).
#pragma once

namespace py::uisound {

// CSoundManager::playSound(name, ESoundType 0, no loop, volume 1.0, play)
void play(const char* name);
// CSoundManager::playRandomSound(kind): 0 = order acknowledged, 1 = attack.
// [!] draws CRng (with sound on), as in the original
void playRandom(int kind);

}  // namespace py::uisound
