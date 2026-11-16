#include "res_loader.hpp"

#include <cstdio>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include "common.hpp"

namespace {

struct FontEntry {
    TTF_Font*       font = nullptr;
    SDL_RWops*      rw   = nullptr;
    std::vector<uint8_t>* buf = nullptr;   // owned; freetype reads lazily
};

std::vector<FontEntry> g_fonts;

// stdio read of the whole file — this call pattern is proven working on
// both /vol/content and /vol/external01 (FileExists / log writes use it).
bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    // Sequential reads only: no fseek/ftell (SEEK on the content mount is a
    // hang suspect), chunked so a stuck size is visible in the log.
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        KLAXON_LOG("res: fopen FAILED: %s", path.c_str());
        return false;
    }
    uint8_t chunk[4096];
    size_t total = 0;
    for (;;) {
        size_t got = fread(chunk, 1, sizeof(chunk), f);
        total += got;
        if (got > 0) out.insert(out.end(), chunk, chunk + got);
        if (got < sizeof(chunk)) break;   // EOF or error
        if (total % 16384 == 0) {
            KLAXON_LOG("res: read %zu bytes of %s", total, path.c_str());
        }
    }
    bool ok = !ferror(f) && !out.empty();
    fclose(f);
    if (!ok) {
        KLAXON_LOG("res: read error at %zu bytes: %s", total, path.c_str());
        return false;
    }
    KLAXON_LOG("res: read complete %s (%zu bytes)", path.c_str(), total);
    return true;
}

}  // namespace

TTF_Font* ResOpenFont(const std::string& path, int px) {
    auto* buf = new std::vector<uint8_t>();
    if (!ReadWholeFile(path, *buf)) {
        KLAXON_LOG("res: font read FAILED: %s", path.c_str());
        delete buf;
        return nullptr;
    }

    SDL_RWops* rw = SDL_RWFromMem(buf->data(), (int)buf->size());
    if (!rw) {
        KLAXON_LOG("res: RWFromMem failed (%s): %s", path.c_str(), SDL_GetError());
        delete buf;
        return nullptr;
    }

    // freesrc = 0: we keep the RWops + buffer alive for lazy glyph reads.
    TTF_Font* font = TTF_OpenFontRW(rw, 0, px);
    if (!font) {
        KLAXON_LOG("res: TTF_OpenFontRW(%s, %d) FAILED: %s", path.c_str(), px,
                   TTF_GetError());
        SDL_RWclose(rw);
        delete buf;
        return nullptr;
    }

    KLAXON_LOG("res: font ok %s (%zu bytes, %d px)", path.c_str(),
               buf->size(), px);
    g_fonts.push_back({ font, rw, buf });
    return font;
}

SDL_Texture* ResLoadTexture(SDL_Renderer* renderer, const std::string& path) {
    std::vector<uint8_t> buf;
    if (!ReadWholeFile(path, buf)) {
        KLAXON_LOG("res: texture read FAILED: %s", path.c_str());
        return nullptr;
    }
    SDL_RWops* rw = SDL_RWFromMem(buf.data(), (int)buf.size());
    if (!rw) return nullptr;
    // PNG decoding completes inside this call, so the RWops is closed after.
    SDL_Texture* tex = IMG_LoadTexture_RW(renderer, rw, 0);
    SDL_RWclose(rw);
    if (!tex) {
        KLAXON_LOG("res: IMG_LoadTexture_RW(%s) FAILED: %s", path.c_str(),
                   IMG_GetError());
    }
    return tex;
}

Mix_Chunk* ResLoadWav(const std::string& path) {
    std::vector<uint8_t> buf;
    if (!ReadWholeFile(path, buf)) {
        KLAXON_LOG("res: wav read FAILED: %s", path.c_str());
        return nullptr;
    }
    SDL_RWops* rw = SDL_RWFromMem(buf.data(), (int)buf.size());
    if (!rw) return nullptr;
    // WAV decoding completes inside this call.
    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 0);
    SDL_RWclose(rw);
    if (!chunk) {
        KLAXON_LOG("res: Mix_LoadWAV_RW(%s) FAILED: %s", path.c_str(),
                   Mix_GetError());
    }
    return chunk;
}

// ---------------------------------------------------------------------------
// Staged WAVs: file read at boot (pre-audio), decode after the device opens.
// ---------------------------------------------------------------------------

struct StagedWav {
    bool valid = false;
    std::vector<uint8_t>* buf = nullptr;
    std::string path;
};

std::vector<StagedWav> g_staged;
std::vector<std::vector<uint8_t>*> g_keepAlive;   // buffers referenced by QuickLoad chunks

int ResStageWav(const std::string& path) {
    StagedWav s;
    s.buf = new std::vector<uint8_t>();
    if (!ReadWholeFile(path, *s.buf)) {
        KLAXON_LOG("res: staged wav read FAILED: %s", path.c_str());
        delete s.buf;
        return -1;
    }
    s.valid = true;
    s.path = path;
    g_staged.push_back(s);
    return (int)g_staged.size() - 1;
}

Mix_Chunk* ResDecodeWav(int stagedId) {
    if (stagedId < 0 || stagedId >= (int)g_staged.size()) return nullptr;
    StagedWav& s = g_staged[stagedId];
    if (!s.valid || !s.buf) return nullptr;

    KLAXON_LOG("res: decoding staged wav %s (%zu bytes)", s.path.c_str(),
               s.buf->size());

    // Fast path: our prep pipeline emits canonical 48 kHz stereo S16 WAVs,
    // which exactly match the opened device format.  Parse the RIFF manually
    // and wrap the data chunk with Mix_QuickLoad_RAW - this never touches
    // SDL_mixer's audio lock (which can wedge if the audio thread died).
    const std::vector<uint8_t>& b = *s.buf;
    if (b.size() > 44 && b[0] == 'R' && b[1] == 'I' && b[2] == 'F' && b[3] == 'F'
        && b[8] == 'W' && b[9] == 'A' && b[10] == 'V' && b[11] == 'E') {
        size_t off = 12;
        uint32_t fmtRate = 0, fmtBits = 0, fmtChannels = 0;
        const uint8_t* data = nullptr;
        uint32_t dataLen = 0;
        bool haveFmt = false, haveData = false;
        while (off + 8 <= b.size()) {
            uint32_t ckId   = (uint32_t)b[off] | ((uint32_t)b[off+1] << 8) |
                              ((uint32_t)b[off+2] << 16) | ((uint32_t)b[off+3] << 24);
            uint32_t ckSize = (uint32_t)b[off+4] | ((uint32_t)b[off+5] << 8) |
                              ((uint32_t)b[off+6] << 16) | ((uint32_t)b[off+7] << 24);
            if (ckId == 0x20746D66) {   // 'fmt '
                fmtChannels = (uint32_t)b[off+10] | ((uint32_t)b[off+11] << 8);
                fmtRate     = (uint32_t)b[off+12] | ((uint32_t)b[off+13] << 8) |
                              ((uint32_t)b[off+14] << 16) | ((uint32_t)b[off+15] << 24);
                fmtBits     = (uint32_t)b[off+22] | ((uint32_t)b[off+23] << 8);
                haveFmt = true;
            } else if (ckId == 0x61746164) {  // 'data'
                data = b.data() + off + 8;
                dataLen = (off + 8 + ckSize <= b.size()) ? ckSize
                                                         : (uint32_t)(b.size() - off - 8);
                haveData = true;
            }
            off += 8 + ckSize + (ckSize & 1);
        }
        if (haveFmt && haveData && fmtRate == 48000 && fmtChannels == 2 && fmtBits == 16) {
            Mix_Chunk* chunk = Mix_QuickLoad_RAW(data, dataLen);
            if (chunk) {
                KLAXON_LOG("res: quickload ok %s (%u bytes raw)",
                           s.path.c_str(), dataLen);
                // The chunk references the staged buffer directly: keep it
                // alive until shutdown.
                g_keepAlive.push_back(s.buf);
                s.buf = nullptr;
                s.valid = false;
                return chunk;
            }
            KLAXON_LOG("res: Mix_QuickLoad_RAW FAILED: %s", Mix_GetError());
        } else {
            KLAXON_LOG("res: wav format mismatch rate=%u ch=%u bits=%u - fallback",
                       fmtRate, fmtChannels, fmtBits);
        }
    }

    // Fallback / odd formats: full SDL_mixer decode from memory.
    SDL_RWops* rw = SDL_RWFromMem(s.buf->data(), (int)s.buf->size());
    if (!rw) return nullptr;
    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 0);
    SDL_RWclose(rw);

    delete s.buf;
    s.buf = nullptr;
    s.valid = false;

    if (!chunk) {
        KLAXON_LOG("res: staged decode FAILED (%s): %s", s.path.c_str(),
                   Mix_GetError());
    }
    return chunk;
}

void ResShutdown() {
    // Frees RWops + buffers.  Fonts themselves are closed by their owners
    // (TTF_CloseFont) BEFORE this runs.
    for (auto& e : g_fonts) {
        if (e.rw) SDL_RWclose(e.rw);
        delete e.buf;
        e.font = nullptr;
        e.rw = nullptr;
        e.buf = nullptr;
    }
    g_fonts.clear();
    for (auto* b : g_keepAlive) delete b;
    g_keepAlive.clear();
}
