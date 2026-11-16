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

#include <memory>
#include <vector>

#include <SDL.h>

#include "game_registry.hpp"
#include "hit_detector.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "res_loader.hpp"
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
    void playChunk(int id);
    bool triggerNow() const;   // trigger, gated by the post-transition lock
    void stageSounds();
    void decodeSounds();
    void preloadTextures();
    void warmTitleText();
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

    // SFX (Singe asset set) — ids into TapePlayer's own mixer, -1 = absent.
    int sndGunshot_  = -1;
    int sndGoodHit_  = -1;
    int sndBadHit_   = -1;
    int sndGameOver_ = -1;
    int sndGetReady_ = -1;
    int sndActionMax_ = -1;
    int sndSteadyAim_ = -1;
    int sndSplash_    = -1;   // user-provided boot jingle (SPLASH)
    int sndStage_[8]  = {};     // staged file reads (pre-audio)
    // PCM backing store: the mixer holds raw pointers, so these must outlive it.
    std::vector<std::unique_ptr<SfxClip>> sndClips_;

    // Preloaded textures (read at boot, zero file I/O during the run loop)
    SDL_Texture* texBg_[5]  = {};   // title background stills
    SDL_Texture* texArt_[5] = {};   // box arts
    int    actionMaxId_ = -1;   // the clip whose end chains "A Steady Aim..."
    bool   actionMaxPlaying_ = false;
    float  hitLightTimer_ = 0.0f;
    bool   titleJingleDone_ = false;
    bool   splashJinglePlayed_ = false;

    // last-game scoreboard (session)
    int lastShots_ = 0, lastGood_ = 0, lastBad_ = 0;

    // settings
    bool aimFlipY_    = false;
    bool videoDar43_  = true;
    bool fullVerify_  = false;
    bool audioEnabled_ = true;   // config: audio=0 skips the device (bisect)
    bool autoplay_     = false;  // config: autoplay=1 drives the menus itself (Cemu testing)
    bool autoTrigger_  = false;
    float autoTimer_   = 0.0f;
    int  titleLevel_   = 9;      // config: title_level=N draws only the first N parts

    bool running_     = false;
    bool inForeground_ = true;
    float inputLock_  = 0.0f;   // ignore the trigger just after a state change
    int   videoDbg_   = 0;      // decoded-frame log counter
    bool  videoStatic_ = true;  // wut: streaming textures upload "ok" but never show
    int   videoMode_   = 0;     // 0 = update, 1 = texture from surface, 2 = lock/unlock
    int   dumpFrame_  = -1;     // config: dumpframe=N writes that decoded frame
    bool  dumped_     = false;
    bool shutDown_    = false;
    int  frameCount_  = 0;
};
