#include "common.hpp"

#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

static FILE* g_log = nullptr;

bool LogInit() {
    static const char* kCandidates[] = {
        "/vol/external01/wiiu/apps/Klaxon-ActionMax/klaxon.log",
        "fs:/vol/external01/wiiu/apps/Klaxon-ActionMax/klaxon.log",
        nullptr
    };
    for (int i = 0; kCandidates[i]; i++) {
        FILE* f = fopen(kCandidates[i], "w");
        if (f) {
            g_log = f;
            return true;
        }
    }
    return false;
}

void LogClose() {
    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }
}

FILE* KlaxonLog() { return g_log; }

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

const char* const kSdPrefixes[] = {
    "/vol/external01/wiiu/apps/Klaxon-ActionMax/",
    "fs:/vol/external01/wiiu/apps/Klaxon-ActionMax/",
    nullptr
};

const char* const kSdAppDir = "wiiu/apps/Klaxon-ActionMax/";

bool FileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

std::string ResolveMediaPath(const std::string& relativePath) {
    for (int i = 0; kSdPrefixes[i]; i++) {
        std::string full = std::string(kSdPrefixes[i]) + relativePath;
        if (FileExists(full)) return full;
    }
    std::string content = std::string("/vol/content/") + relativePath;
    if (FileExists(content)) return content;
    return std::string();
}

std::string ResolveAssetPath(const std::string& relativePath) {
    // SD data/ layout FIRST: streaming reads from /vol/content can hang on
    // hardware once the audio stack is up, so prefer plain SD files.
    for (int i = 0; kSdPrefixes[i]; i++) {
        std::string full = std::string(kSdPrefixes[i]) + "data/" + relativePath;
        if (FileExists(full)) return full;
    }
    std::string content = std::string("/vol/content/") + relativePath;
    if (FileExists(content)) return content;
    for (int i = 0; kSdPrefixes[i]; i++) {
        std::string full = std::string(kSdPrefixes[i]) + relativePath;
        if (FileExists(full)) return full;
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Small utils
// ---------------------------------------------------------------------------

std::string Trim(const std::string& in) {
    size_t a = in.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = in.find_last_not_of(" \t\r\n");
    return in.substr(a, b - a + 1);
}

int ParseIntDefault(const std::string& s, int fallback) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str()) return fallback;
    return (int)v;
}
