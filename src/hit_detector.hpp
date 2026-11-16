// hit_detector.hpp — the Action Max hit test, a faithful port of the original
// Singe `emulator.singe` script (statePlaying block + getPixelState).
//
// How the real hardware works: the tape flashes a fixed "suction cup" patch
// on the TV between black and white.  The gun's suction-cup sensor watches
// that patch; on every state FLIP of the patch the console samples the light
// state under the gun's aim point.  A shot is armed on trigger pull and
// decided on the SECOND flip: if the aim pixel classified the same way the
// sensor pixel did (and actually changed between the two samples), it's a
// good hit; a confident mismatch is a bad hit (you shot a friend).
#pragma once

#include <string>

// Mirrors the Singe script: pixelLow = 0, pixelHigh = 1, pixelUnknown = 2.
enum PixelState {
    PIXEL_LOW     = 0,
    PIXEL_HIGH    = 1,
    PIXEL_UNKNOWN = 2
};

// Playfield geometry, in the Singe overlay space (360x240).
struct GamePlayfield {
    int highThreshold;   // "close to white" tolerance (255 - channel < t)
    int lowThreshold;    // "close to black" tolerance (channel < t)
    int sensorX;         // playfield width extent  (deadzone beyond this)
    int sensorY;         // playfield height extent (deadzone beyond this)
    int sensorLeft;      // suction-cup sensor box: aim with x < sensorLeft
    int sensorTop;       // ... and y < sensorTop is required for a shot
};

// Pixel provider: samples one pixel of the CURRENT tape frame in overlay
// coordinates (raw R/G/B, no classification).  Implemented by the tape player
// (reads the decoded RGBA) — the exact equivalent of Singe's vldpGetPixel.
typedef void (*PixelSampleFn)(void* user, int overlayX, int overlayY,
                              int* r, int* g, int* b);

enum ShotDecision {
    SHOT_NONE   = 0,
    SHOT_GOOD   = 1,
    SHOT_BAD    = 2,
    SHOT_JUNK   = 3   // sample pair was ambiguous; no score change
};

class HitDetector {
public:
    void beginGame(const GamePlayfield& playfield);

    // Trigger pull (the Singe script: gunshot + shotsFired++ + arm 2 samples).
    void pullTrigger();

    // Called once for every new tape frame.  aimX/aimY are in overlay space.
    // Returns what happened on this frame.
    ShotDecision onFrame(PixelSampleFn sampleFn, void* sampleUser,
                         int aimX, int aimY);

    void reset();

    // The Singe script's score is shotsGood - shotsBad (can go negative).
    int  shotsFired() const { return shotsFired_; }
    int  shotsGood()  const { return shotsGood_; }
    int  shotsBad()   const { return shotsBad_; }
    int  score()      const { return shotsGood_ - shotsBad_; }

private:
    // The script's getPixelState(): deadzone + threshold classification.
    PixelState getPixelState(PixelSampleFn sampleFn, void* user,
                             int x, int y) const;

    GamePlayfield pf_{};
    int  triggerPulled_ = 0;
    PixelState sensorState_     = PIXEL_UNKNOWN;
    PixelState sensorLastState_ = PIXEL_UNKNOWN;
    PixelState gunState_        = PIXEL_UNKNOWN;
    PixelState gunLastState_    = PIXEL_UNKNOWN;
    int shotsFired_ = 0;
    int shotsGood_  = 0;
    int shotsBad_   = 0;
};
