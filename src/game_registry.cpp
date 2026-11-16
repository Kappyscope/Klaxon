#include "game_registry.hpp"

#include "common.hpp"
#include "sha256.hpp"

#include <cstdio>
#include <cstring>

// The five Action Max titles.  Titles use the names from the original
// release; ids match the Singe scripts / folder layout.
namespace {
struct GameSeed {
    const char* id;
    const char* title;
};

const GameSeed kSeeds[] = {
    { "38ambushalley", "38 Ambush Alley" },
    { "bluethunder",   "Blue Thunder" },
    { "hydrosub2021",  "Hydro Sub 2021" },
    { "popsghostly",   "The Rescue of Pops Ghostly" },
    { "sonicfury",     "Sonic Fury" },
};
const int kSeedCount = sizeof(kSeeds) / sizeof(kSeeds[0]);

void applyDefaults(GameConfig& cfg, const char* id, const char* title) {
    cfg.id    = id;
    cfg.title = title;
    cfg.media = "tape.mpg";
    // Verified values from the original Singe scripts (ScottD).  A game.cfg
    // overrides these; these defaults keep the app honest if it's missing.
    cfg.highThreshold   = 127;
    cfg.lowThreshold    = 20;
    cfg.sensorX         = 312;
    cfg.sensorY         = 205;
    cfg.sensorLeft      = 282;
    cfg.sensorTop       = 182;
    cfg.lengthIntro     = 354;
    cfg.lengthGame      = 0;
    cfg.lengthMenu      = 96;
    cfg.backgroundFrame = 0;
    if (id == std::string("bluethunder")) {
        // Blue Thunder's script differs from the others (verified 2026-11).
        cfg.highThreshold = 64;
        cfg.sensorX       = 320;
        cfg.sensorY       = 210;
        cfg.sensorLeft    = 288;
        cfg.backgroundFrame = 2453;
    }
}
}  // namespace

bool GameRegistry::parseGameCfg(const std::string& path, GameConfig& cfg) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    char line[512];
    int applied = 0;
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(s.substr(0, eq));
        std::string val = Trim(s.substr(eq + 1));
        if (key.empty() || val.empty()) continue;
        if (key[0] == '#') continue;

        if      (key == "gameID")           { /* informational */ }
        else if (key == "title")            cfg.title           = val;
        else if (key == "highThreshold")    cfg.highThreshold   = ParseIntDefault(val, cfg.highThreshold);
        else if (key == "lowThreshold")     cfg.lowThreshold    = ParseIntDefault(val, cfg.lowThreshold);
        else if (key == "sensorX")          cfg.sensorX         = ParseIntDefault(val, cfg.sensorX);
        else if (key == "sensorY")          cfg.sensorY         = ParseIntDefault(val, cfg.sensorY);
        else if (key == "sensorLeft")       cfg.sensorLeft      = ParseIntDefault(val, cfg.sensorLeft);
        else if (key == "sensorTop")        cfg.sensorTop       = ParseIntDefault(val, cfg.sensorTop);
        else if (key == "lengthIntro")      cfg.lengthIntro     = ParseIntDefault(val, cfg.lengthIntro);
        else if (key == "lengthGame")       cfg.lengthGame      = ParseIntDefault(val, cfg.lengthGame);
        else if (key == "lengthMenu")       cfg.lengthMenu      = ParseIntDefault(val, cfg.lengthMenu);
        else if (key == "backgroundFrame")  cfg.backgroundFrame = ParseIntDefault(val, cfg.backgroundFrame);
        else if (key == "media")            cfg.media           = val;
        applied++;
    }
    fclose(f);
    return applied > 0;
}

void GameRegistry::resolveMedia(GameConfig& cfg, bool fullVerify) {
    // The SD layout is <app dir>/data/media/... (build_and_deploy.ps1 stages
    // it there); /vol/content fallback covers fully-packed builds.
    std::string rel = "data/media/" + cfg.id + "/" + cfg.media;
    cfg.mediaPath = ResolveMediaPath(rel);
    cfg.ready = false;
    cfg.statusNote.clear();

    if (cfg.mediaPath.empty()) {
        cfg.statusNote = "media not found on SD card (" + rel + ")";
        return;
    }
    if (cfg.lengthGame <= 0) {
        cfg.statusNote = "game.cfg has no lengthGame";
        return;
    }

    if (!manifestLoaded_) {
        // No bundled manifest: accept by existence, note it in the log.
        cfg.ready = true;
        cfg.statusNote = "manifest missing; existence check only";
        return;
    }

    // Look up the expected digest+size by relpath as written in the manifest
    // (manifest paths are relative to the media/ dir).
    std::string wantRel = cfg.id + "/" + cfg.media;
    const ManifestEntry* entry = nullptr;
    for (const auto& m : manifest_) {
        if (m.first == wantRel) { entry = &m.second; break; }
    }

    FILE* f = fopen(cfg.mediaPath.c_str(), "rb");
    if (!f) {
        cfg.statusNote = "media not readable";
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    if (!entry) {
        // File exists but this build's manifest doesn't know it.
        cfg.ready = true;
        cfg.statusNote = "not in manifest; size " + std::to_string(size);
        return;
    }

    if (fullVerify) {
        std::string hex;
        if (!Sha256::file(cfg.mediaPath.c_str(), hex)) {
            cfg.statusNote = "hash read failed";
            return;
        }
        if (hex != entry->fullHash) {
            cfg.statusNote = "full hash mismatch - re-run tools/prepare_media.ps1";
            return;
        }
        cfg.mediaQuickHash = hex;
    } else {
        std::string hex;
        uint64_t qsize = 0;
        if (!Sha256::quickFile(cfg.mediaPath.c_str(), 256 * 1024, hex, &qsize)) {
            cfg.statusNote = "quick hash read failed";
            return;
        }
        if (qsize != entry->size) {
            cfg.statusNote = "size mismatch - wrong or truncated media file";
            return;
        }
        if (entry->quickHash.size() == 64 && hex != entry->quickHash) {
            cfg.statusNote = "integrity check failed - wrong media file";
            return;
        }
        cfg.mediaQuickHash = hex;
    }

    cfg.mediaSize = (uint64_t)size;
    cfg.ready = true;
    cfg.statusNote = "OK";
}

int GameRegistry::load(bool fullVerify) {
    fullVerify_ = fullVerify;
    games_.clear();

    // Bundled integrity manifest (romfs/manifest/prepared_hashes.sha256):
    //   <full-sha256>  <quick-sha256>  <size>  <relpath-under-media/>
    manifestLoaded_ = false;
    std::string manifestPath = ResolveAssetPath("manifest/prepared_hashes.sha256");
    if (!manifestPath.empty()) {
        FILE* f = fopen(manifestPath.c_str(), "rb");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                std::string s(line);
                size_t p1 = s.find_first_of(" \t");
                if (p1 == std::string::npos) continue;
                std::string fullHash = s.substr(0, p1);
                std::string rest = Trim(s.substr(p1 + 1));
                size_t p2 = rest.find_first_of(" \t");
                if (p2 == std::string::npos) continue;
                std::string quickHash = rest.substr(0, p2);
                rest = Trim(rest.substr(p2 + 1));
                size_t p3 = rest.find_first_of(" \t");
                if (p3 == std::string::npos) continue;
                std::string sizeTok = rest.substr(0, p3);
                std::string rel = Trim(rest.substr(p3 + 1));
                if (fullHash.size() != 64 || rel.empty()) continue;
                manifest_.push_back({ rel,
                    { fullHash, quickHash,
                      (uint64_t)strtoull(sizeTok.c_str(), nullptr, 10) } });
            }
            fclose(f);
            manifestLoaded_ = !manifest_.empty();
        }
    }

    for (int i = 0; i < kSeedCount; i++) {
        GameConfig cfg;
        applyDefaults(cfg, kSeeds[i].id, kSeeds[i].title);

        std::string cfgPath = ResolveAssetPath("games/" + cfg.id + "/game.cfg");
        if (!cfgPath.empty()) {
            parseGameCfg(cfgPath, cfg);
            KLAXON_LOG("cfg %-14s high=%d low=%d field=%dx%d sensorBox=(%d,%d) intro=%d game=%d menu=%d bg=%d",
                       cfg.id.c_str(), cfg.highThreshold, cfg.lowThreshold,
                       cfg.sensorX, cfg.sensorY, cfg.sensorLeft, cfg.sensorTop,
                       cfg.lengthIntro, cfg.lengthGame, cfg.lengthMenu, cfg.backgroundFrame);
        } else {
            KLAXON_LOG("cfg %-14s MISSING, using built-in Singe values", cfg.id.c_str());
        }

        resolveMedia(cfg, fullVerify);
        KLAXON_LOG("media %-14s %s", cfg.id.c_str(), cfg.statusNote.c_str());
        games_.push_back(cfg);
    }

    // Common segments.
    introMediaPath = ResolveMediaPath("data/media/common/intro.mpg");
    menuMediaPath  = ResolveMediaPath("data/media/common/menu.mpg");
    commonReady = !introMediaPath.empty() && !menuMediaPath.empty();
    commonStatusNote = commonReady ? "OK" : "intro/menu media missing on SD card";
    KLAXON_LOG("common media: intro=%s menu=%s (%s)",
               introMediaPath.empty() ? "(missing)" : introMediaPath.c_str(),
               menuMediaPath.empty() ? "(missing)" : menuMediaPath.c_str(),
               commonStatusNote.c_str());

    return readyCount();
}

int GameRegistry::readyCount() const {
    int n = 0;
    for (const auto& g : games_) if (g.ready) n++;
    return n;
}
