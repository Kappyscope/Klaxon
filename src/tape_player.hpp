// tape_player.hpp — plays the Action Max "tape": intro segment, gameplay
// segment, menu segment — one muxed MPEG-1 System file each (pl_mpeg), and
// owns the audio device (tape PCM + one-shot SFX mixed in one callback).
//
// Design:
//   * A dedicated decode thread runs on CPU2 (the OS cores are 0/1; SDL's
//     main loop lives on 1).  It drives pl_mpeg and feeds two consumers:
//       - AUDIO: pl_mpeg audio callback -> S16 ring buffer, drained by our
//         own SDL audio callback.  The device IS the master clock.
//       - VIDEO: pl_mpeg video callback -> latest-wins RGBA frame slots.
//         The main thread polls the newest frame; slower rendering simply
//         drops old frames, never blocks the decoder.
//   * NO SDL_mixer.  Every SDL_mixer entry point (Mix_LoadWAV, Mix_QuickLoad
//     RAW, Mix_PlayChannel) takes the audio-device lock, and on this port a
//     take of that lock after the callback thread is live deadlocks: the
//     main thread stops with no log line.  SFX are decoded to plain S16 PCM
//     at boot and triggered through a lock-free queue that the audio
//     callback drains.
//   * Segment timeline mirrors the Singe scripts: the intro segment is cut
//     at `lengthIntro` authored frames even if the file carries a few more,
//     gameplay runs for exactly `lengthGame` frames, the menu loops.
//   * Pixel readback (for the hit detector) samples the held frame — the
//     same frame the user sees, exactly like vldpGetPixel in Singe.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <coreinit/mutex.h>
#include <coreinit/thread.h>

#include <atomic>

#include <SDL.h>

#include "hit_detector.hpp"

struct plm_t;
struct plm_frame_t;
struct plm_samples_t;

enum class TapePhase { IDLE, INTRO, GAME, MENU };

struct TapeFrame {
    const uint8_t* rgba  = nullptr;   // RGBA8888 (byte order R,G,B,A)
    int width   = 0;
    int height  = 0;
    int frameInPhase = 0;             // 0-based decoded frames this phase
};

class TapePlayer {
public:
    TapePlayer();
    ~TapePlayer();

    // Opens the audio device (paused) + decode thread.  Nothing is audible
    // until startAudio().  openAudio=false skips the device entirely (bisect):
    // the app then runs silent and playSfx() is a no-op.
    bool init(bool openAudio = true);
    void shutdown();

    // Tape phases.  The app issues each LOAD when the previous phase ends
    // (see phaseEnded()); the intro's authored cut point is cfg.lengthIntro,
    // gameplay's is cfg.lengthGame, the menu loops forever.
    bool startIntro(const std::string& introPath, int introFrames);
    bool startGame(const std::string& gamePath, int gameFrames);
    bool startMenuLoop(const std::string& menuPath);

    void pause();
    void resume();
    void stop();

    // State queries (main thread).
    bool      active() const { return phase_ != TapePhase::IDLE; }
    TapePhase phase()  const { return phase_; }
    // True when the current phase reached its authored end (intro cut or
    // game end).  Menu phases never end (they loop).
    bool phaseEnded();
    bool openFailed();   // last load failed (missing/corrupt file)

    // Main thread: returns true once when a newer frame exists; the newest
    // frame becomes the held frame (used for render + pixel sampling).
    bool pollFrame();
    const TapeFrame& frame() const { return heldFrame_; }

    // PixelSampleFn adapter for HitDetector: raw RGB of one pixel of the
    // held frame, overlay 360x240 coordinates (Singe's vldpGetPixel).
    static void pixelProbe(void* user, int overlayX, int overlayY,
                           int* r, int* g, int* b);

    // --- audio engine (own mixer; SDL_mixer is deliberately absent) --------
    // Registers an S16 stereo PCM clip at the device rate.  The caller keeps
    // the samples alive for the app's lifetime; returns an SFX id or -1.
    int  addSfx(const int16_t* pcm, uint32_t frames);

    // Fires a one-shot from the main thread (lock-free, never blocks).
    void playSfx(int id);

    // True exactly once, after the last callback that finished this clip.
    bool sfxFinished(int id);

    // Unpauses the device.  Called once everything else is staged, so no
    // file I/O or allocation can ever happen once audio is running.
    void startAudio();

    // Diagnostics.
    uint32_t audioUnderruns() const;
    uint32_t framesDropped()  const;
    float    decodeMsPerFrame() const;
    uint32_t audioDeviceRate() const { return deviceRate_; }
    uint32_t audioPeriods()   const { return periods_.load(std::memory_order_relaxed); }
    int      activeVoices()   const { return voicesActive_.load(std::memory_order_relaxed); }

private:
    // --- decode-thread side ------------------------------------------------
    struct Cmd {
        enum Action { NONE, LOAD, STOP } action = NONE;
        std::string path;
        TapePhase   phase      = TapePhase::IDLE;
        int         maxFrames  = 0;     // 0 = run to stream end
        bool        loop       = false; // menu behaviour
    };

    struct Slot {
        std::unique_ptr<uint8_t[]> buf;
        int width = 0, height = 0, frame = 0;
        bool filled = false;
    };

    struct Voice {
        const int16_t* pcm = nullptr;
        uint32_t frames = 0;
        uint32_t pos = 0;
        int  id = -1;
        bool active = false;
    };

    static const int kMaxVoices = 8;
    static const int kMaxSfx    = 16;
    static const int kTrigCap   = 32;   // power of two

    static void videoCb(plm_t* plm, plm_frame_t* frame, void* user);
    static void audioCb(plm_t* plm, plm_samples_t* samples, void* user);
    static void SDLCALL deviceCb(void* user, uint8_t* stream, int len);
    static int  threadEntry(int argc, const char** argv);

    void decodeLoop();
    bool openSegmentLocked();          // lock_ held; uses pending cmd
    void closeSegmentLocked();
    void pushAudio(const float* interleaved, int frames);

    // --- audio-thread side (NO locks, NO allocation, NO file I/O) ----------
    void mixInto(uint8_t* stream, int len);
    void startVoice(int id);

    // --- state (guarded by lock_ unless noted) -----------------------------
    OSMutex  lock_{};
    OSThread thread_{};
    bool     threadUp_ = false;
    std::atomic<bool> threadDone_{false};   // set by the decode thread on exit
    uint8_t* stack_ = nullptr;   // decode-thread stack (freed in shutdown)

    Cmd      cmd_;                // pending command
    bool     paused_ = false;
    bool     quitThread_ = false; // shutdown only: decode thread returns
    bool     audioOpen_ = false;
    TapePhase phase_ = TapePhase::IDLE;
    bool     phaseEndedFlag_ = false;
    bool     openFailedFlag_ = false;

    // decode-thread members
    plm_t*   plm_ = nullptr;
    std::string currentPath_;
    int  framesInPhase_ = 0;
    int  maxFrames_ = 0;
    bool loop_ = false;
    int  width_ = 0, height_ = 0;
    double lastVideoTime_ = -1.0;
    int  latestSlot_ = -1;        // newest filled slot, -1 none

    // frame slots (latest-wins)
    Slot slots_[3];

    // held frame (main thread only, no lock needed)
    std::unique_ptr<uint8_t[]> heldBuf_;
    TapeFrame heldFrame_{};

    // --- audio ring (LOCK-FREE: the mixer callback runs in SDL's audio
    // context where blocking on a mutex wedges the audio thread on real
    // hardware — atomics only, no OSMutex anywhere near the callback) ---
    std::unique_ptr<int16_t[]> ring_;
    size_t ringCap_ = 0;          // stereo sample-pairs
    std::atomic<uint32_t> ringRd_{0};
    std::atomic<uint32_t> ringWr_{0};
    std::atomic<uint32_t> ringCount_{0};
    std::atomic<uint32_t> underruns_{0};
    std::atomic<uint32_t> dropped_{0};
    uint32_t deviceRate_ = 48000;
    uint32_t streamRate_ = 48000;
    double   resamplePos_ = 0.0;

    // --- audio device + own mixer -----------------------------------------
    SDL_AudioDeviceID devId_ = 0;
    const int16_t* sfxPcm_[kMaxSfx] = {};
    uint32_t       sfxFrames_[kMaxSfx] = {};
    int            sfxCount_ = 0;
    Voice          voices_[kMaxVoices];
    std::atomic<uint32_t> trigWr_{0};     // main thread -> audio thread
    std::atomic<uint32_t> trigRd_{0};
    int                   trig_[kTrigCap] = {};
    std::atomic<uint32_t> doneMask_{0};   // bit per sfx id, set by callback
    std::atomic<uint32_t> periods_{0};
    std::atomic<int>      voicesActive_{0};

    // decode stats
    float decodeMs_ = 0.0f;
};
