// common.hpp — platform glue: SD logging, path resolution, small utils.
//
// On the Wii U there is no "sd:" device (that is a Wii/libogc convention).
// wut registers the SD card as /vol/external01, also reachable via
// fs:/vol/external01.  The app installs to <SD>:/wiiu/apps/Klaxon-ActionMax/;
// large media lives next to the .wuhb under media/, small assets are packed
// into the .wuhb and show up under /vol/content.
#pragma once

#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

bool LogInit();
void LogClose();
FILE* KlaxonLog();

#define KLAXON_LOG(...)                                                    \
    do {                                                                   \
        FILE* _f = KlaxonLog();                                            \
        if (_f) {                                                          \
            fprintf(_f, "[Klaxon] " __VA_ARGS__);                          \
            fprintf(_f, "\n");                                             \
            fflush(_f);                                                    \
        }                                                                  \
    } while (0)

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

// App install dir on the SD card (trailing slash), for media + logs.
extern const char* const kSdPrefixes[];
extern const char* const kSdAppDir;

bool FileExists(const std::string& path);

// Media lives on the SD card first; /vol/content is the fallback (packed
// .wuhb content).  Returns an empty string when nothing matches.
std::string ResolveMediaPath(const std::string& relativePath);

// Bundled assets live in /vol/content; an SD copy overrides if present.
std::string ResolveAssetPath(const std::string& relativePath);

// ---------------------------------------------------------------------------
// Small utils
// ---------------------------------------------------------------------------

std::string Trim(const std::string& in);
int ParseIntDefault(const std::string& s, int fallback);

// Overlay/tape coordinate space used by every Singe-authored value.
static const int OVERLAY_W = 360;
static const int OVERLAY_H = 240;

// Authored tape frame rate (the Singe scripts number frames at 29.97).
static const double AUTHORED_FPS = 29.97;
