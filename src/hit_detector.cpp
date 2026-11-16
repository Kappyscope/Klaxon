// hit_detector.cpp — 1:1 port of the Singe emulator.singe shot logic.
//
// Original (for reference):
//
//   function deadzone(intX, intY)
//       return intX == 0 or intY == 0 or intX > sensorX or intY > sensorY
//   end
//
//   function getPixelState(intX, intY)
//       if deadzone(intX, intY) then return pixelUnknown end
//       r1, g1, b1 = vldpGetPixel(intX, intY)
//       if ((255 - r1 < highThreshold) and (255 - g1 < highThreshold)
//           and (255 - b1 < highThreshold)) then return pixelHigh
//       elseif ((r1 < lowThreshold) and (g1 < lowThreshold)
//           and (b1 < lowThreshold)) then return pixelLow
//       else return pixelUnknown end
//   end
//
//   -- onInputPressed(BUTTON3) while playing:
//   triggerPulled = 2 ; shotsFired = shotsFired + 1
//
//   -- per frame while playing:
//   sensorLastState = sensorState
//   sensorState = getPixelState(sensorX, sensorY)
//   if triggerPulled > 0 and sensorLastState ~= sensorState then
//       if mouseX < sensorLeft or mouseY < sensorTop then
//           gunState = getPixelState(mouseX, mouseY)
//       else gunState = pixelUnknown end
//       if triggerPulled == 1 then
//           if all four states known and gunLastState ~= gunState then
//               if gunState == sensorState then GOOD else BAD
//           end
//       else gunLastState = gunState end
//       triggerPulled = triggerPulled - 1
//   end

#include "hit_detector.hpp"

void HitDetector::beginGame(const GamePlayfield& playfield) {
    pf_ = playfield;
    reset();
}

void HitDetector::reset() {
    triggerPulled_   = 0;
    sensorState_     = PIXEL_UNKNOWN;
    sensorLastState_ = PIXEL_UNKNOWN;
    gunState_        = PIXEL_UNKNOWN;
    gunLastState_    = PIXEL_UNKNOWN;
    shotsFired_ = 0;
    shotsGood_  = 0;
    shotsBad_   = 0;
}

void HitDetector::pullTrigger() {
    shotsFired_++;
    triggerPulled_ = 2;
    gunLastState_  = PIXEL_UNKNOWN;
}

PixelState HitDetector::getPixelState(PixelSampleFn sampleFn, void* user,
                                      int x, int y) const {
    // deadzone(): exactly the script's test.  x/y == 0 or beyond the
    // playfield extent carries no tape information.
    if (x == 0 || y == 0 || x > pf_.sensorX || y > pf_.sensorY) {
        return PIXEL_UNKNOWN;
    }

    int r = 0, g = 0, b = 0;
    sampleFn(user, x, y, &r, &g, &b);

    // Threshold classification, verbatim from the script.
    if ((255 - r < pf_.highThreshold) &&
        (255 - g < pf_.highThreshold) &&
        (255 - b < pf_.highThreshold)) {
        return PIXEL_HIGH;
    }
    if (r < pf_.lowThreshold && g < pf_.lowThreshold && b < pf_.lowThreshold) {
        return PIXEL_LOW;
    }
    return PIXEL_UNKNOWN;
}

ShotDecision HitDetector::onFrame(PixelSampleFn sampleFn, void* sampleUser,
                                  int aimX, int aimY) {
    sensorLastState_ = sensorState_;
    sensorState_ = getPixelState(sampleFn, sampleUser, pf_.sensorX, pf_.sensorY);

    if (triggerPulled_ <= 0) return SHOT_NONE;
    if (sensorLastState_ == sensorState_) return SHOT_NONE;  // no flip yet

    // Shooting the sensor patch itself is not a valid shot (bad user).
    if (aimX < pf_.sensorLeft || aimY < pf_.sensorTop) {
        gunState_ = getPixelState(sampleFn, sampleUser, aimX, aimY);
    } else {
        gunState_ = PIXEL_UNKNOWN;
    }

    ShotDecision decision = SHOT_NONE;

    if (triggerPulled_ == 1) {
        // Second flip: decide.
        if (gunState_        != PIXEL_UNKNOWN &&
            sensorState_     != PIXEL_UNKNOWN &&
            gunLastState_    != PIXEL_UNKNOWN &&
            sensorLastState_ != PIXEL_UNKNOWN) {
            if (gunLastState_ != gunState_) {
                if (gunState_ == sensorState_) {
                    shotsGood_++;
                    decision = SHOT_GOOD;
                } else {
                    shotsBad_++;
                    decision = SHOT_BAD;
                }
            } else {
                // Sensor and gun unchanged across the pair makes no sense.
                decision = SHOT_JUNK;
            }
        } else {
            decision = SHOT_JUNK;
        }
    } else {
        // First flip: remember for the second pass.
        gunLastState_ = gunState_;
    }

    triggerPulled_--;
    return decision;
}
