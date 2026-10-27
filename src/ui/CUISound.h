// CUISound.h - the HUD's CSoundManager calls (engine +0x17c: playSound(name,
// ...) and playRandomSound(kind)). Sound is phase 8 and not ported yet:
// these are no-ops that keep the call sites (and their names) in place.
#pragma once

namespace py::uisound {

// CSoundManager::playSound(name, 0, 0, 1.0, true)
inline void play(const char* /*name*/) {}
// CSoundManager::playRandomSound(kind): 0 = order acknowledged, 1 = attack
inline void playRandom(int /*kind*/) {}

}  // namespace py::uisound
