// CCampaign.cpp - see CCampaign.h. Menu and HUD data only: nothing here is
// read by the simulation, so plain float is used (in the original's
// operation order anyway).
#include "CCampaign.h"

#include <cstdio>
#include <cstring>

#include "core/CConfig.h"
#include "core/CMemoryFile.h"
#include "core/CUtils.h"
#include "game/CEngine.h"
#include "platform/GLFunctions.h"
#include "render/CTexture.h"

namespace py {

namespace {

// boost::char_separator("\n") over the whole file: the lines without their
// '\n' (a CRLF file keeps the '\r'), empty lines dropped
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        if (end > pos) lines.push_back(text.substr(pos, end - pos));
        pos = end + 1;
    }
    return lines;
}

// CUtils::safeReadFromFile (Ghidra 0x3d2ef4..): the next line must be
// "<key> <value>"; false (value untouched) otherwise.
bool readKeyUInt(FILE* f, const char* key, unsigned& v) {
    char buf[256];
    if (!std::fgets(buf, sizeof buf, f)) return false;
    char k[256] = "";
    unsigned u = 0;
    if (std::sscanf(buf, "%255s %u", k, &u) != 2 || std::strcmp(k, key) != 0) {
        CUtils::log("ERROR failed read [%s = %s]", key, k);
        return false;
    }
    v = u;
    return true;
}

}  // namespace

// Ghidra 0x151f58
CCampaign::CCampaign(unsigned id) : mEngine(CEngine::getSingletonPtr()), mId(id) {
    mDefLoaded = false;
    mPurchased = false;
    mDownloaded = false;
    char icon[0x100];
    std::snprintf(icon, sizeof icon, "cmp%uicon.png", mId);
    mIconFile = icon;
    mMissionsDone = 0.0f;
    mCurChapter = mCurEpisode = mMaxChapter = mMaxEpisode = 1;
    mCompleted = false;
    mPercentDone = 0.0f;
    mProgressLoaded = false;
    mFinalCut = 1;
    mInitRunning = false;
    mInitSuccess = false;
    mMissionsTotal = static_cast<float>(mNumChapters * mEpisodesPerChapter);   // (0 before parseDef)
}

// Ghidra 0x1520c8 (detaches the worker thread)
CCampaign::~CCampaign() = default;

// Ghidra 0x151430
bool CCampaign::wasEpisodeDone(unsigned chapter, unsigned episode) const {
    return chapter <= mMaxChapter && (mMaxChapter != chapter || episode <= mMaxEpisode);
}

// Ghidra 0x15145c
bool CCampaign::isOutdated() const {
    return mRevision < CConfig::get().getUInt("CurrentMapRevision" + std::to_string(mId + 1000), 0);
}

// Ghidra 0x152948
bool CCampaign::parseDef() {
    char file[300];
    std::snprintf(file, sizeof file, "cmp%udef%s.txt", mId, mEngine->mLanguage.c_str());
    std::string text;
    if (!CMemoryFile::readAll(file, text)) {
        mDefLoaded = false;
        return false;
    }
    CUtils::log("parse campaign def %s", file);
    parseDefText(text);
    return true;
}

void CCampaign::parseDefText(const std::string& text) {
    // (the original sets +0x1c only after the loop, so an "id" mismatch is overwritten [!])
    for (const std::string& line : splitLines(text)) {
        const char* s = line.c_str();
        char word[20] = "";
        std::sscanf(s, "%19s", word);
        if (std::strcmp(word, "rev") == 0) std::sscanf(s, "%*s %u", &mRevision);
        if (std::strcmp(word, "id") == 0) {
            unsigned id = 0;
            std::sscanf(s, "%*s %u", &id);
            if (mId != id) {
                CUtils::log("id mismatch");
                mDefLoaded = false;
            }
        }
        if (std::strcmp(word, "name") == 0) mName = line.size() > 5 ? line.substr(5) : std::string();
        if (std::strcmp(word, "desc") == 0) mDesc = line.size() > 5 ? line.substr(5) : std::string();
        if (std::strcmp(word, "finalcut") == 0) std::sscanf(s, "%*s %u", &mFinalCut);
        if (std::strcmp(word, "chp") == 0) std::sscanf(s, "%*s %u", &mNumChapters);
        if (std::strcmp(word, "eps") == 0) std::sscanf(s, "%*s %u", &mEpisodesPerChapter);
        if (std::strcmp(word, "chpname") == 0) {
            unsigned chp = 0;
            char name[256] = "";
            std::sscanf(s, "%*s %u %255[^\n]", &chp, name);
            mChapterNames[chp] = name;
        }
        if (std::strcmp(word, "map") == 0) {
            unsigned chp = 0, eps = 0, mapId = 0, players = 0, cut = 0;
            char name[256] = "";
            std::sscanf(s, "%*s %u %u %u %u %u %255[^\n]", &chp, &eps, &mapId, &players, &cut, name);
            CCampaignMapInfo& m = mMaps[std::make_pair(chp, eps)];
            m.chapter = chp;
            m.episode = eps;
            m.name = name;
            m.mapId = mapId;
            m.numPlayers = players;
            m.cutImage = cut;
        }
        if (std::strcmp(word, "chars") == 0) std::sscanf(s, "%*s %u", &mNumChars);
        if (std::strcmp(word, "char") == 0) {
            unsigned idx = 0;
            char key[256] = "", sub[256] = "";
            float r = 0.0f, g = 0.0f, b = 0.0f;
            std::sscanf(s, "%*s %u %255s %f %f %f %255[^\n]", &idx, key, &r, &g, &b, sub);
            const float inv = 1.0f / static_cast<float>(static_cast<int>(mNumChars));
            const float u = static_cast<float>(idx) * inv;
            CCampaignCharInfo& c = mChars[key];
            c.uv = Vec3(u, 0.0f, inv);
            c.subtitle = sub;
            c.color = Vec3(r, g, b);
        }
    }
    mDefLoaded = true;
}

// Ghidra 0x151488
void CCampaign::readProgress() {
    mMissionsDone = 0.0f;
    mCurChapter = mCurEpisode = mMaxChapter = mMaxEpisode = 1;
    mMissionsTotal = static_cast<float>(mNumChapters * mEpisodesPerChapter);
    mPercentDone = 0.0f;
    mProgressLoaded = false;
    mCompleted = false;
    char name[64];
    std::snprintf(name, sizeof name, "cmp%uprog.txt", mId);
    const std::string path = mEngine->mSaveDir + "/" + name;
    CUtils::log("Load campaign progress %s", path.c_str());
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
        CUtils::log("Load campaign progress open failed!");
        return;
    }
    if (!readKeyUInt(f, "chp", mMaxChapter)) mMaxChapter = 1;
    mCurChapter = mMaxChapter;
    if (!readKeyUInt(f, "eps", mMaxEpisode)) mMaxEpisode = 1;
    mCurEpisode = mMaxEpisode;
    unsigned cmp = 0;
    mCompleted = readKeyUInt(f, "cmp", cmp) && cmp != 0;
    std::fclose(f);
    mMissionsDone = static_cast<float>(mEpisodesPerChapter * (mCurChapter - 1) + mMaxEpisode - 1);
    mMissionsTotal = static_cast<float>(mNumChapters * mEpisodesPerChapter);
    mPercentDone = mMissionsDone * 100.0f / mMissionsTotal;
    if (mCompleted) {
        mPercentDone = 100.0f;
    } else if (!(mPercentDone > 0.0f)) {
        return;
    }
    mProgressLoaded = true;
}

// Ghidra 0x1516c8
void CCampaign::saveProgress() const {
    char name[64];
    std::snprintf(name, sizeof name, "cmp%uprog.txt", mId);
    const std::string path = mEngine->mSaveDir + "/" + name;
    CUtils::log("Save campaign progress %s", path.c_str());
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        CUtils::log("Save campaign progress open failed!");
        return;
    }
    std::fprintf(f, "chp %u\n", mMaxChapter);
    std::fprintf(f, "eps %u\n", mMaxEpisode);
    std::fprintf(f, "cmp %u\n", mCompleted ? 1u : 0u);
    std::fclose(f);
}

// Ghidra 0x1517f0 (a linear search over the map, in (chapter, episode) order)
CCampaignMapInfo* CCampaign::getMapInfo(unsigned mapId) {
    for (auto& entry : mMaps)
        if (entry.second.mapId == mapId) return &entry.second;
    return nullptr;
}

// Ghidra 0x152140
CCampaignMapInfo* CCampaign::getMapInfo(unsigned chapter, unsigned episode) {
    return &mMaps[std::make_pair(chapter, episode)];
}

// Ghidra 0x152210
CCampaignMapInfo* CCampaign::getCurrentMapInfo() { return &mMaps[std::make_pair(mCurChapter, mCurEpisode)]; }

// Ghidra 0x151eb4
bool CCampaign::markMapCompleted(unsigned mapId) {
    const CCampaignMapInfo* info = getMapInfo(mapId);
    if (!info) {
        // [PC] the original dereferences the null result (a map that is not
        // part of the campaign; the menus never get here with one)
        CUtils::log("CCampaign::markMapCompleted: map %u is not in campaign %u", mapId, mId);
        return false;
    }
    unsigned chapter = info->chapter;
    unsigned episode;
    if (info->episode == mEpisodesPerChapter) {
        if (chapter == mNumChapters) {
            mCompleted = true;
            mPercentDone = 100.0f;
            saveProgress();
            return true;
        }
        chapter = chapter + 1;
        episode = 1;
    } else {
        episode = info->episode + 1;
    }
    mCurEpisode = episode;
    mCurChapter = chapter;
    if (chapter < mMaxChapter || chapter == mMaxChapter) {
        if (chapter == mMaxChapter && mMaxEpisode < episode) mMaxEpisode = episode;
    } else {
        mMaxChapter = chapter;
        mMaxEpisode = 1;
    }
    saveProgress();
    return false;
}

// Ghidra 0x1522e4
std::vector<CTextListing> CCampaign::getCampaignText(const char* kind) const {
    std::vector<CTextListing> out;
    char file[300];
    std::snprintf(file, sizeof file, "cmp%u_%s_%s.txt", mId, kind, mEngine->mLanguage.c_str());
    std::string text;
    if (!CMemoryFile::readAll(file, text)) return out;
    CUtils::log("parse text listing file %s", file);
    return parseTextListing(text);
}

std::vector<CTextListing> CCampaign::parseTextListing(const std::string& text) {
    std::vector<CTextListing> out;
    float y = 0.0f;
    float scale = 1.0f;
    for (const std::string& line : splitLines(text)) {
        const char c = line[0];
        if (c == '#') {
            char word[20] = "";
            float v = 0.0f;
            const int n = std::sscanf(line.c_str(), "%*s %19s %f", word, &v);
            if (std::strcmp(word, "S") == 0 && n == 2) scale = v;
        } else if (c == '-') {
            y = y + scale;
        } else {
            CTextListing l;
            l.y = y;
            l.scale = scale;
            l.text = line;
            out.push_back(l);
            y = y + scale;
        }
    }
    return out;
}

// Ghidra 0x151b24
CTexture* CCampaign::getAvatarTexture() const {
    if (!gl::isLoaded()) return nullptr;
    char file[64];
    std::snprintf(file, sizeof file, "cmp%uavatare.png", mId);
    return CTextureManager::get().getTexture(file, 1, 0, 0, true);
}

Vec3 CCampaign::getAvatarUVs(const std::string& key) const {
    auto it = mChars.find(key);
    return it == mChars.end() ? Vec3(0.0f, 0.0f, 0.0f) : it->second.uv;
}

std::string CCampaign::getAvatarSubtitle(const std::string& key) const {
    auto it = mChars.find(key);
    return it == mChars.end() ? std::string() : it->second.subtitle;
}

Vec3 CCampaign::getAvatarColor(const std::string& key) const {
    auto it = mChars.find(key);
    return it == mChars.end() ? Vec3(0.0f, 0.0f, 0.0f) : it->second.color;
}

// Ghidra 0x15183c
void CCampaign::checkDownloaded(float progressShare) {
    mDownloaded = true;
    char avatar[64];
    std::snprintf(avatar, sizeof avatar, "cmp%uavatare.png", mId);
    // (CEngine::validateMD5forFile: [PC] no MD5 sums)
    if (CMemoryFile::resolve(avatar).empty()) {
        mDownloaded = false;
        return;
    }
    mInitProgress = 0.0f;
    const float step = progressShare / static_cast<float>(mMaps.size());
    for (auto& entry : mMaps) {
        if (!mEngine->isMapDownloaded(entry.second.mapId)) {
            mDownloaded = false;
            break;
        }
        mInitProgress = mInitProgress + step;
    }
}

// Ghidra 0x151ad8
void CCampaign::checkPurchased() { mPurchased = mEngine->isMapPurchased(mId + 1000); }

// Ghidra 0x1534e0 (+ the worker functions 0x151964 / 0x151990, run at once)
void CCampaign::startInitAsync(bool alsoGlobalMaps) {
    mInitSuccess = false;
    mInitProgress = 0.0f;
    mInitRunning = true;
    if (!parseDef()) {
        mInitRunning = false;
        mInitProgress = 1.0f;
        return;
    }
    readProgress();
    // (CLobbyClient::requestCmpProgress when the lobby is connected: no lobby on PC)
    checkPurchased();
    if (!alsoGlobalMaps) {
        checkDownloaded(1.0f);   // checkCampaignFilesThreadWorkerFunction
    } else {
        // checkCampaignAndGlobalMapFilesThreadWorkerFunction: also pre-checks
        // every map 0..39 and 201..209 with a CurrentMapRevision
        checkDownloaded(0.5f);
        for (unsigned id = 0; id < 0x44c; ++id) {
            if (!(id <= 0x27 || (id >= 0xc9 && id <= 0xd1))) continue;
            if (CConfig::get().getInt("CurrentMapRevision" + std::to_string(id), 0) < 1) continue;
            mEngine->isMapDownloaded(id);
        }
    }
    mInitProgress = 1.0f;
    mInitSuccess = true;
    mInitRunning = false;
}

}  // namespace py
