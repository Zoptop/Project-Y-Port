#include "CPlayer.h"

#include "core/CUtils.h"
#include "game/CEngine.h"
#include "game/CGame.h"

namespace py {

CPlayer::CPlayer(const std::string& name, int type, const Vec3& color, unsigned id, unsigned team)
    : mName(name), mType(type), mColor(color), mId(id), mTeam(team) {}

// Ghidra 0x375068: collects every player (itself included) with the same
// team; "has team mates" means at least two members.
void CPlayer::initTeamMembers() {
    mTeamMembers.clear();
    unsigned count = 0;
    CGame* game = CEngine::getSingletonPtr()->mGame;
    if (game) {
        for (const auto& p : game->players()) {
            if (p.second->mTeam == mTeam) {
                mTeamMembers.push_back(p.second.get());
                ++count;
            }
        }
    }
    mHasTeamMates = count >= 2;
}

// Ghidra 0x372fd0
void CPlayer::setCaptureAbility(bool can) {
    CUtils::log("Player %i can capture set to %i", mId, can ? 1 : 0);
    mCanCapture = can;
}

}  // namespace py
