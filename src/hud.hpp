// hud.hpp — the redesigned in-game overlay: LED score counter (bottom-left),
// hit light, ammo display, crosshair, and small helper drawing utilities.
//
// Design language (Klaxon UI redesign):
//   * dark charcoal panels with 1px action-orange borders
//   * DSEG7 red LED digits for the score (the counter the user asked for)
//   * Chakra Petch for labels/body, Orbitron for headings
//   * HUD lives in the pillarbox bars around the 4:3 picture
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>

class Hud {
public:
    bool init(SDL_Renderer* renderer);
    void shutdown();

    // Fonts
    TTF_Font* fontDisplay(int px);   // Orbitron
    TTF_Font* fontBody(int px);      // Chakra Petch
    TTF_Font* fontDigits();          // DSEG7 seven-segment

    // Cached-text helper: renders text once, reuses the texture.
    // Returns a texture the HUD owns until the same cache id is re-set.
    SDL_Texture* textTexture(const std::string& cacheId, const char* text,
                             TTF_Font* font, SDL_Color color,
                             int* outW, int* outH);

    // ---- widgets ----
    // Score counter: black box, orange border, DSEG7 red digits.
    // (x, y) is the box centre.
    void drawScore(int x, int y, int score);

    // Hit light sprite (Singe assets), drawn top-centre of (x, y).
    void drawHitLight(int x, int y, bool on);

    // Ammo bullets row (Limited mode), right-aligned ending at (x, y).
    void drawAmmo(int xRight, int y, int ammoLeft);

    // Crosshair at screen coordinates.
    void drawCrosshair(int cx, int cy);

    // One-line toast at the bottom of the screen (errors / hints).
    void drawToast(const char* text);

    // Big centred pulsing text ("PULL TRIGGER TO START").
    void drawPulsingCentred(SDL_Texture* tex, int w, int h, int cy, float t);

    SDL_Texture* texCrosshair()  const { return texCrosshair_; }
    SDL_Texture* texLightOn()    const { return texLightOn_; }
    SDL_Texture* texLightOff()   const { return texLightOff_; }
    SDL_Texture* texBullet()     const { return texBullet_; }
    SDL_Texture* texLogo()       const { return texLogo_; }

    // Draws the KLAXON wordmark scaled to `width`, centred on (cx, top).
    void drawLogo(int cx, int top, int width);

private:
    SDL_Texture* loadTex(const char* rel);

    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* fDisplay48_ = nullptr;
    TTF_Font* fDisplay64_ = nullptr;
    TTF_Font* fDisplay96_ = nullptr;
    TTF_Font* fBody16_ = nullptr;
    TTF_Font* fBody20_ = nullptr;
    TTF_Font* fDseg40_ = nullptr;

    SDL_Texture* texCrosshair_ = nullptr;
    SDL_Texture* texLightOn_   = nullptr;
    SDL_Texture* texLightOff_  = nullptr;
    SDL_Texture* texBullet_    = nullptr;
    SDL_Texture* texLogo_      = nullptr;

    // tiny text cache
    struct CachedText {
        std::string id;
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };
    CachedText cache_[32];
    int cacheCount_ = 0;
};
