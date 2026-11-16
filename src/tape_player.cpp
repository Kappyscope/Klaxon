#include "tape_player.hpp"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <malloc.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include <SDL.h>
#include <coreinit/time.h>

#include "common.hpp"

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

TapePlayer::TapePlayer() = default;

TapePlayer::~TapePlayer() {
    shutdown();
}

bool TapePlayer::init(bool openAudio) {
    OSInitMutex(&lock_);

    // Audio ring: ~341 ms of stereo S16 pairs at 48 kHz.  Indices are
    // lock-free atomics — see mixerHook (no mutex ever near the callback).
    ringCap_ = 16384;
    ring_.reset(new int16_t[ringCap_ * 2]);
    ringRd_.store(0);
    ringWr_.store(0);
    ringCount_.store(0);

    // Audio device: plain SDL, our own callback, our own mixer.  SDL_mixer is
    // gone on purpose — see the header.  Note SDL_mixer used to initialise the
    // audio subsystem for us; now we must do it ourselves (SDL_Init in App
    // only brings up video).
    if (!openAudio) {
        KLAXON_LOG("audio: disabled by config (no device, no mixer)");
        audioOpen_ = false;
    } else if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        KLAXON_LOG("SDL_InitSubSystem(AUDIO) FAILED: %s", SDL_GetError());
    } else {
        KLAXON_LOG("audio subsystem init OK");

        SDL_AudioSpec want;
        SDL_zero(want);
        want.freq     = (int)deviceRate_;
        want.format   = AUDIO_S16SYS;
        want.channels = 2;
        want.samples  = 1024;
        want.callback = &TapePlayer::deviceCb;
        want.userdata = this;
        SDL_AudioSpec have;
        SDL_zero(have);
        devId_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (devId_ != 0) {
            audioOpen_ = true;
            if (have.freq > 0) deviceRate_ = (uint32_t)have.freq;
            KLAXON_LOG("audio device open: %u Hz S16 stereo (paused until startAudio)",
                       deviceRate_);
        } else {
            KLAXON_LOG("SDL_OpenAudioDevice FAILED (%s) - running silent",
                       SDL_GetError());
        }
    }

    // Decode thread on CPU2 (CPU0/1 belong to the OS and SDL's main loop).
    const uint32_t kStackSize = 256 * 1024;
    stack_ = (uint8_t*)memalign(16, kStackSize);
    if (!stack_) {
        KLAXON_LOG("decode thread stack alloc failed");
        return false;
    }
    if (!OSCreateThread(&thread_, &TapePlayer::threadEntry, 1,
                        (char*)this,   // wut passes argv through to the entry
                        stack_, kStackSize, 17,
                        OS_THREAD_ATTRIB_AFFINITY_CPU2)) {
        KLAXON_LOG("OSCreateThread failed");
        free(stack_);
        stack_ = nullptr;
        return false;
    }
    threadUp_ = true;
    OSResumeThread(&thread_);
    KLAXON_LOG("decode thread running on CPU2");
    return true;
}

void TapePlayer::shutdown() {
    if (threadUp_) {
        OSLockMutex(&lock_);
        quitThread_ = true;
        cmd_.action = Cmd::STOP;
        OSUnlockMutex(&lock_);

        int result = 0;
        OSJoinThread(&thread_, &result);
        threadUp_ = false;
    }
    if (stack_) {
        free(stack_);
        stack_ = nullptr;
    }

    const bool hadAudio = audioOpen_;
    if (hadAudio) {
        // Deliberately NOT SDL_CloseAudioDevice: closing takes the same device
        // lock that has deadlocked every SDL_mixer call on this stack.  Pause
        // instead and let process exit reclaim the device.  The ring buffer is
        // left allocated for the same reason - a paused driver must never be
        // able to read freed memory.
        SDL_PauseAudioDevice(devId_, 1);
        audioOpen_ = false;
        KLAXON_LOG("audio: device paused (not closed - device lock unsafe here)");
    }

    OSLockMutex(&lock_);
    closeSegmentLocked();
    OSUnlockMutex(&lock_);
    if (!hadAudio) {
        ring_.reset();
        ringCap_ = 0;
    }
}

// ---------------------------------------------------------------------------
// Public control (main thread)
// ---------------------------------------------------------------------------

bool TapePlayer::startIntro(const std::string& introPath, int introFrames) {
    OSLockMutex(&lock_);
    cmd_.action    = Cmd::LOAD;
    cmd_.path      = introPath;
    cmd_.phase     = TapePhase::INTRO;
    cmd_.maxFrames = introFrames;
    cmd_.loop      = false;
    phaseEndedFlag_ = false;
    openFailedFlag_ = false;
    OSUnlockMutex(&lock_);
    return true;
}

bool TapePlayer::startGame(const std::string& gamePath, int gameFrames) {
    OSLockMutex(&lock_);
    cmd_.action    = Cmd::LOAD;
    cmd_.path      = gamePath;
    cmd_.phase     = TapePhase::GAME;
    cmd_.maxFrames = gameFrames;
    cmd_.loop      = false;
    phaseEndedFlag_ = false;
    openFailedFlag_ = false;
    OSUnlockMutex(&lock_);
    return true;
}

bool TapePlayer::startMenuLoop(const std::string& menuPath) {
    OSLockMutex(&lock_);
    cmd_.action    = Cmd::LOAD;
    cmd_.path      = menuPath;
    cmd_.phase     = TapePhase::MENU;
    cmd_.maxFrames = 0;
    cmd_.loop      = true;
    phaseEndedFlag_ = false;
    openFailedFlag_ = false;
    OSUnlockMutex(&lock_);
    return true;
}

void TapePlayer::pause() {
    OSLockMutex(&lock_);
    paused_ = true;
    OSUnlockMutex(&lock_);
}

void TapePlayer::resume() {
    OSLockMutex(&lock_);
    paused_ = false;
    OSUnlockMutex(&lock_);
}

void TapePlayer::stop() {
    OSLockMutex(&lock_);
    cmd_.action = Cmd::STOP;
    phase_ = TapePhase::IDLE;
    OSUnlockMutex(&lock_);
}

bool TapePlayer::phaseEnded() {
    OSLockMutex(&lock_);
    bool e = phaseEndedFlag_;
    OSUnlockMutex(&lock_);
    return e;
}

bool TapePlayer::openFailed() {
    OSLockMutex(&lock_);
    bool f = openFailedFlag_;
    OSUnlockMutex(&lock_);
    return f;
}

bool TapePlayer::pollFrame() {
    OSLockMutex(&lock_);

    int best = -1;
    for (int i = 0; i < 3; i++) {
        if (slots_[i].filled && (best < 0 || slots_[i].frame > slots_[best].frame)) {
            best = i;
        }
    }
    if (best < 0) {
        OSUnlockMutex(&lock_);
        return false;
    }

    // Swap the newest slot's buffer with the held buffer: the old held frame
    // stays alive until the next poll, and the slot becomes free for reuse.
    heldBuf_.swap(slots_[best].buf);
    heldFrame_.rgba         = heldBuf_.get();
    heldFrame_.width        = slots_[best].width;
    heldFrame_.height       = slots_[best].height;
    heldFrame_.frameInPhase = slots_[best].frame;
    slots_[best].filled = false;

    OSUnlockMutex(&lock_);
    return true;
}

void TapePlayer::pixelProbe(void* user, int overlayX, int overlayY,
                            int* r, int* g, int* b) {
    TapePlayer* self = (TapePlayer*)user;
    if (!self->heldFrame_.rgba || self->heldFrame_.width <= 0 ||
        self->heldFrame_.height <= 0) {
        // No frame on screen: neutral mid-grey classifies as UNKNOWN under
        // the Singe thresholds, which is the correct "no signal" verdict.
        *r = 128; *g = 128; *b = 128;
        return;
    }

    int vx = overlayX * self->heldFrame_.width  / OVERLAY_W;
    int vy = overlayY * self->heldFrame_.height / OVERLAY_H;
    if (vx < 0) vx = 0;
    if (vy < 0) vy = 0;
    if (vx >= self->heldFrame_.width)  vx = self->heldFrame_.width - 1;
    if (vy >= self->heldFrame_.height) vy = self->heldFrame_.height - 1;

    const uint8_t* p = self->heldFrame_.rgba +
                       ((size_t)vy * self->heldFrame_.width + vx) * 4;
    *r = p[0];
    *g = p[1];
    *b = p[2];
}

uint32_t TapePlayer::audioUnderruns() const { return underruns_; }
uint32_t TapePlayer::framesDropped()  const { return dropped_; }
float    TapePlayer::decodeMsPerFrame() const { return decodeMs_; }

// ---------------------------------------------------------------------------
// Decode thread
// ---------------------------------------------------------------------------

int TapePlayer::threadEntry(int argc, const char** argv) {
    (void)argc;
    TapePlayer* self = (TapePlayer*)argv;   // wut passes the void* arg as argv
    self->decodeLoop();
    return 0;
}

void TapePlayer::closeSegmentLocked() {
    if (plm_) {
        plm_destroy(plm_);
        plm_ = nullptr;
    }
    currentPath_.clear();
    framesInPhase_ = 0;
    maxFrames_ = 0;
    loop_ = false;
    width_ = height_ = 0;
    lastVideoTime_ = -1.0;
    latestSlot_ = -1;
    for (int i = 0; i < 3; i++) slots_[i].filled = false;
}

bool TapePlayer::openSegmentLocked() {
    // cmd_ holds the load request.
    plm_t* plm = plm_create_with_filename(cmd_.path.c_str());
    if (!plm) {
        KLAXON_LOG("plm_create_with_filename failed: %s", cmd_.path.c_str());
        return false;
    }

    // Force the header to parse so width/height/samplerate are known.
    int guard = 0;
    while (plm_get_width(plm) <= 0 && !plm_has_ended(plm) && guard++ < 400) {
        plm_decode(plm, 0.005);
    }
    if (plm_get_width(plm) <= 0) {
        KLAXON_LOG("no video header in %s", cmd_.path.c_str());
        plm_destroy(plm);
        return false;
    }

    width_  = plm_get_width(plm);
    height_ = plm_get_height(plm);
    framesInPhase_ = 0;
    maxFrames_ = cmd_.maxFrames;
    loop_ = cmd_.loop;
    phase_ = cmd_.phase;
    currentPath_ = cmd_.path;
    lastVideoTime_ = -1.0;
    plm_set_loop(plm, loop_ ? 1 : 0);

    plm_set_video_decode_callback(plm, &TapePlayer::videoCb, this);
    plm_set_audio_decode_callback(plm, &TapePlayer::audioCb, this);

    uint32_t rate = (uint32_t)plm_get_samplerate(plm);
    if (rate > 8000 && rate < 96000) {
        streamRate_ = rate;
        resamplePos_ = 0.0;
    }

    KLAXON_LOG("segment open: %s phase=%d %dx%d audio=%u Hz maxFrames=%d loop=%d",
               cmd_.path.c_str(), (int)cmd_.phase, width_, height_, rate,
               maxFrames_, (int)loop_);

    plm_ = plm;
    return true;
}

void TapePlayer::decodeLoop() {
    const size_t kHighWater = 14000;  // stereo pairs (~292 ms at 48 kHz)

    for (;;) {
        // ---- command processing ----
        OSLockMutex(&lock_);
        if (quitThread_) {
            OSUnlockMutex(&lock_);
            break;
        }
        if (cmd_.action == Cmd::STOP) {
            // Stops the CURRENT segment, not the thread: the title screen
            // calls stop() on the way in, and a later startIntro() must still
            // work.  (Killing the thread here left every tape silent.)
            closeSegmentLocked();
            phase_ = TapePhase::IDLE;
            cmd_.action = Cmd::NONE;
            OSUnlockMutex(&lock_);
            continue;
        }
        if (cmd_.action == Cmd::LOAD) {
            closeSegmentLocked();
            bool ok = openSegmentLocked();
            openFailedFlag_ = !ok;
            cmd_.action = Cmd::NONE;
            if (!ok) phase_ = TapePhase::IDLE;
            OSUnlockMutex(&lock_);
            continue;
        }
        const bool paused = paused_;
        const bool ended  = phaseEndedFlag_;
        const bool haveStream = (plm_ != nullptr);
        OSUnlockMutex(&lock_);

        if (!haveStream || paused || ended) {
            OSSleepTicks(OSMillisecondsToTicks(6));
            continue;
        }

        // ---- audio-master pacing: don't run ahead of the device ----
        const size_t ringFill = ringCount_.load(std::memory_order_acquire);
        if (ringFill > kHighWater) {
            OSSleepTicks(OSMillisecondsToTicks(3));
            continue;
        }

        // ---- decode one step (~1/30 s of stream time) ----
        uint32_t t0 = SDL_GetTicks();
        plm_decode(plm_, 1.0 / 29.97);
        uint32_t dt = SDL_GetTicks() - t0;
        decodeMs_ = decodeMs_ * 0.9f + (float)dt * 0.1f;

        bool streamEnded = plm_has_ended(plm_) != 0;

        OSLockMutex(&lock_);
        if (maxFrames_ > 0 && framesInPhase_ >= maxFrames_) {
            // Authored cut point reached (intro / gameplay boundary).
            phaseEndedFlag_ = true;
        } else if (streamEnded && !loop_) {
            // Physical end of stream with no authored cut pending.
            phaseEndedFlag_ = true;
        }
        OSUnlockMutex(&lock_);
    }
}

// ---------------------------------------------------------------------------
// Video path (decode thread)
// ---------------------------------------------------------------------------

void TapePlayer::videoCb(plm_t* plm, plm_frame_t* frame, void* user) {
    (void)plm;
    TapePlayer* self = (TapePlayer*)user;

    OSLockMutex(&self->lock_);

    // Menu loop detection: pl_mpeg rewinds internally when looping; a
    // backwards jump in frame time means a new pass started.
    if (self->lastVideoTime_ >= 0 && frame->time < self->lastVideoTime_ - 0.05) {
        self->framesInPhase_ = 0;
    }
    self->lastVideoTime_ = frame->time;

    int f = self->framesInPhase_++;

    // Pick a slot: free one if available, else overwrite the oldest.
    int target = -1;
    int oldest = -1;
    for (int i = 0; i < 3; i++) {
        if (!self->slots_[i].filled) { target = i; break; }
        if (oldest < 0 || self->slots_[i].frame < self->slots_[oldest].frame) {
            oldest = i;
        }
    }
    if (target < 0) {
        target = oldest;
        self->dropped_++;
    }

    Slot& s = self->slots_[target];
    const int w = (int)frame->width;
    const int h = (int)frame->height;
    if (!s.buf || s.width != w || s.height != h) {
        s.buf.reset(new uint8_t[(size_t)w * h * 4]);
        s.width = w;
        s.height = h;
    }
    plm_frame_to_rgba(frame, s.buf.get(), w * 4);
    s.frame = f;
    s.filled = true;
    self->latestSlot_ = target;

    OSUnlockMutex(&self->lock_);
}

// ---------------------------------------------------------------------------
// Audio path
// ---------------------------------------------------------------------------

void TapePlayer::audioCb(plm_t* plm, plm_samples_t* samples, void* user) {
    (void)plm;
    TapePlayer* self = (TapePlayer*)user;
    self->pushAudio(samples->interleaved, (int)samples->count);
}

void TapePlayer::pushAudio(const float* interleaved, int frames) {
    const double rate = (streamRate_ > 0) ? (double)streamRate_
                                          : (double)deviceRate_;
    // Input frames consumed per output frame.  1.0 when the stream rate
    // matches the device (our prep pipeline guarantees this); anything else
    // falls back to nearest-neighbour resampling driven by a fractional
    // position that carries across callbacks.
    const double step = rate / (double)deviceRate_;

    double pos = resamplePos_;
    int produced = 0;
    const int kMaxProduced = frames * 2 + 8;  // runaway guard
    uint32_t wr = ringWr_.load(std::memory_order_relaxed);

    for (;;) {
        int idx = (int)pos;
        if (idx >= frames || produced >= kMaxProduced) break;
        if (ringCount_.load(std::memory_order_acquire) >= ringCap_) {
            dropped_++;
            break;
        }

        float l = interleaved[idx * 2 + 0];
        float r = interleaved[idx * 2 + 1];
        if (l >  1.0f) l =  1.0f;
        if (l < -1.0f) l = -1.0f;
        if (r >  1.0f) r =  1.0f;
        if (r < -1.0f) r = -1.0f;

        ring_[wr * 2 + 0] = (int16_t)(l * 32767.0f);
        ring_[wr * 2 + 1] = (int16_t)(r * 32767.0f);
        wr = (wr + 1) % ringCap_;
        ringWr_.store(wr, std::memory_order_release);
        ringCount_.fetch_add(1, std::memory_order_acq_rel);
        produced++;
        pos += step;
    }
    resamplePos_ = pos - (double)frames;  // carry into the next callback
}

// ---------------------------------------------------------------------------
// Audio: own mixer, no SDL_mixer
// ---------------------------------------------------------------------------

int TapePlayer::addSfx(const int16_t* pcm, uint32_t frames) {
    if (!pcm || frames == 0) return -1;
    if (sfxCount_ >= kMaxSfx) {
        KLAXON_LOG("audio: sfx table full, dropping clip");
        return -1;
    }
    const int id = sfxCount_++;
    sfxPcm_[id]    = pcm;
    sfxFrames_[id] = frames;
    return id;
}

void TapePlayer::playSfx(int id) {
    if (id < 0 || id >= sfxCount_) return;
    if (!audioOpen_) return;
    const uint32_t wr = trigWr_.load(std::memory_order_relaxed);
    const uint32_t rd = trigRd_.load(std::memory_order_acquire);
    if (wr - rd >= (uint32_t)kTrigCap) return;   // queue full: drop, never block
    trig_[wr & (uint32_t)(kTrigCap - 1)] = id;
    trigWr_.store(wr + 1, std::memory_order_release);
}

bool TapePlayer::sfxFinished(int id) {
    if (id < 0 || id >= 32) return false;
    const uint32_t bit = 1u << id;
    if (!(doneMask_.load(std::memory_order_acquire) & bit)) return false;
    doneMask_.fetch_and(~bit, std::memory_order_acq_rel);
    return true;
}

void TapePlayer::startAudio() {
    if (devId_ == 0) return;
    SDL_PauseAudioDevice(devId_, 0);
    KLAXON_LOG("audio: device running (own mixer, no SDL_mixer)");
}

void TapePlayer::startVoice(int id) {
    if (id < 0 || id >= sfxCount_) return;
    const int16_t* pcm = sfxPcm_[id];
    const uint32_t n   = sfxFrames_[id];
    if (!pcm || n == 0) return;

    int slot = -1;
    for (int i = 0; i < kMaxVoices; i++) {
        if (!voices_[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = 0;              // steal the first voice

    Voice& v = voices_[slot];
    const bool wasActive = v.active;
    v.pcm    = pcm;
    v.frames = n;
    v.pos    = 0;
    v.id     = id;
    v.active = true;
    if (!wasActive) voicesActive_.fetch_add(1, std::memory_order_relaxed);
}

void TapePlayer::deviceCb(void* user, uint8_t* stream, int len) {
    ((TapePlayer*)user)->mixInto(stream, len);
}

void TapePlayer::mixInto(uint8_t* stream, int len) {
    // Runs in SDL's audio context: NO locks, NO allocation, NO file I/O.
    periods_.fetch_add(1, std::memory_order_relaxed);

    int16_t* out = (int16_t*)stream;
    const int frames = len / 4;         // S16 stereo

    // 1) tape ring -> output (lock-free SPSC drain)
    uint32_t count = ringCount_.load(std::memory_order_acquire);
    uint32_t rd    = ringRd_.load(std::memory_order_relaxed);
    const int n = (frames < (int)count) ? frames : (int)count;
    const int16_t* ring = ring_.get();
    const uint32_t cap = (uint32_t)ringCap_;

    if (ring && cap > 0) {
        for (int i = 0; i < n; i++) {
            out[i * 2 + 0] = ring[rd * 2 + 0];
            out[i * 2 + 1] = ring[rd * 2 + 1];
            rd = (rd + 1 == cap) ? 0 : rd + 1;
        }
        if (n > 0) {
            ringRd_.store(rd, std::memory_order_release);
            ringCount_.fetch_sub((uint32_t)n, std::memory_order_acq_rel);
        }
    }
    for (int i = n * 2; i < frames * 2; i++) out[i] = 0;
    if (n < frames) underruns_.fetch_add(1, std::memory_order_relaxed);

    // 2) start queued one-shots
    uint32_t tw = trigWr_.load(std::memory_order_acquire);
    uint32_t tr = trigRd_.load(std::memory_order_relaxed);
    while (tr != tw) {
        startVoice(trig_[tr & (uint32_t)(kTrigCap - 1)]);
        tr++;
    }
    trigRd_.store(tr, std::memory_order_release);

    // 3) mix active voices over the tape audio (saturating add)
    for (int vi = 0; vi < kMaxVoices; vi++) {
        Voice& v = voices_[vi];
        if (!v.active) continue;
        int i = 0;
        for (; i < frames && v.pos < v.frames; i++, v.pos++) {
            int32_t l = (int32_t)out[i * 2 + 0] + (int32_t)v.pcm[v.pos * 2 + 0];
            int32_t r = (int32_t)out[i * 2 + 1] + (int32_t)v.pcm[v.pos * 2 + 1];
            out[i * 2 + 0] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
            out[i * 2 + 1] = (int16_t)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
        }
        if (v.pos >= v.frames) {
            if (v.id >= 0 && v.id < 32) {
                doneMask_.fetch_or(1u << v.id, std::memory_order_release);
            }
            v.active = false;
            v.id     = -1;
            v.pcm    = nullptr;
            voicesActive_.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

