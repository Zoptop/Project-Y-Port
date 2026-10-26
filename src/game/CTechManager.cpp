// CTechManager.cpp - see CTechManager.h.
#include "game/CTechManager.h"

#include <cstdio>

#include "core/CConfig.h"
#include "core/CTxtLoc.h"
#include "game/CEngine.h"

namespace py {

CTechManager& CTechManager::get() {
    static CTechManager* sInstance = [] {
        auto* t = new CTechManager();
        t->init();
        return t;
    }();
    return *sInstance;
}

// Ghidra 0x39c5ec
void CTechManager::addReqTech(int tech, int required) { mRequired[tech].push_back(required); }

// Ghidra 0x39c640
void CTechManager::addReqTechs(int tech, int a, int b, int c, int d) {
    addReqTech(tech, a);
    if (b != -1) addReqTech(tech, b);
    if (c != -1) addReqTech(tech, c);
    if (d != -1) addReqTech(tech, d);
}

// Ghidra 0x39c6ac. Tech 0 needs nothing; tech 19 (0x13) is the common tier-2 gate.
void CTechManager::init() {
    for (auto& v : mRequired) v.clear();
    addReqTechs(1, 0, -1, -1, -1);
    addReqTechs(2, 1, 0x13, -1, -1);
    addReqTechs(3, 0, -1, -1, -1);
    addReqTechs(4, 3, 0x13, -1, -1);
    addReqTechs(5, 0, -1, -1, -1);
    addReqTechs(6, 5, 0x13, -1, -1);
    addReqTechs(7, 0, -1, -1, -1);
    addReqTechs(8, 7, 0x13, -1, -1);
    addReqTechs(9, 0, -1, -1, -1);
    addReqTechs(10, 9, 0x13, -1, -1);
    addReqTechs(0xb, 10, 2, 0x1c, -1);
    addReqTechs(0xc, 7, 0x13, -1, -1);
    addReqTechs(0xd, 0, -1, -1, -1);
    addReqTechs(0xe, 0xd, 0x13, -1, -1);
    addReqTechs(0xf, 0x1c, 0xe, -1, -1);
    addReqTechs(0x10, 0, -1, -1, -1);
    addReqTechs(0x11, 0x13, 0x10, -1, -1);
    addReqTechs(0x12, 0x11, 0x1c, -1, -1);
    addReqTechs(0x13, 0, -1, -1, -1);
    addReqTechs(0x14, 0xd, 0x13, -1, -1);
    addReqTechs(0x15, 0x1c, 0x14, -1, -1);
    addReqTechs(0x16, 0x1c, 0x14, -1, -1);
    addReqTechs(0x17, 0x1d, 0x16, -1, -1);
    addReqTechs(0x18, 0x1d, 0x15, -1, -1);
    addReqTechs(0x19, 0xd, 0x13, -1, -1);
    addReqTechs(0x1a, 0x1c, 0x19, -1, -1);
    addReqTechs(0x1b, 0x1d, 0x1a, -1, -1);
    addReqTechs(0x1c, 0x13, -1, -1, -1);
    addReqTechs(0x1d, 0x1c, -1, -1, -1);
    addReqTechs(0x1e, 0xf, 0x1d, -1, -1);
    addReqTechs(0x1f, 0xb, 0x1d, -1, -1);
    addReqTechs(0x20, 0x1c, 6, 0xe, -1);
    const CConfig& cfg = CConfig::get();
    char key[32];
    for (int i = 0; i < kNumTechs; ++i) {
        std::snprintf(key, sizeof key, "NameTech%i", i);
        mName[i] = CTxtLoc::get().getS(key);
        std::snprintf(key, sizeof key, "DescTech%i", i);
        mDesc[i] = CTxtLoc::get().getS(key);
        std::snprintf(key, sizeof key, "TechBuildTime%i", i);
        cfg.updateConfigValue(mBuildTime[i], key);
        std::snprintf(key, sizeof key, "TechBuildCost%i", i);
        cfg.updateConfigValue(mBuildCost[i], key);
    }
}

// Ghidra 0x39c594
Real CTechManager::getTechNominalResearchTime(int tech) const {
    return mBuildTime[tech] * CEngine::getSingletonPtr()->getSpeedFactor();
}

}  // namespace py
