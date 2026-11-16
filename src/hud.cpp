#include "hud.hpp"

#include "common.hpp"
#include "res_loader.hpp"

#include <SDL_image.h>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Palette (Klaxon redesign)
// ---------------------------------------------------------------------------
namespace palette {
static const SDL_Color orange   = { 255, 107, 26, 255 };
static const SDL_Color amber    = { 255, 210, 63, 255 };
static const SDL_Color ledRed   = { 255, 42, 42, 255 };
static const SDL_Color ledDark  = { 70, 12, 12, 255 };
static const SDL_Color panel    = { 10, 12, 20, 216 };
static const SDL_Color white    = { 235, 235, 240, 255 };
static const SDL_Color gray     = { 150, 155, 165, 255 };
}  // namespace

// ---------------------------------------------------------------------------
// Init / shutdown
// ---------------------------------------------------------------------------

SDL_Texture* Hud::loadTex(const char* rel) {
    std::string path = ResolveAssetPath(rel);
    if (path.empty()) return nullptr;
    return ResLoadTexture(renderer_, path);
}

bool Hud::init(SDL_Renderer* renderer) {
    renderer_ = renderer;
    KLAXON_LOG("hud init: begin");

    auto openFont = [&](const char* rel, int px) -> TTF_Font* {
        std::string path = ResolveAssetPath(rel);
        KLAXON_LOG("hud: font %s (path=%s)", rel,
                   path.empty() ? "MISSING" : path.c_str());
        if (path.empty()) return nullptr;
        return ResOpenFont(path, px);
    };

    fDisplay48_ = openFont("assets/fonts/ui_display.ttf", 48);
    fDisplay64_ = openFont("assets/fonts/ui_display.ttf", 64);
    fDisplay96_ = openFont("assets/fonts/ui_display.ttf", 96);
    fBody16_    = openFont("assets/fonts/ui_body.ttf", 16);
    fBody20_    = openFont("assets/fonts/ui_body.ttf", 20);
    fDseg40_    = openFont("assets/fonts/dseg7.ttf", 40);

    // Graceful fallback to the original Singe fonts if the redesign fonts
    // are missing for any reason.
    if (!fDisplay48_) fDisplay48_ = openFont("assets/fonts/font_bluestone.ttf", 40);
    if (!fDisplay64_) fDisplay64_ = openFont("assets/fonts/font_bluestone.ttf", 56);
    if (!fBody16_)    fBody16_    = openFont("assets/fonts/font_chemrea.ttf", 16);
    if (!fBody20_)    fBody20_    = openFont("assets/fonts/font_chemrea.ttf", 20);
    if (!fDseg40_)    fDseg40_    = openFont("assets/fonts/font_led_real.ttf", 36);

    auto loadTex = [&](const char* rel) -> SDL_Texture* {
        std::string path = ResolveAssetPath(rel);
        if (path.empty()) {
            KLAXON_LOG("hud: tex %s MISSING", rel);
            return nullptr;
        }
        return ResLoadTexture(renderer_, path);
    };

    texCrosshair_ = loadTex("assets/sprites/sprite_crosshair.png");
    texLightOn_   = loadTex("assets/sprites/sprite_lighton.png");
    texLightOff_  = loadTex("assets/sprites/sprite_lightoff.png");
    texBullet_    = loadTex("assets/sprites/sprite_bullet.png");

    KLAXON_LOG("hud: fonts %s | crosshair=%d lightOn=%d bullet=%d",
               fDisplay48_ && fBody20_ && fDseg40_ ? "OK" : "PARTIAL",
               texCrosshair_ != nullptr, texLightOn_ != nullptr,
               texBullet_ != nullptr);
    return true;
}

void Hud::shutdown() {
    for (int i = 0; i < cacheCount_; i++) {
        if (cache_[i].tex) SDL_DestroyTexture(cache_[i].tex);
        cache_[i].tex = nullptr;
    }
    cacheCount_ = 0;
    if (texCrosshair_) { SDL_DestroyTexture(texCrosshair_); texCrosshair_ = nullptr; }
    if (texLightOn_)   { SDL_DestroyTexture(texLightOn_);   texLightOn_ = nullptr; }
    if (texLightOff_)  { SDL_DestroyTexture(texLightOff_);  texLightOff_ = nullptr; }
    if (texBullet_)    { SDL_DestroyTexture(texBullet_);    texBullet_ = nullptr; }
    if (fDisplay48_)   { TTF_CloseFont(fDisplay48_); fDisplay48_ = nullptr; }
    if (fDisplay64_)   { TTF_CloseFont(fDisplay64_); fDisplay64_ = nullptr; }
    if (fDisplay96_)   { TTF_CloseFont(fDisplay96_); fDisplay96_ = nullptr; }
    if (fBody16_)      { TTF_CloseFont(fBody16_); fBody16_ = nullptr; }
    if (fBody20_)      { TTF_CloseFont(fBody20_); fBody20_ = nullptr; }
    if (fDseg40_)      { TTF_CloseFont(fDseg40_); fDseg40_ = nullptr; }
    renderer_ = nullptr;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

TTF_Font* Hud::fontDisplay(int px) {
    if (px >= 88) return fDisplay96_ ? fDisplay96_ : fDisplay64_;
    return px >= 56 ? fDisplay64_ : fDisplay48_;
}

TTF_Font* Hud::fontBody(int px) {
    return px >= 20 ? fBody20_ : fBody16_;
}

TTF_Font* Hud::fontDigits() { return fDseg40_; }

SDL_Texture* Hud::textTexture(const std::string& cacheId, const char* text,
                              TTF_Font* font, SDL_Color color,
                              int* outW, int* outH) {
    // Reuse if the id matches and the text is the same.
    for (int i = 0; i < cacheCount_; i++) {
        if (cache_[i].id == cacheId && cache_[i].tex) {
            if (outW) *outW = cache_[i].w;
            if (outH) *outH = cache_[i].h;
            return cache_[i].tex;
        }
    }

    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;

    // Replace an existing entry with the same id, else append.
    int slot = -1;
    for (int i = 0; i < cacheCount_; i++) {
        if (cache_[i].id == cacheId) { slot = i; break; }
    }
    if (slot < 0 && cacheCount_ < 16) slot = cacheCount_++;
    if (slot < 0) { SDL_DestroyTexture(tex); return nullptr; }
    if (cache_[slot].tex) SDL_DestroyTexture(cache_[slot].tex);
    cache_[slot].id = cacheId;
    cache_[slot].tex = tex;
    cache_[slot].w = w;
    cache_[slot].h = h;

    if (outW) *outW = w;
    if (outH) *outH = h;
    return tex;
}

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------

void Hud::drawScore(int x, int y, int score) {
    // Panel
    const int boxW = 168, boxH = 92;
    SDL_Rect box = { x - boxW / 2, y - boxH / 2, boxW, boxH };
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, palette::panel.r, palette::panel.g, palette::panel.b, palette::panel.a);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, palette::orange.r, palette::orange.g,
                           palette::orange.b, 255);
    SDL_RenderDrawRect(renderer_, &box);

    // Label
    int lw = 0, lh = 0;
    SDL_Texture* label = textTexture("scorelabel", "SCORE", fBody16_,
                                     palette::amber, &lw, &lh);
    if (label) {
        SDL_Rect lr = { x - lw / 2, box.y + 8, lw, lh };
        SDL_RenderCopy(renderer_, label, nullptr, &lr);
    }

    // Digits (DSEG7).  Composed from ten pre-built digit textures so no
    // texture is ever created mid-run (that has been unreliable on hardware).
    int scoreVal = score < 0 ? -score : score;
    char buf[8];
    snprintf(buf, sizeof(buf), "%s%02d", score < 0 ? "-" : "", scoreVal % 100);

    int widths[4] = { 0, 0, 0, 0 };
    int heights[4] = { 0, 0, 0, 0 };
    SDL_Texture* glyphs[4] = { nullptr, nullptr, nullptr, nullptr };
    int n = 0;
    for (const char* c = buf; *c && n < 4; c++, n++) {
        int gw = 0, gh = 0;
        std::string id = (c[0] == '-') ? std::string("digitsign")
                                       : (std::string("digit") + c[0]);
        char txt[2] = { *c, 0 };
        glyphs[n] = textTexture(id, txt, fDseg40_, palette::ledRed, &gw, &gh);
        widths[n] = gw;
        heights[n] = gh;
    }

    int totalW = 0;
    for (int i = 0; i < n; i++) totalW += widths[i];
    int gx = x - totalW / 2;
    for (int i = 0; i < n; i++) {
        if (glyphs[i]) {
            SDL_Rect dr = { gx, box.y + 30, widths[i], heights[i] };
            SDL_RenderCopy(renderer_, glyphs[i], nullptr, &dr);
        }
        gx += widths[i];
    }
}

void Hud::drawHitLight(int x, int y, bool on) {
    SDL_Texture* tex = on ? texLightOn_ : texLightOff_;
    if (!tex) return;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = { x - 32, y, 64, 64 * h / (w ? w : 1) };
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
}

void Hud::drawAmmo(int xRight, int y, int ammoLeft) {
    if (!texBullet_ || ammoLeft <= 0) return;
    int w = 0, h = 0;
    SDL_QueryTexture(texBullet_, nullptr, nullptr, &w, &h);
    int bw = 20, bh = 20 * h / (w ? w : 1);
    for (int i = 0; i < ammoLeft; i++) {
        SDL_Rect dst = { xRight - (i + 1) * (bw + 4), y, bw, bh };
        SDL_RenderCopy(renderer_, texBullet_, nullptr, &dst);
    }
    int lw = 0, lh = 0;
    SDL_Texture* label = textTexture("ammolabel", "AMMO", fBody16_,
                                     palette::gray, &lw, &lh);
    if (label) {
        SDL_Rect lr = { xRight - lw, y + bh + 6, lw, lh };
        SDL_RenderCopy(renderer_, label, nullptr, &lr);
    }
}

void Hud::drawCrosshair(int cx, int cy) {
    if (!texCrosshair_) return;
    SDL_Rect dst = { cx - 20, cy - 20, 40, 40 };
    SDL_RenderCopy(renderer_, texCrosshair_, nullptr, &dst);
}

void Hud::drawToast(const char* text) {
    if (!text || !*text) return;
    int w = 0, h = 0;
    SDL_Texture* tex = textTexture("toast", text, fBody20_, palette::white,
                                   &w, &h);
    if (!tex) return;
    SDL_Rect bg = { (1280 - w) / 2 - 16, 720 - h - 34, w + 32, h + 14 };
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, palette::panel.r, palette::panel.g, palette::panel.b, 235);
    SDL_RenderFillRect(renderer_, &bg);
    SDL_Rect tr = { (1280 - w) / 2, 720 - h - 27, w, h };
    SDL_RenderCopy(renderer_, tex, nullptr, &tr);
}

void Hud::drawPulsingCentred(SDL_Texture* tex, int w, int h, int cy, float t) {
    if (!tex) return;
    Uint8 alpha = (Uint8)(128 + 127 * SDL_sin(t * 4.0));
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Rect dst = { (1280 - w) / 2, cy, w, h };
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
}
