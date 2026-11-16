// tape_player.hpp — plays the Action Max "tape": intro segment, gameplay
// segment, menu segment — one muxed MPEG-1 System file each (pl_mpeg).
//
// Design:
//   * A dedicated decode thread runs on CPU2 (the OS cores are 0/1; SDL's
//     main loop lives on 1).  It drives pl_mpeg and feeds two consumers:
//       - AUDIO: pl_mpeg audio callback -> S16 ring buffer; SDL_mixer's
//         Mix_HookMusic drains the ring on the audio device thread.  The
//         audio device IS the master clock (the same way the VHS head was).
//       - VIDEO: pl_mpeg video callback -> latest-wins RGBA frame slots.
//         The main thread polls the newest frame; slower rendering simply
//         drops old frames, never blocks the decoder.
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

    // Opens the mixer (48 kHz S16 stereo) + Mix_HookMusic + decode thread.
    bool init();
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

    // Diagnostics.
    uint32_t audioUnderruns() const;
    uint32_t framesDropped()  const;
    float    decodeMsPerFrame() const;

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

    static void videoCb(plm_t* plm, plm_frame_t* frame, void* user);
    static void audioCb(plm_t* plm, plm_samples_t* samples, void* user);
    static void mixerHook(void* udata, uint8_t* stream, int len);
    static int  threadEntry(int argc, const char** argv);

    void decodeLoop();
    bool openSegmentLocked();          // lock_ held; uses pending cmd
    void closeSegmentLocked();
    void pushAudio(const float* interleaved, int frames);

    // --- state (guarded by lock_ unless noted) -----------------------------
    OSMutex  lock_{};
    OSThread thread_{};
    bool     threadUp_ = false;
    uint8_t* stack_ = nullptr;   // decode-thread stack (freed in shutdown)

    Cmd      cmd_;                // pending command
    bool     paused_ = false;
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

    // decode stats
    float decodeMs_ = 0.0f;
};
