// CTechManager.h - port of ProjectY::CTechManager (engine+0x48, 0x3a0 bytes):
// the 33 techs (ETechs 0..32), their prerequisites, names, research time and
// cost (config TechBuildTime<i> / TechBuildCost<i>).
#pragma once

#include <string>
#include <vector>

#include "core/Real.h"

namespace py {

class CTechManager {
public:
    static constexpr int kNumTechs = 33;

    static CTechManager& get();   // the engine's instance (+0x48), created and init()ed on first use

    void init();                                                     // Ghidra 0x39c6ac
    const std::vector<int>& getRequiredTechs(int tech) const { return mRequired[tech]; }   // Ghidra 0x39c4b8
    const std::string& getTechName(int tech) const { return mName[tech]; }   // Ghidra 0x39c55c
    const std::string& getTechDesc(int tech) const { return mDesc[tech]; }   // Ghidra 0x39c578
    // Ghidra 0x39c594: build time x the production speed factor
    Real getTechNominalResearchTime(int tech) const;
    Real getTechNominalResearchCost(int tech) const { return mBuildCost[tech]; }   // Ghidra 0x39c5d0

private:
    void addReqTech(int tech, int required);
    void addReqTechs(int tech, int a, int b, int c, int d);

    std::vector<int> mRequired[kNumTechs];   // +0x04
    std::string mName[kNumTechs];            // +0x190
    std::string mDesc[kNumTechs];            // +0x214
    Real mBuildTime[kNumTechs] = {};         // +0x298
    Real mBuildCost[kNumTechs] = {};         // +0x31c
};

}  // namespace py
