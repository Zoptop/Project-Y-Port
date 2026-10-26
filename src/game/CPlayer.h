// CPlayer.h - port of ProjectY::CPlayer / CPlayerKI, phase 3 subset: identity,
// team, start position, unit list and the AI mode set by map scripts.
// Economy, tech, FOW and scoring follow in phase 4 and later.
//
// Ghidra: CPlayer::CPlayer 0x375124, isNeutral 0x373fd8, setTeam 0x372ba8,
// isFriend 0x372b84, initTeamMembers 0x375068, CPlayerKI::CPlayerKI 0x2d8074.
#pragma once

#include <string>
#include <vector>

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CUnit;

class CPlayer {
public:
    enum EType { TYPE_HUMAN = 0, TYPE_NEUTRAL = 1, TYPE_AI = 2 };

    CPlayer(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team);
    virtual ~CPlayer() = default;

    virtual bool isAI() const { return false; }               // vtable +0x1c
    virtual void addUnit(CUnit* unit) { mUnits.push_back(unit); }   // vtable +0x04
    virtual void setTeam(unsigned team) { mTeam = team; }     // vtable +0x54

    bool isNeutral() const { return mType == TYPE_NEUTRAL; }
    bool isFriend(const CPlayer* other) const { return mTeam == other->mTeam; }
    void initTeamMembers();
    void setCaptureAbility(bool can);
    const std::string& getName() const { return mName; }

    std::string mName;                  // +0x04
    int mType = 0;                      // +0x0c
    Vec3 mColor;                        // +0x10
    unsigned mId = 0;                   // +0x1c
    CVector2 mStartPosition;            // +0x24  "set start <p> position x y"
    unsigned mTeam = 0;                 // +0x78
    std::vector<CPlayer*> mTeamMembers; // +0x90
    bool mHasTeamMates = false;         // +0x9c  more than one member in the team
    float mResources = 1000.0f;         // +0xe4
    bool mCanCapture = true;            // +0xe8
    std::vector<CUnit*> mUnits;
};

class CPlayerKI : public CPlayer {
public:
    using CPlayer::CPlayer;
    bool isAI() const override { return true; }
    int mKIMode = 0;                    // +0x14c "set kimode", "set kimainmode", aioption type 4
};

}  // namespace py
