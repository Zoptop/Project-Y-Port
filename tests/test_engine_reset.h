// test_engine_reset.h - a fresh CEngine singleton for tests.
//
// CEngine's constructor follows the original (UseFOW on, map 2, minimap 2).
// The simulation tests were written, and their expected values recorded, with
// fog of war off and map 1 (the emulator oracle starts games that way), so
// they reset the engine to those settings.
#pragma once

#include "../src/game/CEngine.h"

inline py::CEngine* resetTestEngine() {
    py::CEngine::release();
    py::CEngine* e = py::CEngine::getSingletonPtr();
    e->mUseFOW = false;
    e->mMapId = 1;
    e->mMinimapSize = 0;
    return e;
}
