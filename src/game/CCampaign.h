// CCampaign.h - port of ProjectY::CCampaign (0x1a0 bytes, Ghidra
// 0x151430..0x1536e8): a campaign's definition (cmp<id>def<Language>.txt:
// chapters, episodes = missions, the speakers' avatars), the player's
// progress (<save dir>/cmp<id>prog.txt) and the texts of the ending
// (cmp<id>_<kind>_<Language>.txt).
//
// The engine owns the campaigns (engine +0x2e0, std::map<unsigned,
// CCampaign*>, campaign 1 is created by CEngine::init); the menus' campaign
// slider picks engine +0x2f8 (CEngine::getCurCampaign).
//
// Progress: +0x180/+0x184 is the episode the menus start (chapter, episode),
// +0x188/+0x18c the highest unlocked one, +0x196 "campaign completed". Only
// the highest unlocked episode and the completed flag are saved; readProgress
// makes the highest one current. An episode (c, e) is playable when
// wasEpisodeDone(c, e): (c, e) <= (highest chapter, highest episode)
// lexicographically. markMapCompleted (called by the aftermath screen after a
// won campaign mission, CInterfaceMenu::setupElements 0x1ba548) moves the
// current episode to the next one, raises the highest one and saves.
//
// [PC] The original runs the file checks (checkDownloaded: the avatar sheet and
// every mission's files, with MD5 sums) in a worker thread
// (startInitAsync); the port does them at once, without the MD5 sums.
// The lobby's campaign progress sync (CLobbyClient::requestCmpProgress) is
// not ported (no lobby). Nothing here is encrypted: the AES-encrypted files
// of the original (stats_ranked/unranked.cache, CEngine::saveStats) are the
// lobby's statistics and are never written by single-player or campaign
// games.
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/CVector2.h"
#include "render/Math3D.h"

namespace py {

class CEngine;
class CTexture;

// ProjectY::CCampaignMapInfo (the map value at node +0x18, 0x18 bytes)
struct CCampaignMapInfo {
    unsigned chapter = 0;      // +0x00
    unsigned episode = 0;      // +0x04
    std::string name;          // +0x08
    unsigned mapId = 0;        // +0x0c
    unsigned numPlayers = 0;   // +0x10 -> engine +0x128 when the mission starts
    unsigned cutImage = 0;     // +0x14 the loading screen's cmp1_cut_<n>.jpg
};

// ProjectY::CCampaignCharInfo (node +0x14): an avatar of the speaker texts
struct CCampaignCharInfo {
    Vec3 uv;                   // +0x00 (u0 = idx / chars, v0 = 0, width = 1 / chars)
    std::string subtitle;      // +0x0c
    Vec3 color;                // +0x10
};

// ProjectY::CTextListing (0x0c bytes): a line of a campaign text
struct CTextListing {
    float y = 0.0f;            // +0x00 position in lines (sum of the scales before it)
    float scale = 1.0f;        // +0x04 text size in lines (set by "# S <scale>")
    std::string text;          // +0x08
};

class CCampaign {
public:
    explicit CCampaign(unsigned id);   // Ghidra 0x151f58
    ~CCampaign();                      // Ghidra 0x1520c8

    // Ghidra 0x152948: reads cmp<id>def<Language>.txt; returns (and stores
    // in +0x1c) whether it was read (false also on an id mismatch).
    bool parseDef();
    // the parser of parseDef, on the file's text (also used by the tests)
    void parseDefText(const std::string& text);
    // Ghidra 0x151488 / 0x1516c8: <save dir>/cmp<id>prog.txt
    void readProgress();
    void saveProgress() const;
    // Ghidra 0x151430
    bool wasEpisodeDone(unsigned chapter, unsigned episode) const;
    // Ghidra 0x15145c: rev < config CurrentMapRevision<1000 + id>
    bool isOutdated() const;
    // Ghidra 0x1517f0: the episode of a map id, null if the map is not part of the campaign
    CCampaignMapInfo* getMapInfo(unsigned mapId);
    // Ghidra 0x152140 / 0x152210: std::map::operator[] (an unknown episode
    // is created empty, as in the original)
    CCampaignMapInfo* getMapInfo(unsigned chapter, unsigned episode);
    CCampaignMapInfo* getCurrentMapInfo();
    // Ghidra 0x151eb4: returns true if it was the last episode (the
    // campaign is complete: the credits follow)
    bool markMapCompleted(unsigned mapId);
    // Ghidra 0x1522e4: the listing of cmp<id>_<kind>_<Language>.txt (empty if missing)
    std::vector<CTextListing> getCampaignText(const char* kind) const;
    // its parser, on the file's text
    static std::vector<CTextListing> parseTextListing(const std::string& text);

    // Ghidra 0x151b24: cmp<id>avatare.png (null without a GL context)
    CTexture* getAvatarTexture() const;
    // Ghidra 0x151ba8 / 0x151cbc / 0x151da0. [!] The original uses
    // std::map::at: an icon name without a "char" line throws
    // std::out_of_range, which ends the game (map 314's first speaker text
    // has iconname("a1")). [PC] hasAvatar() lets the caller skip the icon.
    bool hasAvatar(const std::string& key) const { return mChars.count(key) != 0; }
    Vec3 getAvatarUVs(const std::string& key) const;
    std::string getAvatarSubtitle(const std::string& key) const;
    Vec3 getAvatarColor(const std::string& key) const;

    // Ghidra 0x15183c: the avatar sheet and every mission map are installed
    // ([PC] CEngine::isMapDownloaded; no MD5 sums). Sets +0x195.
    void checkDownloaded(float progressShare);
    void checkPurchased();   // Ghidra 0x151ad8 (CEngine::isMapPurchased: always)
    // Ghidra 0x1534e0: parseDef, readProgress, checkPurchased and the file
    // check. [PC] synchronous (the original starts a worker thread for the
    // file check; the menus wait for isInitComplete).
    void startInitAsync(bool alsoGlobalMaps);
    bool isInitComplete() const { return !mInitRunning; }   // Ghidra 0x151af8
    float getInitProgress() const { return mInitProgress; }  // Ghidra 0x151b04
    bool wasInitSuccessful() const { return mInitSuccess; }  // Ghidra 0x151b1c

    // ---- fields (original offsets)
    CEngine* mEngine = nullptr;                // +0x000
    unsigned mId = 0;                          // +0x004
    unsigned mNumChapters = 0;                 // +0x008 "chp"
    unsigned mEpisodesPerChapter = 0;          // +0x00c "eps"
    std::string mName;                         // +0x010 "name" (the rest of the line, with its '\r')
    std::string mDesc;                         // +0x014 "desc"
    unsigned mRevision = 0;                    // +0x018 "rev"
    bool mDefLoaded = false;                   // +0x01c
    std::string mIconFile;                     // +0x01d "cmp<id>icon.png" (char[0x100])
    float mMissionsDone = 0.0f;                // +0x11c
    float mMissionsTotal = 0.0f;               // +0x120 chapters x episodes
    float mPercentDone = 0.0f;                 // +0x124
    std::map<std::pair<unsigned, unsigned>, CCampaignMapInfo> mMaps;   // +0x128 by (chapter, episode)
    std::map<unsigned, std::string> mChapterNames;                    // +0x140 "chpname"
    unsigned mNumChars = 0;                                           // +0x158 "chars"
    std::map<std::string, CCampaignCharInfo> mChars;                  // +0x15c "char"
    bool mProgressLoaded = false;              // +0x17c a progress file with something done
    unsigned mCurChapter = 1;                  // +0x180
    unsigned mCurEpisode = 1;                  // +0x184
    unsigned mMaxChapter = 1;                  // +0x188 highest unlocked
    unsigned mMaxEpisode = 1;                  // +0x18c
    unsigned mFinalCut = 1;                    // +0x190 "finalcut": the credits' cmp1_cut_<n>.jpg
    bool mPurchased = false;                   // +0x194
    bool mDownloaded = false;                  // +0x195
    bool mCompleted = false;                   // +0x196 "cmp"
    bool mInitRunning = false;                 // +0x197
    bool mInitSuccess = false;                 // +0x198
    float mInitProgress = 0.0f;                // +0x19c
};

}  // namespace py
