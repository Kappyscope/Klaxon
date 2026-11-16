#include "input.hpp"

#include <cmath>

#include <padscore/kpad.h>
#include <vpad/input.h>
#include <coreinit/time.h>

#include "common.hpp"

// Hide the crosshair after this many consecutive lost IR samples (~250 ms
// at 60 fps), matching the old build's proven behaviour.
static const int IR_HOLD_FRAMES = 30;

// Quit gesture: hold "-" on the Wii Remote (+ on the GamePad) this long.
static const float QUIT_HOLD_SECONDS = 1.2f;

// Median of three: kills single-sample IR spikes without adding lag.
static float Median3(const float v[3]) {
    float a = v[0], b = v[1], c = v[2], t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}

bool InputManager::init() {
    VPADInit();
    KPADInit();
    for (int i = 0; i < 4; i++) {
        KPADEnableDPD((KPADChan)i);
    }
    initialised_ = true;
    KLAXON_LOG("input: KPAD/VPAD init, DPD enabled on 4 channels");
    return true;
}

void InputManager::shutdown() {
    // No KPADShutdown() here: it must run after SDL's audio/ProcUI teardown
    // and Aroma reclaims input at process end.  (Learned from the old build.)
    initialised_ = false;
}

void InputManager::update(float dt) {
    if (!initialised_) return;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;

    bool quitHeld = false;

    // ---- GamePad: passive, only the quit gesture is honoured ----
    {
        VPADStatus vpad;
        VPADReadError vErr;
        int32_t n = VPADRead(VPAD_CHAN_0, &vpad, 1, &vErr);
        if (n > 0 && vErr == VPAD_READ_SUCCESS) {
            if (vpad.hold & VPAD_BUTTON_PLUS) quitHeld = true;
        }
    }

    // ---- Wii Remotes: channels 0..3 ----
    st_.connected = false;
    st_.aiming = false;
    st_.triggerPressed = false;
    st_.triggerHeld = false;
    st_.channel = -1;

    int firstConnected = -1;
    int bestChan = -1;

    for (int c = 0; c < 4; c++) {
        Chan& ch = chans_[c];
        KPADStatus kbuf[8];
        KPADError err;
        // Read up to 8 queued samples and use the newest: reading a single
        // status can miss button edges when the remote reports in bursts.
        uint32_t n = KPADReadEx((KPADChan)c, kbuf, 8, &err);

        if (n > 0 && err == KPAD_ERROR_OK) {
            KPADStatus k = kbuf[n - 1];
            ch.connected = true;

            bool wasMinus = ch.heldMinus;
            ch.heldMinus = (k.hold & WPAD_BUTTON_MINUS) != 0;
            if (ch.heldMinus && !wasMinus) {
                KLAXON_LOG("input: '-' pressed (ch %d)", c);
            }

            bool wasB = ch.heldB;
            bool wasA = ch.heldA;
            ch.heldB = (k.hold & WPAD_BUTTON_B) != 0;
            ch.heldA = (k.hold & WPAD_BUTTON_A) != 0;

            // IR aim: KPAD reports pos in [-1,+1]; +x right, +y down, which
            // already matches screen space (0 = top).
            float nx = (k.pos.x + 1.0f) * 0.5f;
            float ny = (k.pos.y + 1.0f) * 0.5f;

            bool valid = std::isfinite(nx) && std::isfinite(ny) &&
                         nx >= -0.1f && nx <= 1.1f &&
                         ny >= -0.1f && ny <= 1.1f;
            if (valid && k.posValid == 0 && ch.posValid) {
                // A packet with no valid IR that still carries coordinates is
                // usually stale garbage: reject it when it jumps.
                if (std::fabs(nx - ch.posX) > 0.2f ||
                    std::fabs(ny - ch.posY) > 0.2f) {
                    valid = false;
                }
            }

            if (valid) {
                if (nx < 0.0f) nx = 0.0f;
                if (nx > 1.0f) nx = 1.0f;
                if (ny < 0.0f) ny = 0.0f;
                if (ny > 1.0f) ny = 1.0f;

                if (ch.posValid) {
                    // Median of the last three raw samples, then a slew limit
                    // and low-pass.  Raw IR alternates between the two sensor
                    // bar LEDs, which is what made the crosshair "seize".
                    ch.rawX[ch.rawIdx] = nx;
                    ch.rawY[ch.rawIdx] = ny;
                    ch.rawIdx = (ch.rawIdx + 1) % 3;
                    const float mx = Median3(ch.rawX);
                    const float my = Median3(ch.rawY);

                    const float kMaxStep = 0.06f;   // ~6% of the screen / frame
                    float dx = mx - ch.posX;
                    float dy = my - ch.posY;
                    // Sub-threshold movement is jitter, not aim.
                    if (std::fabs(dx) < 0.010f) dx = 0.0f;
                    if (std::fabs(dy) < 0.010f) dy = 0.0f;
                    if (dx >  kMaxStep) dx =  kMaxStep;
                    if (dx < -kMaxStep) dx = -kMaxStep;
                    if (dy >  kMaxStep) dy =  kMaxStep;
                    if (dy < -kMaxStep) dy = -kMaxStep;
                    ch.posX += dx * 0.6f;
                    ch.posY += dy * 0.6f;
                } else {
                    ch.posX = nx;
                    ch.posY = ny;
                    ch.rawX[0] = ch.rawX[1] = ch.rawX[2] = nx;
                    ch.rawY[0] = ch.rawY[1] = ch.rawY[2] = ny;
                }
                ch.posValid = true;
                ch.lostFrames = 0;
            } else {
                ch.posValid = false;
                if (ch.lostFrames < IR_HOLD_FRAMES) ch.lostFrames++;
            }

            if (k.hold & WPAD_BUTTON_MINUS) quitHeld = true;

            if (firstConnected < 0) firstConnected = c;
            bool pressing = (!wasB && ch.heldB) || (!wasA && ch.heldA);
            if (bestChan < 0 && (pressing || (ch.posValid && c == 0))) {
                bestChan = c;
            }
            // Remember edges per channel so nothing is missed.
            if (c == 0) {
                st_.triggerPressed = pressing;
                st_.triggerIsB = (!wasB && ch.heldB);
            } else if (pressing && !st_.triggerPressed) {
                st_.triggerPressed = true;
                st_.triggerIsB = (!wasB && ch.heldB);
                bestChan = c;
            }
        } else {
            ch.connected = false;
            ch.posValid = false;
            ch.heldB = ch.heldA = ch.heldMinus = false;
        }
    }

    // Prefer a remote that just fired; otherwise channel 0 if connected;
    // otherwise the first connected remote (aim display).
    int active = bestChan >= 0 ? bestChan
                 : (chans_[0].connected ? 0 : firstConnected);
    if (active >= 0) {
        const Chan& ch = chans_[active];
        st_.connected = true;
        st_.channel = active;
        st_.aiming = ch.posValid || (ch.lostFrames < IR_HOLD_FRAMES);
        st_.aimX = ch.posX;
        st_.aimY = ch.posY;
        st_.triggerHeld = ch.heldB || ch.heldA;
        // Trigger edges were already aggregated per channel above.
    }

    // ---- quit / back gestures ----
    //   tap "-" (under 0.5 s)  -> back to the title from a tape
    //   hold "-" (1.2 s)       -> quit the app
    backRequested_ = false;
    if (quitHeld) {
        quitHold_ += dt;
        if (quitHold_ >= QUIT_HOLD_SECONDS) quitRequested_ = true;
    } else {
        if (quitHold_ > 0.0f && quitHold_ < 0.5f) backRequested_ = true;
        quitHold_ = 0.0f;
    }
}
