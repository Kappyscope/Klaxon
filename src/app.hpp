// app.hpp — Klaxon application: screens, game flow, ProcUI-safe main loop.
//
// Flow (faithful to the Singe state machine, presentation redesigned):
//   SPLASH -> TITLE -> GAME_SELECT -> GAME_TYPE -> INTRO -> PLAYING
//                                                       ^          |
//                                                       |          v
//                                                       +------ GAME_OVER
//   TITLE shows the backgroundFrame still of the last selected game and the
//   LAST GAME stats panel; GAME_TYPE plays the menu tape with the two
//   Singe-faithful halves (Standard / Limited Ammo); GAME_OVER plays the menu
//   tape under the results.  Hold "-" on the Wii Remote to quit cleanly.
#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

#include "game_registry.hpp"
#include "hit_detector.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "tape_player.hpp"

enum class AppState {
    SPLASH,
    TITLE,
    GAME_SELECT,
    GAME_TYPE,
    INTRO,
    PLAYING,
    GAME_OVER,
    NO_MEDIA,
};

class App {
public:
    bool init();
    void run();
    void shutdown();

    // Title jingle chaining (called from the SDL_mixer channel-finished hook).
    void onChannelFinished(int channel);

private:
    // ---- flow ----
    void enterTitle();
    void startSelectedGameIntro(bool limited);
    void finishGame(bool outOfAmmo);
    void ensureVideoTexture(int w, int h);

    // ---- per-frame ----
    void consumeTapeFrames();      // texture update + shot logic (PLAYING)
    void update(float dt);
    void render();

    // ---- screens ----
    void updateSplash(float dt);
    void updateTitle(float dt);
    void updateGameSelect(float dt);
    void updateGameType(float dt);
    void updateIntro(float dt);
    void updatePlaying(float dt);
    void updateGameOver(float dt);

    void renderSplash();
    void renderTitle();
    void renderGameSelect();
    void renderGameType();
    void renderVideoScreen(bool hud);   // INTRO/PLAYING (+GAME_OVER base)
    void renderGameOver();
    void renderNoMedia();

    // ---- helpers ----
    void videoRect(SDL_Rect* out) const;
    void aimToOverlay(int* ox, int* oy) const;
    void playChunk(Mix_Chunk* chunk);
    bool loadSounds();
    void loadSettings();
    void drawBackgroundStills(const GameConfig& cfg, int cx, int cy, int w, int h);
    void drawVideo(const SDL_Rect& vr);

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  texVideo_ = nullptr;
    int texVideoW_ = 0, texVideoH_ = 0;

    Hud         hud_;
    TapePlayer  tape_;
    InputManager input_;
    GameRegistry registry_;

    AppState state_ = AppState::SPLASH;
    float stateTime_ = 0.0f;
    float pulse_ = 0.0f;

    int  selectedGame_ = 0;
    int  activeGame_   = -1;
    bool limitedMode_  = false;
    int  ammoLeft_     = 0;
    static const int kAmmoCount = 5;

    HitDetector detector_;

    // SFX (Singe asset set)
    Mix_Chunk* sndGunshot_  = nullptr;
    Mix_Chunk* sndGoodHit_  = nullptr;
    Mix_Chunk* sndBadHit_   = nullptr;
    Mix_Chunk* sndGameOver_ = nullptr;
    Mix_Chunk* sndGetReady_ = nullptr;
    Mix_Chunk* sndActionMax_ = nullptr;
    Mix_Chunk* sndSteadyAim_ = nullptr;
    Mix_Chunk* sndSplash_    = nullptr;   // user-provided boot jingle (SPLASH)
    int    actionMaxChannel_ = -1;
    float  hitLightTimer_ = 0.0f;
    bool   titleJingleDone_ = false;
    bool   splashJinglePlayed_ = false;

    // last-game scoreboard (session)
    int lastShots_ = 0, lastGood_ = 0, lastBad_ = 0;

    // settings
    bool aimFlipY_    = false;
    bool videoDar43_  = true;
    bool fullVerify_  = false;

    bool running_     = false;
    bool inForeground_ = true;
    bool shutDown_    = false;
};
