// CGameFeatures.h - port of ProjectY::CGameFeatures (CGame+0x188).
//
// 26 switches (ids 0..25), all on after reset(). Ids from usage:
//   1 title bar "Brains", 2 title bar "Sectors", 3 shield-gen button,
//   4 group bar, 5 event bar, 6 rally points, 7 minimap, 8 dial buttons
//   0x18/0x19, 9 XP + upgrades, 10 world touch input, 11 neutral-unit auto
//   damage (mirrors config +0x159), 12 unit availability (param1 = unit type),
//   13 building production (param1 = player, param2 = building id; 0 = all),
//   14 = 1..9, 11, 15..25 at once, 15..24 action-bar buttons, 25 dial
//   buttons 0x15/0x17.
// Ghidra: reset 0x1809b0, setFeature(id, on) 0x180a88, setFeature(id, on,
// p1, p2) 0x180c10, isFeatureEnabled 0x180ed0.
#pragma once

namespace py {

class CGameFeatures {
public:
    static constexpr int kCount = 26;

    CGameFeatures() { reset(); }
    void reset();
    void setFeature(int feature, bool on);
    void setFeature(int feature, bool on, int param1, int param2);
    void enableFeature(int f) { setFeature(f, true); }
    void disableFeature(int f) { setFeature(f, false); }
    bool isFeatureEnabled(int f) const { return f >= 0 && f < kCount && mFeatures[f]; }

private:
    bool mFeatures[kCount] = {};   // +0x04
};

}  // namespace py
