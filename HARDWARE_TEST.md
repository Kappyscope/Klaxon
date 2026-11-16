# HARDWARE_TEST — Klaxon on a real Wii U

Cemu cannot exercise the ProcUI transitions or real KPAD IR, so this list is
what only your console can verify. Work top to bottom; each item says what
"pass" looks like.

## 0. Install

1. Copy `dist/Klaxon-ActionMax/` (whole folder) to `<SD>:/wiiu/apps/`.
2. Boot Aroma → Wii U Menu → the **Klaxon** tile appears with its icon.
   - PASS: tile visible; launching shows the KLAXON splash.
   - FAIL: grab `klaxon.log` from the app folder.

## 1. Boot sanity

3. App opens to the KLAXON splash (~1.6 s) → TITLE: blue wordmark, subtitle,
   LAST GAME panel, pulsing "PULL TRIGGER TO START".
4. `klaxon.log` contains `decodeSounds: done (8 clips)`, `audio device open:
   48000 Hz`, `sounds ready: gun=0 … splash=7`, `decode thread running on CPU2`.

## 2. HOME / exit (the softlock killers)

5. Press **HOME** on the Wii Remote → the HOME menu opens normally; returning
   resumes the app (the tape continues where it left off).
6. From HOME choose **Close Software** → the console returns to the Wii U Menu.
   - Log should end with `shutdown: done - exiting process now`.
7. Relaunch, hold **`-`** for ~1.2 s → app quits the same way ("Hold - to quit"
   is shown on the title screen).

## 3. Aiming (IR)

8. Point the remote at the TV: the crosshair follows smoothly across the whole
   screen. It holds through brief sensor dropouts instead of blinking.
9. Cover the sensor bar / point away: the crosshair hides after ~0.25 s.
10. If the crosshair moves **opposite** vertically: set `aim_flip_y=1` in
    `<SD>:/wiiu/apps/Klaxon-ActionMax/data/config.txt`.

## 4. Game flow

11. TITLE → pull B → **SELECT GAME** (5 cards) → aim at a card → B.
12. **INTRO**: the Action Max intro tape plays with "GET READY!" pulsing, then
    cuts to gameplay **exactly at the sting's end** (not into gameplay footage
    — that was the old bug).
13. While a tape plays, **tap `-`** to leave it and return to TITLE.
    (There is no game-type screen and no ammo limit: the real Action Max has
    neither. A tape simply plays to its end.)

## 5. Hit detection (the core)

14. Shooting white circles as they flash → red LED **SCORE** in the bottom-left
    box increments; the hit light (top-left) flashes; good-hit sound plays.
15. Shooting non-targets during a flip → bad-hit sound, score decreases.
16. Let a full tape run to the end → GAME OVER screen with stats; B returns to
    TITLE; the **LAST GAME** panel now shows those numbers.

## 6. Audio/Video

17. Voice/music stays in sync with the picture for a full ~15 min tape.
18. No crackling when firing during loud tape audio (the mixer scales voices).
19. Video is decoded at 720x480 and uploaded at 360x240, then stretched: expect
    a soft picture, not macroblock garbage. If the screen is black while audio
    plays, check the log for `video[...]` lines — they report the sampled
    luminance of every 30th frame.

## 7. Hash / integrity path

20. Corrupt one tape file on the SD (change a few bytes) → reboot → that game's
    card shows as unavailable while the others still work.

## Notes

* Multiple remotes: any connected Wiimote can shoot; channel 0 wins the aim.
* If audio is silent: look for `SDL_OpenAudioDevice FAILED` in `klaxon.log`.
  The app runs silent instead of crashing (report it either way).
* Anything that fails: send `klaxon.log` plus the step number.
