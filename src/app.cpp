#include "app.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <SDL_image.h>
#include <SDL_ttf.h>

#include "common.hpp"
#include "res_loader.hpp"

// ---------------------------------------------------------------------------
// Screen constants
// ---------------------------------------------------------------------------
static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;
static const float GAME_OVER_SECONDS = 8.0f;

// The Singe title jingle: "Action Max" sting, then "A Steady Aim is Critical".
// Chaining happens in update() by polling the mixer's finish flag (there is no
// SDL_mixer callback any more).

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void App::loadSettings() {
    std::string path = ResolveAssetPath("config.txt");
    if (path.empty()) return;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(s.substr(0, eq));
        std::string val = Trim(s.substr(eq + 1));
        if (key == "aim_flip_y")      aimFlipY_   = (val == "1" || val == "true");
        else if (key == "video_dar_4_3")   videoDar43_ = (val == "1" || val == "true");
        else if (key == "full_media_verify") fullVerify_ = (val == "1" || val == "true");
        else if (key == "audio") audioEnabled_ = !(val == "0" || val == "false");
        else if (key == "title_level") titleLevel_ = ParseIntDefault(val, 9);
        else if (key == "autoplay") autoplay_ = (val == "1" || val == "true");
        else if (key == "dumpframe") dumpFrame_ = ParseIntDefault(val, -1);
    }
    fclose(f);
}

void App::stageSounds() {
    auto stage = [&](int slot, const char* rel) {
        std::string path = ResolveAssetPath(rel);
        if (path.empty()) {
            KLAXON_LOG("sound missing: %s", rel);
            sndStage_[slot] = -1;
            return;
        }
        sndStage_[slot] = ResStageWav(path);
    };
    stage(0, "assets/sounds/sound_gunshot.wav");
    stage(1, "assets/sounds/sound_goodhit.wav");
    stage(2, "assets/sounds/sound_badhit.wav");
    stage(3, "assets/sounds/sound_gameover.wav");
    stage(4, "assets/sounds/sound_getreadyforaction.wav");
    stage(5, "assets/sounds/sound_actionmax.wav");
    stage(6, "assets/sounds/sound_asteadyaimiscritical.wav");
    stage(7, "assets/sounds/sound_splash.wav");
}

void App::decodeSounds() {
    const uint32_t devRate = tape_.audioDeviceRate();
    KLAXON_LOG("decodeSounds: begin (device %u Hz)", devRate);

    sndClips_.clear();
    int* ids[8] = { &sndGunshot_, &sndGoodHit_, &sndBadHit_, &sndGameOver_,
                    &sndGetReady_, &sndActionMax_, &sndSteadyAim_, &sndSplash_ };

    for (int i = 0; i < 8; i++) {
        *ids[i] = -1;
        if (sndStage_[i] < 0) {
            KLAXON_LOG("sfx[%d]: not staged", i);
            continue;
        }
        KLAXON_LOG("sfx[%d]: decoding staged id %d", i, sndStage_[i]);
        std::unique_ptr<SfxClip> clip(new SfxClip());
        if (!ResDecodeWav(sndStage_[i], devRate, clip.get())) {
            KLAXON_LOG("sfx[%d]: decode FAILED", i);
            continue;
        }
        const int id = tape_.addSfx(clip->pcm.data(), clip->frames);
        if (id < 0) {
            KLAXON_LOG("sfx[%d]: addSfx FAILED", i);
            continue;
        }
        *ids[i] = id;
        sndClips_.push_back(std::move(clip));   // mixer holds a raw pointer
        KLAXON_LOG("sfx[%d]: ready id=%d frames=%u", i, id, sndClips_.back()->frames);
    }

    KLAXON_LOG("sounds ready: gun=%d good=%d bad=%d over=%d ready=%d "
               "jingle=%d steady=%d splash=%d",
               sndGunshot_, sndGoodHit_, sndBadHit_, sndGameOver_,
               sndGetReady_, sndActionMax_, sndSteadyAim_, sndSplash_);

    // Audio starts LAST: from here on nothing may read files or allocate.
    tape_.startAudio();
}

void App::warmTitleText() {
    const SDL_Color orange = { 255, 107, 26, 255 };
    const SDL_Color white  = { 235, 235, 240, 255 };
    const SDL_Color amber  = { 255, 210, 63, 255 };
    const SDL_Color gray   = { 170, 175, 185, 255 };
    const SDL_Color dim    = { 130, 135, 145, 255 };
    int w = 0, h = 0;

    hud_.textTexture("wordmark", "KLAXON", hud_.fontDisplay(96), orange, &w, &h);
    hud_.textTexture("titlesub", "A C T I O N   M A X", hud_.fontBody(20), white, &w, &h);
    hud_.textTexture("lastgametitle", "LAST GAME", hud_.fontBody(20), amber, &w, &h);

    char lines[4][96];
    snprintf(lines[0], sizeof(lines[0]), "Shots Fired: %d", lastShots_);
    snprintf(lines[1], sizeof(lines[1]), "Good Hits:   %d", lastGood_);
    snprintf(lines[2], sizeof(lines[2]), "Bad Hits:    %d", lastBad_);
    snprintf(lines[3], sizeof(lines[3]), "Game Score:  %d (%d%%)", 0, 0);
    for (int i = 0; i < 4; i++) {
        std::string id = std::string("lg") + std::to_string(i);
        hud_.textTexture(id, lines[i], hud_.fontBody(16), gray, &w, &h);
    }

    hud_.textTexture("pullstart", "PULL TRIGGER TO START", hud_.fontBody(20),
                     amber, &w, &h);
    hud_.textTexture("credit1", "Action Max scripts & assets by ScottD",
                     hud_.fontBody(16), dim, &w, &h);
    hud_.textTexture("credit2", "Emulation: Singe / DAPHNE",
                     hud_.fontBody(16), dim, &w, &h);
    hud_.textTexture("toast", "Hold - (Wiimote) to quit", hud_.fontBody(20),
                     white, &w, &h);

    // HUD widgets: score box, ten digits and the ammo label are built here so
    // the run loop never creates a texture (unreliable once audio is live).
    const SDL_Color ledRed = { 255, 42, 42, 255 };
    const SDL_Color hudGray = { 150, 155, 165, 255 };
    hud_.textTexture("scorelabel", "SCORE", hud_.fontBody(16), amber, &w, &h);
    for (int d = 0; d < 10; d++) {
        char key[12];
        snprintf(key, sizeof(key), "digit%d", d);
        char txt[2] = { (char)('0' + d), 0 };
        hud_.textTexture(key, txt, hud_.fontDigits(), ledRed, &w, &h);
    }
    hud_.textTexture("digitsign", "-", hud_.fontDigits(), ledRed, &w, &h);
    hud_.textTexture("ammolabel", "AMMO", hud_.fontBody(16), hudGray, &w, &h);
    KLAXON_LOG("hud: title text warmed (no texture creation after audio starts)");
}

void App::preloadTextures() {
    for (size_t i = 0; i < registry_.games().size() && i < 5; i++) {
        const GameConfig& cfg = registry_.games()[i];
        std::string bgPath  = ResolveAssetPath("assets/backgrounds/" + cfg.id + ".png");
        std::string artPath = ResolveAssetPath("assets/sprites/sprite_" + cfg.id + ".png");
        texBg_[i]  = bgPath.empty()  ? nullptr : ResLoadTexture(renderer_, bgPath);
        texArt_[i] = artPath.empty() ? nullptr : ResLoadTexture(renderer_, artPath);
        KLAXON_LOG("preload %-14s bg=%d art=%d", cfg.id.c_str(),
                   texBg_[i] != nullptr, texArt_[i] != nullptr);
    }
}

bool App::init() {
    KLAXON_LOG("=== app init begin ===");

    loadSettings();
    KLAXON_LOG("settings: aim_flip_y=%d video_dar_4_3=%d full_verify=%d audio=%d",
               (int)aimFlipY_, (int)videoDar43_, (int)fullVerify_, (int)audioEnabled_);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        KLAXON_LOG("SDL_Init(VIDEO) FAILED: %s", SDL_GetError());
        return false;
    }
    if (TTF_Init() != 0) {
        KLAXON_LOG("TTF_Init FAILED: %s", TTF_GetError());
        return false;
    }
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        KLAXON_LOG("IMG_Init FAILED: %s", IMG_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("Klaxon Action Max",
                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window_) {
        KLAXON_LOG("SDL_CreateWindow FAILED: %s", SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        KLAXON_LOG("accelerated renderer failed, trying software: %s", SDL_GetError());
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }
    if (!renderer_) {
        KLAXON_LOG("SDL_CreateRenderer FAILED: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // First paint so the screen is not garbage while assets load.
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);

    registry_.load(fullVerify_);

    // All asset file reads happen BEFORE the audio stack and decode thread
    // start: on hardware, file reads hang once SDL's audio thread is live.
    // Fonts/textures land in memory here; WAV files are staged as raw bytes.
    input_.init();
    hud_.init(renderer_);
    stageSounds();
    preloadTextures();

    // Every texture the title screen needs is built BEFORE the audio device
    // opens: creating textures while the mixer callback runs has crashed both
    // Cemu (host AV) and the console.  Same rule as the file reads.
    warmTitleText();

    // Audio device + decode thread LAST, after every file read is done.
    tape_.init(audioEnabled_);

    // Decode the staged WAVs into PCM now that the device exists (pure memory
    // work), then unpause audio.
    decodeSounds();

    state_ = (registry_.readyCount() > 0 && registry_.commonReady)
                 ? AppState::SPLASH
                 : AppState::NO_MEDIA;

    // User-provided boot jingle: plays under the KLAXON splash and into the
    // title screen; the original Singe title sting stays silent when it did.
    if (state_ == AppState::SPLASH && sndSplash_ >= 0) {
        tape_.playSfx(sndSplash_);
        splashJinglePlayed_ = true;
        titleJingleDone_ = true;
        KLAXON_LOG("splash jingle playing");
    }

    KLAXON_LOG("=== app init done (ready games: %d) ===", registry_.readyCount());
    return true;
}

void App::shutdown() {
    if (shutDown_) return;
    shutDown_ = true;

    KLAXON_LOG("shutdown: begin");

    tape_.shutdown();   // joins the decode thread, pauses the audio device

    sndClips_.clear();

    for (int i = 0; i < 5; i++) {
        if (texBg_[i])  { SDL_DestroyTexture(texBg_[i]);  texBg_[i] = nullptr; }
        if (texArt_[i]) { SDL_DestroyTexture(texArt_[i]); texArt_[i] = nullptr; }
    }

    if (texVideo_) { SDL_DestroyTexture(texVideo_); texVideo_ = nullptr; }
    hud_.shutdown();
    ResShutdown();   // free font buffers AFTER TTF_CloseFont ran in hud
    input_.shutdown();

    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_); window_ = nullptr; }

    IMG_Quit();
    TTF_Quit();

    // SDL_Quit() performs the single, canonical Wii U ProcUI exit handshake
    // (SYSLaunchMenu + pump until EXITING + GX2 shutdown).  Never call
    // SYSLaunchMenu() or ProcUIShutdown() ourselves - that is what
    // softlocked the old builds.
    KLAXON_LOG("shutdown: SDL_Quit (ProcUI exit handshake)");
    SDL_Quit();
    KLAXON_LOG("shutdown: done");
}

// ---------------------------------------------------------------------------
// Flow transitions
// ---------------------------------------------------------------------------

void App::enterTitle() {
    tape_.stop();
    state_ = AppState::TITLE;
    stateTime_ = 0.0f;
    inputLock_ = 0.30f;
    activeGame_ = -1;
    KLAXON_LOG("-> TITLE");

    // Title jingle once per title visit (Singe: on stateSetup).
    if (!titleJingleDone_ && sndActionMax_ >= 0) {
        tape_.playSfx(sndActionMax_);
        actionMaxId_ = sndActionMax_;
        actionMaxPlaying_ = true;
        titleJingleDone_ = true;
    }
}

void App::startSelectedGameIntro(bool limited) {
    const GameConfig& cfg = registry_.games()[selectedGame_];
    limitedMode_ = limited;
    ammoLeft_ = limited ? kAmmoCount : 0;

    GamePlayfield pf;
    pf.highThreshold = cfg.highThreshold;
    pf.lowThreshold  = cfg.lowThreshold;
    pf.sensorX = cfg.sensorX;
    pf.sensorY = cfg.sensorY;
    pf.sensorLeft = cfg.sensorLeft;
    pf.sensorTop  = cfg.sensorTop;
    detector_.beginGame(pf);

    activeGame_ = selectedGame_;
    lastShots_ = lastGood_ = lastBad_ = 0;

    tape_.startIntro(registry_.introMediaPath, cfg.lengthIntro);
    if (sndGetReady_ >= 0) tape_.playSfx(sndGetReady_);
    state_ = AppState::INTRO;
    stateTime_ = 0.0f;
    inputLock_ = 0.40f;
    KLAXON_LOG("intro: %s (limited=%d)", cfg.title.c_str(), (int)limited);
}

void App::finishGame(bool outOfAmmo) {
    const GameConfig& cfg = registry_.games()[activeGame_];
    lastShots_ = detector_.shotsFired();
    lastGood_  = detector_.shotsGood();
    lastBad_   = detector_.shotsBad();

    KLAXON_LOG("game over: %s shots=%d good=%d bad=%d (outOfAmmo=%d)",
               cfg.title.c_str(), lastShots_, lastGood_, lastBad_,
               (int)outOfAmmo);

    if (outOfAmmo && sndGameOver_ >= 0) tape_.playSfx(sndGameOver_);

    if (!registry_.menuMediaPath.empty()) {
        tape_.startMenuLoop(registry_.menuMediaPath);
    } else {
        tape_.stop();
    }
    state_ = AppState::GAME_OVER;
    stateTime_ = 0.0f;
    inputLock_ = 0.40f;
    detector_.reset();
}

void App::ensureVideoTexture(int w, int h) {
    if (texVideo_ && texVideoW_ == w && texVideoH_ == h) return;
    if (texVideo_) SDL_DestroyTexture(texVideo_);
    texVideo_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                  SDL_TEXTUREACCESS_STREAMING, w, h);
    if (texVideo_) {
        SDL_SetTextureBlendMode(texVideo_, SDL_BLENDMODE_NONE);
        texVideoW_ = w;
        texVideoH_ = h;
        KLAXON_LOG("video texture %dx%d", w, h);
    }
}

// ---------------------------------------------------------------------------
// Tape frame consumption (video texture + shot logic)
// ---------------------------------------------------------------------------

void App::aimToOverlay(int* ox, int* oy) const {
    const InputManager::State& in = input_.state();
    float ax = in.aimX;
    float ay = in.aimY;
    if (aimFlipY_) ay = 1.0f - ay;

    SDL_Rect vr;
    videoRect(&vr);
    // Screen -> video pixels -> Singe overlay (360x240, video is 2x overlay).
    int vx = (int)((ax * SCREEN_W - vr.x) * 720 / (float)vr.w);
    int vy = (int)((ay * SCREEN_H - vr.y) * 480 / (float)vr.h);
    *ox = vx / 2;
    *oy = vy / 2;
}

void App::consumeTapeFrames() {
    while (tape_.pollFrame()) {
        ensureVideoTexture(tape_.frame().width, tape_.frame().height);
        if (texVideo_ && tape_.frame().rgba) {
            int rc = SDL_UpdateTexture(texVideo_, nullptr, tape_.frame().rgba,
                                       tape_.frame().width * 4);
            // Numeric proof of the video path (no screenshots needed): the
            // centre and quarter points of the decoded frame.
            if (videoDbg_ < 6 || (videoDbg_ % 30) == 0) {
                const int vw = tape_.frame().width;
                const int vh = tape_.frame().height;
                const uint8_t* p  = tape_.frame().rgba;
                const uint8_t* c1 = p + ((size_t)(vh / 2) * vw + (vw / 2)) * 4;
                // Mean + peak luminance over the frame: proves whether a dark
                // screen is dark CONTENT or a dead video path.
                uint64_t sum = 0;
                int peak = 0;
                for (size_t i = 0; i < (size_t)vw * vh; i += 7) {
                    int lum = (p[i * 4] * 30 + p[i * 4 + 1] * 59 + p[i * 4 + 2] * 11) / 100;
                    sum += (uint64_t)lum;
                    if (lum > peak) peak = lum;
                }
                int mean = (int)(sum / (((size_t)vw * vh) / 7 + 1));
                KLAXON_LOG("video[%d] %dx%d upd=%d mid=%d,%d,%d mean=%d peak=%d",
                           videoDbg_, vw, vh, rc, c1[0], c1[1], c1[2], mean, peak);
            }
            // Optional single-frame dump of the DECODED buffer (never the
            // screen): config dumpframe=N.
            if (dumpFrame_ >= 0 && !dumped_ && videoDbg_ == dumpFrame_) {
                const int vw = tape_.frame().width, vh = tape_.frame().height;
                std::string path = ResolveAssetPath("frame.ppm");
                if (path.empty()) path = "/vol/external01/wiiu/apps/Klaxon-ActionMax/frame.ppm";
                FILE* f = fopen(path.c_str(), "wb");
                if (f) {
                    fprintf(f, "P6\n%d %d\n255\n", vw, vh);
                    const uint8_t* p = tape_.frame().rgba;
                    for (int i = 0; i < vw * vh; i++) {
                        fputc(p[i * 4], f); fputc(p[i * 4 + 1], f); fputc(p[i * 4 + 2], f);
                    }
                    fclose(f);
                    KLAXON_LOG("video: frame %d dumped to %s", videoDbg_, path.c_str());
                }
                dumped_ = true;
            }
            videoDbg_++;
        } else if (videoDbg_ < 6) {
            KLAXON_LOG("video[%d] no texture or no frame buffer", videoDbg_++);
        }

        if (state_ == AppState::PLAYING && activeGame_ >= 0) {
            int ox = 0, oy = 0;
            aimToOverlay(&ox, &oy);
            ShotDecision d = detector_.onFrame(&TapePlayer::pixelProbe,
                                               &tape_, ox, oy);
            if (d == SHOT_GOOD) {
                hitLightTimer_ = 0.5f;
                if (sndGoodHit_ >= 0) tape_.playSfx(sndGoodHit_);
                if (limitedMode_) ammoLeft_++;  // refund (net zero)
            } else if (d == SHOT_BAD) {
                if (sndBadHit_ >= 0) tape_.playSfx(sndBadHit_);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Main loop (ProcUI-aware; SDL drives ProcUI from SDL_PollEvent)
// ---------------------------------------------------------------------------

void App::run() {
    running_ = true;
    inForeground_ = true;
    Uint32 lastTicks = SDL_GetTicks();

    KLAXON_LOG("run: begin (state=%d)", (int)state_);

    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    KLAXON_LOG("SDL_QUIT received");
                    running_ = false;
                    break;
                case SDL_APP_WILLENTERBACKGROUND:
                case SDL_APP_DIDENTERBACKGROUND:
                    inForeground_ = false;
                    tape_.pause();
                    break;
                case SDL_APP_WILLENTERFOREGROUND:
                case SDL_APP_DIDENTERFOREGROUND:
                    inForeground_ = true;
                    tape_.resume();
                    lastTicks = SDL_GetTicks();
                    break;
                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED ||
                        ev.window.event == SDL_WINDOWEVENT_HIDDEN) {
                        inForeground_ = false;
                        tape_.pause();
                    } else if (ev.window.event == SDL_WINDOWEVENT_RESTORED ||
                               ev.window.event == SDL_WINDOWEVENT_SHOWN) {
                        inForeground_ = true;
                        tape_.resume();
                        lastTicks = SDL_GetTicks();
                    }
                    break;
                default:
                    break;
            }
        }
        if (!running_) break;

        // While the OS owns the display we must not touch the renderer.
        if (!inForeground_) {
            SDL_Delay(1);
            continue;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTicks) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastTicks = now;
        pulse_ += dt;

        input_.update(dt);
        if (input_.quitRequested()) {
            KLAXON_LOG("quit gesture (hold -)");
            running_ = false;
            break;
        }

        consumeTapeFrames();
        if (!running_) break;

        update(dt);
        if (!running_) break;
        render();
        frameCount_++;
        if (frameCount_ % 60 == 0) {
            KLAXON_LOG("alive: frame %d state=%d underruns=%u dropped=%u "
                       "audioPeriods=%u voices=%d",
                       frameCount_, (int)state_,
                       tape_.audioUnderruns(), tape_.framesDropped(),
                       tape_.audioPeriods(), tape_.activeVoices());
        }
    }

    KLAXON_LOG("run: exit");
    tape_.stop();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void App::update(float dt) {
    stateTime_ += dt;
    if (inputLock_ > 0.0f) {
        inputLock_ -= dt;
        if (inputLock_ < 0.0f) inputLock_ = 0.0f;
    }

    // Cemu testing aid: step through the menus without a Wii Remote.
    if (autoplay_) {
        autoTimer_ += dt;
        if (autoTimer_ >= 2.0f) {
            autoTimer_ = 0.0f;
            autoTrigger_ = true;
        }
    }

    // Title jingle chain: "Action Max" -> "A Steady Aim is Critical".
    // (No SDL_mixer callback: the mixer sets a flag the last time it plays
    // the clip, and we poll it here.)
    if (actionMaxPlaying_ && tape_.sfxFinished(actionMaxId_)) {
        if (sndSteadyAim_ >= 0) tape_.playSfx(sndSteadyAim_);
        actionMaxPlaying_ = false;
    }

    if (hitLightTimer_ > 0.0f) {
        hitLightTimer_ -= dt;
        if (hitLightTimer_ < 0.0f) hitLightTimer_ = 0.0f;
    }

    switch (state_) {
        case AppState::SPLASH:      updateSplash(dt); break;
        case AppState::TITLE:       updateTitle(dt); break;
        case AppState::GAME_SELECT: updateGameSelect(dt); break;
        case AppState::GAME_TYPE:   updateGameType(dt); break;
        case AppState::INTRO:       updateIntro(dt); break;
        case AppState::PLAYING:     updatePlaying(dt); break;
        case AppState::GAME_OVER:   updateGameOver(dt); break;
        case AppState::NO_MEDIA:    break;
    }

    autoTrigger_ = false;
}

void App::updateSplash(float) {
    if (stateTime_ > 1.6f) {
        enterTitle();
    }
}

void App::updateTitle(float) {
    if (triggerNow()) {
        state_ = AppState::GAME_SELECT;
        stateTime_ = 0.0f;
        inputLock_ = 0.30f;   // one press must not blow through the menus
        KLAXON_LOG("-> GAME_SELECT");
    }
}

void App::updateGameSelect(float) {
    const InputManager::State& in = input_.state();

    // Aim-driven highlight (gun-first UI), D-pad as backup.
    int n = (int)registry_.games().size();
    if (in.aiming) {
        int idx = (int)(in.aimX * (float)n * 1.15f) - 1;
        if (idx < 0) idx = 0;
        if (idx >= n) idx = n - 1;
        selectedGame_ = idx;
    }

    if (triggerNow()) {
        const GameConfig& cfg = registry_.games()[selectedGame_];
        if (!cfg.ready) {
            KLAXON_LOG("selected game not ready: %s (%s)",
                       cfg.id.c_str(), cfg.statusNote.c_str());
            return;  // stay; the card is marked MISSING on screen
        }
        KLAXON_LOG("-> GAME_TYPE (%s)", cfg.title.c_str());
        state_ = AppState::GAME_TYPE;
        stateTime_ = 0.0f;
        inputLock_ = 0.30f;
        if (!registry_.menuMediaPath.empty()) {
            tape_.startMenuLoop(registry_.menuMediaPath);
        }
    }
}

void App::updateGameType(float) {
    const InputManager::State& in = input_.state();
    if (triggerNow()) {
        bool limited = !in.aiming || in.aimX >= 0.5f;
        startSelectedGameIntro(limited);
    }
}

void App::updateIntro(float) {
    if (tape_.openFailed()) {
        KLAXON_LOG("intro media failed to open");
        enterTitle();
        return;
    }
    if (tape_.phaseEnded()) {
        const GameConfig& cfg = registry_.games()[activeGame_];
        tape_.startGame(cfg.mediaPath, cfg.lengthGame);
        state_ = AppState::PLAYING;
        stateTime_ = 0.0f;
        KLAXON_LOG("-> PLAYING");
    }
}

void App::updatePlaying(float dt) {
    (void)dt;
    const InputManager::State& in = input_.state();

    if (triggerNow()) {
        playChunk(sndGunshot_);
        detector_.pullTrigger();
        if (limitedMode_) {
            ammoLeft_--;
            if (ammoLeft_ < 0) {
                finishGame(true);   // Singe: out of ammo -> game over now
                return;
            }
        }
    }

    if (tape_.phaseEnded()) {
        finishGame(false);
    }
}

void App::updateGameOver(float) {
    if (stateTime_ > GAME_OVER_SECONDS || triggerNow()) {
        enterTitle();
    }
}

bool App::triggerNow() const {
    return inputLock_ <= 0.0f &&
           (input_.state().triggerPressed || autoTrigger_);
}

void App::playChunk(int id) {
    tape_.playSfx(id);
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------

void App::videoRect(SDL_Rect* out) const {
    if (videoDar43_) {
        // 720x480 NTSC DAR 4:3 -> 960x720 pillarboxed.
        out->x = (SCREEN_W - 960) / 2;
        out->y = 0;
        out->w = 960;
        out->h = 720;
    } else {
        out->x = 0;
        out->y = 0;
        out->w = SCREEN_W;
        out->h = SCREEN_H;
    }
}

void App::drawBackgroundStills(const GameConfig& cfg, int cx, int cy, int w, int h) {
    int idx = -1;
    for (size_t i = 0; i < registry_.games().size(); i++) {
        if (registry_.games()[i].id == cfg.id) { idx = (int)i; break; }
    }
    SDL_Texture* bg = (idx >= 0) ? texBg_[idx] : nullptr;
    if (bg) {
        SDL_Rect dst = { cx - w / 2, cy - h / 2, w, h };
        SDL_SetTextureAlphaMod(bg, 120);
        SDL_RenderCopy(renderer_, bg, nullptr, &dst);
    } else if (idx >= 0 && texArt_[idx]) {
        SDL_Rect dst = { cx - w / 4, cy - h / 4, w / 2, h / 2 };
        SDL_SetTextureAlphaMod(texArt_[idx], 90);
        SDL_RenderCopy(renderer_, texArt_[idx], nullptr, &dst);
    }
}

void App::drawVideo(const SDL_Rect& vr) {
    if (!texVideo_) return;
    SDL_RenderCopy(renderer_, texVideo_, nullptr, &vr);
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

void App::renderSplash() {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);

    int w = 0, h = 0;
    SDL_Color orange = { 255, 107, 26, 255 };
    SDL_Texture* mark = hud_.textTexture("wordmark", "KLAXON",
                                         hud_.fontDisplay(96), orange, &w, &h);
    if (mark) {
        float fade = stateTime_ < 0.5f ? stateTime_ / 0.5f
                    : (stateTime_ > 1.1f ? (1.6f - stateTime_) / 0.5f : 1.0f);
        if (fade < 0) fade = 0;
        SDL_SetTextureAlphaMod(mark, (Uint8)(255 * fade));
        SDL_Rect dst = { (SCREEN_W - w) / 2, (SCREEN_H - h) / 2 - 30, w, h };
        SDL_RenderCopy(renderer_, mark, nullptr, &dst);
    }

    int sw = 0, sh = 0;
    SDL_Color gray = { 150, 155, 165, 255 };
    SDL_Texture* sub = hud_.textTexture("splashsub", "AN ACTION MAX EXPERIENCE",
                                        hud_.fontBody(16), gray, &sw, &sh);
    if (sub) {
        float fade = stateTime_ < 0.5f ? stateTime_ / 0.5f
                    : (stateTime_ > 1.1f ? (1.6f - stateTime_) / 0.5f : 1.0f);
        if (fade < 0) fade = 0;
        SDL_SetTextureAlphaMod(sub, (Uint8)(255 * fade));
        SDL_Rect dst = { (SCREEN_W - sw) / 2, (SCREEN_H - sh) / 2 + 50, sw, sh };
        SDL_RenderCopy(renderer_, sub, nullptr, &dst);
    }
}

void App::renderTitle() {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);

    // Backdrop: deliberately no artwork.  A dark field and one orange rule
    // reads as an arcade attract screen and keeps the wordmark in focus.
    if (titleLevel_ >= 1) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        const int shades[4] = { 10, 12, 14, 16 };
        for (int i = 0; i < 4; i++) {
            SDL_Rect band = { 0, i * 180, SCREEN_W, 180 };
            SDL_SetRenderDrawColor(renderer_, (Uint8)shades[i],
                                   (Uint8)(shades[i] + 2),
                                   (Uint8)(shades[i] + 7), 255);
            SDL_RenderFillRect(renderer_, &band);
        }
        SDL_SetRenderDrawColor(renderer_, 255, 107, 26, 110);
        SDL_Rect rule = { SCREEN_W / 2 - 300, 252, 600, 2 };
        SDL_RenderFillRect(renderer_, &rule);
    }

    // Wordmark.
    if (titleLevel_ >= 2) {
    int w = 0, h = 0;
    SDL_Color orange = { 255, 107, 26, 255 };
    SDL_Texture* mark = hud_.textTexture("wordmark", "KLAXON",
                                         hud_.fontDisplay(96), orange, &w, &h);
    if (mark) {
        SDL_Rect dst = { (SCREEN_W - w) / 2, 110, w, h };
        SDL_RenderCopy(renderer_, mark, nullptr, &dst);
    }
    int sw = 0, sh = 0;
    SDL_Color white = { 235, 235, 240, 255 };
    SDL_Texture* sub = hud_.textTexture("titlesub", "A C T I O N   M A X",
                                        hud_.fontBody(20), white, &sw, &sh);
    if (sub) {
        SDL_Rect dst = { (SCREEN_W - sw) / 2, 110 + h + 8, sw, sh };
        SDL_RenderCopy(renderer_, sub, nullptr, &dst);
    }
    }  // titleLevel_ >= 2 (wordmark)

    // LAST GAME panel (top-left).
    if (titleLevel_ >= 3) {
        SDL_Rect panel = { 36, 300, 320, 170 };
        SDL_SetRenderDrawColor(renderer_, 10, 12, 20, 216);
        SDL_RenderFillRect(renderer_, &panel);
        SDL_SetRenderDrawColor(renderer_, 255, 107, 26, 255);
        SDL_RenderDrawRect(renderer_, &panel);

        int lw = 0, lh = 0;
        SDL_Color amber = { 255, 210, 63, 255 };
        SDL_Texture* t = hud_.textTexture("lastgametitle", "LAST GAME",
                                          hud_.fontBody(20), amber, &lw, &lh);
        if (t) {
            SDL_Rect dst = { panel.x + 16, panel.y + 12, lw, lh };
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }

        char buf[96];
        SDL_Color gray = { 170, 175, 185, 255 };
        char lines[4][96];
        snprintf(lines[0], sizeof(lines[0]), "Shots Fired: %d", lastShots_);
        snprintf(lines[1], sizeof(lines[1]), "Good Hits:   %d", lastGood_);
        snprintf(lines[2], sizeof(lines[2]), "Bad Hits:    %d", lastBad_);
        int shotScore = lastGood_ - lastBad_;
        int pct = (lastShots_ > 0) ? (100 * shotScore) / lastShots_ : 0;
        snprintf(lines[3], sizeof(lines[3]), "Game Score:  %d (%d%%)",
                 shotScore, pct);
        for (int i = 0; i < 4; i++) {
            std::string id = std::string("lg") + std::to_string(i);
            SDL_Texture* lt = hud_.textTexture(id, lines[i],
                                               hud_.fontBody(16), gray, &lw, &lh);
            if (lt) {
                SDL_Rect dst = { panel.x + 16, panel.y + 48 + i * 26, lw, lh };
                SDL_RenderCopy(renderer_, lt, nullptr, &dst);
            }
        }
    }

    // Pull trigger prompt (pulsing).
    if (titleLevel_ >= 4) {
        int pw = 0, ph = 0;
        SDL_Color amber = { 255, 210, 63, 255 };
        SDL_Texture* t = hud_.textTexture("pullstart", "PULL TRIGGER TO START",
                                          hud_.fontBody(20), amber, &pw, &ph);
        hud_.drawPulsingCentred(t, pw, ph, SCREEN_H - 130, pulse_);
    }

    // Credits (two short lines: one wide texture was upsetting the renderer).
    if (titleLevel_ >= 5) {
        int cw = 0, ch = 0;
        SDL_Color gray = { 130, 135, 145, 255 };
        const char* l1 = "Action Max scripts & assets by ScottD";
        const char* l2 = "Emulation: Singe / DAPHNE";
        SDL_Texture* t1 = hud_.textTexture("credit1", l1, hud_.fontBody(16),
                                           gray, &cw, &ch);
        if (t1) {
            SDL_Rect dst = { (SCREEN_W - cw) / 2, SCREEN_H - 58, cw, ch };
            SDL_RenderCopy(renderer_, t1, nullptr, &dst);
        }
        SDL_Texture* t2 = hud_.textTexture("credit2", l2, hud_.fontBody(16),
                                           gray, &cw, &ch);
        if (t2) {
            SDL_Rect dst = { (SCREEN_W - cw) / 2, SCREEN_H - 40, cw, ch };
            SDL_RenderCopy(renderer_, t2, nullptr, &dst);
        }
    }

    if (titleLevel_ >= 6) hud_.drawToast("Hold - (Wiimote) to quit");
}

void App::renderGameSelect() {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);

    int w = 0, h = 0;
    SDL_Color orange = { 255, 107, 26, 255 };
    SDL_Texture* head = hud_.textTexture("selecthead", "SELECT GAME",
                                         hud_.fontDisplay(48), orange, &w, &h);
    if (head) {
        SDL_Rect dst = { (SCREEN_W - w) / 2, 42, w, h };
        SDL_RenderCopy(renderer_, head, nullptr, &dst);
    }

    const int n = (int)registry_.games().size();
    const int cardW = 210, cardH = 300;
    int gap = (SCREEN_W - n * cardW) / (n + 1);
    int y = (SCREEN_H - cardH) / 2 + 10;

    for (int i = 0; i < n; i++) {
        const GameConfig& cfg = registry_.games()[i];
        int x = gap + i * (cardW + gap);
        bool sel = (i == selectedGame_);
        int yy = y - (sel ? 8 : 0);

        // Card panel.
        SDL_Rect card = { x, yy, cardW, cardH };
        SDL_SetRenderDrawColor(renderer_, 10, 12, 20, 230);
        SDL_RenderFillRect(renderer_, &card);

        SDL_Color border = sel ? SDL_Color{ 255, 107, 26, 255 }
                               : SDL_Color{ 60, 64, 76, 255 };
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer_, &card);
        if (sel) {  // double border for glow
            SDL_Rect outer = { x - 3, yy - 3, cardW + 6, cardH + 6 };
            SDL_RenderDrawRect(renderer_, &outer);
        }

        // Box art (preloaded at boot).
        if (texArt_[i]) {
            SDL_Rect artRect = { x + 5, yy + 5, cardW - 10, cardH - 50 };
            SDL_RenderCopy(renderer_, texArt_[i], nullptr, &artRect);
        }

        // Title + readiness.
        int lw = 0, lh = 0;
        SDL_Color tc = cfg.ready ? SDL_Color{ 235, 235, 240, 255 }
                                 : SDL_Color{ 130, 60, 60, 255 };
        std::string id = "cardtitle" + std::to_string(i);
        SDL_Texture* t = hud_.textTexture(id, cfg.title.c_str(),
                                          hud_.fontBody(16), tc, &lw, &lh);
        if (t) {
            SDL_Rect dst = { x + (cardW - lw) / 2, yy + cardH - 40, lw, lh };
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }
        if (!cfg.ready) {
            SDL_Texture* miss = hud_.textTexture("missing", "MEDIA MISSING",
                                                 hud_.fontBody(16),
                                                 SDL_Color{ 255, 80, 80, 255 },
                                                 &lw, &lh);
            if (miss) {
                SDL_Rect dst = { x + (cardW - lw) / 2, yy + cardH - 20, lw, lh };
                SDL_RenderCopy(renderer_, miss, nullptr, &dst);
            }
        }
    }

    hud_.drawToast("AIM at a game - PULL B to play");
}

void App::renderGameType() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_Rect vr;
    videoRect(&vr);
    drawVideo(vr);

    // Faithful Singe halves: left = Standard, right = Limited Ammo.
    const InputManager::State& in = input_.state();
    bool rightSel = !in.aiming || in.aimX >= 0.5f;

    SDL_Rect left  = { vr.x + 20, vr.y + 220, vr.w / 2 - 40, 220 };
    SDL_Rect right = { vr.x + vr.w / 2 + 20, vr.y + 220, vr.w / 2 - 40, 220 };

    auto drawPanel = [&](const SDL_Rect& r, bool sel, const char* heading,
                         const char* l1, const char* l2, const char* idh,
                         const char* id1, const char* id2) {
        SDL_SetRenderDrawColor(renderer_, 10, 12, 20, 200);
        SDL_RenderFillRect(renderer_, &r);
        SDL_Color border = sel ? SDL_Color{ 255, 107, 26, 255 }
                               : SDL_Color{ 60, 64, 76, 255 };
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer_, &r);

        int w = 0, h = 0;
        SDL_Color white = { 235, 235, 240, 255 };
        SDL_Color amber = { 255, 210, 63, 255 };
        SDL_Color gray  = { 170, 175, 185, 255 };
        SDL_Texture* ht = hud_.textTexture(idh, heading,
                                           hud_.fontDisplay(48), amber, &w, &h);
        if (ht) {
            SDL_Rect dst = { r.x + (r.w - w) / 2, r.y + 24, w, h };
            SDL_RenderCopy(renderer_, ht, nullptr, &dst);
        }
        SDL_Texture* t1 = hud_.textTexture(id1, l1, hud_.fontBody(16),
                                           white, &w, &h);
        if (t1) {
            SDL_Rect dst = { r.x + (r.w - w) / 2, r.y + 110, w, h };
            SDL_RenderCopy(renderer_, t1, nullptr, &dst);
        }
        SDL_Texture* t2 = hud_.textTexture(id2, l2, hud_.fontBody(16),
                                           gray, &w, &h);
        if (t2) {
            SDL_Rect dst = { r.x + (r.w - w) / 2, r.y + 136, w, h };
            SDL_RenderCopy(renderer_, t2, nullptr, &dst);
        }
    };

    drawPanel(left, !rightSel, "STANDARD",
              "Shoot the bad guys.", "Unlimited ammo.", "stdh", "std1", "std2");
    drawPanel(right, rightSel, "LIMITED AMMO",
              "5 misses and you're done.", "Good hits refund.", "limh", "lim1", "lim2");

    int hw = 0, hh = 0;
    SDL_Texture* head = hud_.textTexture("typetitle", "SELECT GAME TYPE",
                                         hud_.fontDisplay(48),
                                         SDL_Color{ 255, 107, 26, 255 },
                                         &hw, &hh);
    if (head) {
        SDL_Rect dst = { (SCREEN_W - hw) / 2, 40, hw, hh };
        SDL_RenderCopy(renderer_, head, nullptr, &dst);
    }

    hud_.drawToast("AIM left for Standard, right for Limited - PULL B");
}

void App::renderVideoScreen(bool hud) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_Rect vr;
    videoRect(&vr);
    drawVideo(vr);

    if (!hud) return;

    const bool playing = (state_ == AppState::PLAYING);

    // Left pillar bar: hit light (top) + LED score (bottom left).
    hud_.drawHitLight(90, 40, hitLightTimer_ > 0.0f);
    if (playing) {
        hud_.drawScore(84, SCREEN_H - 120, detector_.score());
    }

    // Right pillar bar: ammo (limited mode).
    if (playing && limitedMode_) {
        hud_.drawAmmo(SCREEN_W - 24, 40, ammoLeft_);
    }

    // Crosshair (aim position, any state with a remote).
    const InputManager::State& in = input_.state();
    if (in.aiming) {
        hud_.drawCrosshair((int)(in.aimX * SCREEN_W),
                           (int)(in.aimY * SCREEN_H));
    }

    if (state_ == AppState::INTRO) {
        int w = 0, h = 0;
        SDL_Texture* t = hud_.textTexture("getready", "GET READY!",
                                          hud_.fontDisplay(48),
                                          SDL_Color{ 255, 210, 63, 255 },
                                          &w, &h);
        hud_.drawPulsingCentred(t, w, h, SCREEN_H - 150, pulse_);
    }
}

void App::renderGameOver() {
    renderVideoScreen(false);

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 170);
    SDL_Rect full = { 0, 0, SCREEN_W, SCREEN_H };
    SDL_RenderFillRect(renderer_, &full);

    int w = 0, h = 0;
    SDL_Texture* go = hud_.textTexture("gameover", "GAME OVER",
                                       hud_.fontDisplay(96),
                                       SDL_Color{ 255, 42, 42, 255 }, &w, &h);
    if (go) {
        SDL_Rect dst = { (SCREEN_W - w) / 2, 120, w, h };
        SDL_RenderCopy(renderer_, go, nullptr, &dst);
    }

    const GameConfig* cfg = activeGame_ >= 0
        ? &registry_.games()[activeGame_] : nullptr;

    char buf[96];
    SDL_Color white = { 235, 235, 240, 255 };
    int lw = 0, lh = 0;
    int y = 260;
    struct { const char* id; std::string text; } rows[6];
    rows[0] = { "go0", cfg ? cfg->title : "" };
    snprintf(buf, sizeof(buf), "Shots Fired: %d", lastShots_);
    rows[1] = { "go1", buf };
    snprintf(buf, sizeof(buf), "Good Hits:   %d", lastGood_);
    rows[2] = { "go2", buf };
    snprintf(buf, sizeof(buf), "Bad Hits:    %d", lastBad_);
    rows[3] = { "go3", buf };
    int shotScore = lastGood_ - lastBad_;
    int pct = (lastShots_ > 0) ? (100 * shotScore) / lastShots_ : 0;
    snprintf(buf, sizeof(buf), "Game Score:  %d  (%d%%)", shotScore, pct);
    rows[4] = { "go4", buf };
    rows[5] = { "go5", limitedMode_ ? "Mode: Limited Ammo" : "Mode: Standard" };

    for (int i = 0; i < 6; i++) {
        SDL_Texture* t = hud_.textTexture(rows[i].id, rows[i].text.c_str(),
                                          hud_.fontBody(20), white, &lw, &lh);
        if (t) {
            SDL_Rect dst = { (SCREEN_W - lw) / 2, y, lw, lh };
            SDL_RenderCopy(renderer_, t, nullptr, &dst);
        }
        y += 36;
    }

    int pw = 0, ph = 0;
    SDL_Texture* t = hud_.textTexture("gocont", "PULL B TO CONTINUE",
                                      hud_.fontBody(20),
                                      SDL_Color{ 255, 210, 63, 255 }, &pw, &ph);
    hud_.drawPulsingCentred(t, pw, ph, SCREEN_H - 110, pulse_);
}

void App::renderNoMedia() {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);

    int w = 0, h = 0;
    SDL_Texture* head = hud_.textTexture("nomedia", "NO READY GAMES",
                                         hud_.fontDisplay(48),
                                         SDL_Color{ 255, 80, 80, 255 }, &w, &h);
    if (head) {
        SDL_Rect dst = { (SCREEN_W - w) / 2, 140, w, h };
        SDL_RenderCopy(renderer_, head, nullptr, &dst);
    }

    int y = 260;
    int lw = 0, lh = 0;
    SDL_Color gray = { 170, 175, 185, 255 };
    SDL_Texture* t = hud_.textTexture(
        "noline1",
        "Copy the tape media to <SD>:/wiiu/apps/Klaxon-ActionMax/media/",
        hud_.fontBody(16), gray, &lw, &lh);
    if (t) {
        SDL_Rect dst = { (SCREEN_W - lw) / 2, y, lw, lh };
        SDL_RenderCopy(renderer_, t, nullptr, &dst);
    }
    t = hud_.textTexture(
        "noline2",
        "Get the tapes from archive.org and run tools/prepare_media.ps1 (see README).",
        hud_.fontBody(16), gray, &lw, &lh);
    if (t) {
        SDL_Rect dst = { (SCREEN_W - lw) / 2, y + 30, lw, lh };
        SDL_RenderCopy(renderer_, t, nullptr, &dst);
    }

    // Per-game status.
    y += 90;
    for (const auto& g : registry_.games()) {
        std::string id = "nogame" + g.id;
        std::string line = g.id + ": " + g.statusNote;
        SDL_Texture* lt = hud_.textTexture(id, line.c_str(),
                                           hud_.fontBody(16), gray, &lw, &lh);
        if (lt) {
            SDL_Rect dst = { 200, y, lw, lh };
            SDL_RenderCopy(renderer_, lt, nullptr, &dst);
        }
        y += 26;
    }

    hud_.drawToast("Hold - (Wiimote) to quit");
}

void App::render() {
    switch (state_) {
        case AppState::SPLASH:      renderSplash(); break;
        case AppState::TITLE:       renderTitle(); break;
        case AppState::GAME_SELECT: renderGameSelect(); break;
        case AppState::GAME_TYPE:   renderGameType(); break;
        case AppState::INTRO:       renderVideoScreen(true); break;
        case AppState::PLAYING:     renderVideoScreen(true); break;
        case AppState::GAME_OVER:   renderGameOver(); break;
        case AppState::NO_MEDIA:    renderNoMedia(); break;
    }
    SDL_RenderPresent(renderer_);
}
