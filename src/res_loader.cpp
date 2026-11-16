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
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return false;
    }
    out.resize((size_t)size);
    size_t got = fread(out.data(), 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        out.clear();
        return false;
    }
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
}
