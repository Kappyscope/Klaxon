// input.hpp — Wii Remote (KPAD) input: IR aiming, B trigger, quit gesture.
//
// The Wii Remote IS the Action Max gun (user's spec): the IR camera gives
// the aim point, the B trigger fires.  All four channels are polled; the
// first connected remote with a valid aim wins.  The GamePad is passive
// ("PadCon") — it mirrors the TV picture via SDL and is not a gun.
#pragma once

#include <cstdint>

class InputManager {
public:
    bool init();          // VPADInit / KPADInit / KPADEnableDPD
    void shutdown();
    void update(float dt);

    struct State {
        bool connected       = false;
        bool aiming          = false;   // valid IR sample on screen
        float aimX           = 0.5f;    // [0,1] screen space, 0 = left
        float aimY           = 0.5f;    // [0,1] screen space, 0 = top
        bool triggerPressed  = false;   // edge this frame (B or A)
        bool triggerHeld     = false;
        bool triggerIsB      = true;    // which button fired (telemetry)
        int  channel         = -1;      // KPAD channel of the active remote
    };

    const State& state() const { return st_; }
    bool quitRequested() const { return quitRequested_; }

private:
    struct Chan {
        bool  connected = false;
        bool  heldB = false;
        bool  heldA = false;
        float posX = 0.5f, posY = 0.5f;
        bool  posValid = false;
        int   lostFrames = 0;
    };

    State st_{};
    Chan chans_[4];
    bool  quitRequested_ = false;
    float quitHold_ = 0.0f;
    bool  initialised_ = false;
};
