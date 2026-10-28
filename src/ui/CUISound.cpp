// CUISound.cpp - see CUISound.h.
#include "ui/CUISound.h"

#include "game/CEngine.h"
#include "game/CSoundManager.h"

namespace py::uisound {

void play(const char* name) {
    if (CSoundManager* sm = CEngine::getSingletonPtr()->getSoundManager())
        sm->playSound(name, SOUNDTYPE_BANK, false, 1.0f, true);
}

void playRandom(int kind) {
    if (CSoundManager* sm = CEngine::getSingletonPtr()->getSoundManager()) sm->playRandomSound(kind);
}

}  // namespace py::uisound
