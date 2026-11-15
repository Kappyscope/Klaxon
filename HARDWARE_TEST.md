# HARDWARE_TEST — Klaxon on a real Wii U

Cemu cannot exercise the ProcUI transitions or real KPAD IR, so this list is
what only your console can verify. Work top to bottom; each item says what
"pass" looks like.

## 0. Install

1. Copy `dist/Klaxon-ActionMax/` (whole folder) to `<SD>:/wiiu/apps/`.
2. Boot Aroma → Wii U Menu → the **Klaxon Action Max** tile appears with an
   icon (and later: custom splash/boot sound once the branding pass lands).
   - PASS: app tile visible; launching shows the KLAXON splash, not an endless
     loading sound.
   - FAIL = the old iteration #1 failure mode (never boots). Grab
     `klaxon.log` from the app folder.

## 1. Boot sanity

2. App opens to the KLAXON splash (~1.6 s) → TITLE with wordmark + box art
   background.
3. `klaxon.log` contains: `=== app init done (ready games: N)`, per-game
   `media ... OK` lines, `decode thread running on CPU2`.

## 2. HOME / exit (the softlock killers)

4. Press **HOME** on the Wii Remote → the HOME menu opens normally; returning
   resumes the app (video continues where it left).
5. From HOME choose **Close Software** → console returns to the Wii U Menu,
   no freeze.
6. Relaunch, hold **`-`** for ~1.2 s → app exits to the Wii U Menu cleanly.

## 3. Aiming (IR)

7. Point the remote at the TV: crosshair follows the aim 1:1 across the whole
   screen (pillarboxed video area and bars alike).
8. Cover the sensor bar / point away: crosshair hides after ~0.25 s.
9. If the crosshair moves **opposite** vertically: set `aim_flip_y=1` in
   `<SD>:/wiiu/apps/Klaxon-ActionMax/config.txt` (copy it out of the WUHB's
   packed content first — or tell me and I'll ship the file).

## 4. Game flow

10. TITLE → pull B → **SELECT GAME** (5 cards) → aim at a card → B.
11. **GAME TYPE**: menu tape plays behind two panels; aim left (Standard) or
    right (Limited Ammo) → B.
12. **INTRO**: the Action Max intro tape plays with "GET READY!" pulsing, then
    cuts to gameplay **exactly at the sting's end** (not into gameplay footage
    — that was the old bug).

## 5. Hit detection (the core)

13. Shooting white circles as they flash → red LED **SCORE** in the bottom-left
    black box increments; hit light (top-left) flashes; good-hit sound.
14. Shooting non-targets during a flip → bad-hit sound, score can decrease.
15. **Limited Ammo**: 5 bullet icons top-right; each miss removes one; good
    hits don't; at 5 misses → GAME OVER with the game-over sting.
16. Let a full game run to the end → GAME OVER screen with stats; B returns to
    TITLE; the **LAST GAME** panel now shows the numbers.

## 6. Audio/Video

17. Voice/music stays in sync with picture for a full ~15 min tape.
18. No crackling when firing during loud tape audio (SFX mix over tape).
19. Picture holds ~30 fps without tearing; `klaxon.log` decode timings
    (`segment open ... ms` lines and per-frame stats) are worth a look here —
    if `decodeMsPerFrame` is above ~33 ms, tell me: there's a half-res media
    option ready as a fallback.

## 7. Hash / integrity path

20. Corrupt one tape file on the SD (change a few bytes) → reboot app → that
    game's card shows **MEDIA MISSING**-style status and the others still work
    (boot log: `integrity check failed`).

## Notes

* Multiple remotes: any connected Wiimote can shoot; channel 0 wins the aim.
* If audio is silent: check `klaxon.log` for `Mix_OpenAudioDevice FAILED` —
  the app runs silent instead of crashing (report it either way).
* Anything that fails: send me `klaxon.log` + which step number failed.
