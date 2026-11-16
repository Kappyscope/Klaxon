# Klaxon ground-up rebuild

Third generation of Klaxon, rebuilt from zero after both previous iterations
failed on every front (boot, exit, video, hit detection, audio). Everything
below is either verified on a real console or explicitly marked as unverified.

## What changed vs the old builds

| # | Old failure | This rebuild |
|---|---|---|
| 1 | MPEG-2 `.m2v` fed to pl_mpeg (MPEG-1-only) - green macroblock garbage | Hash-gated pipeline re-encodes every segment to **muxed MPEG-1 System `.mpg`** (720x480 @ 29.97, MP2 48 kHz), validated per file (codec, sequence-header rate code, frame count) |
| 2 | Re-encode doubled fps to 59.94, patched at runtime | Sequence-header gate pins 29.97; frame numbering is 1:1 with the scripts |
| 3 | Gameplay file treated as containing the intro | Tape model = intro - game - menu segments with the authored cut points (Singe cuts the intro at frame 354) |
| 4 | Audio drift, then a mixer that wedged the audio thread | Own mixer on a plain SDL audio device: decode thread on CPU2 fills a lock-free ring, SFX are decoded to PCM before the device opens, everything is mixed in the callback with no locks at all. **SDL_mixer is not used.** |
| 5 | Single-frame "is it white" hit test | Line-by-line port of `emulator.singe`: three-state classification, playfield deadzone, sensor exclusion, two-flip decision, score = good - bad |
| 6 | Four of five games shipped with Blue Thunder's geometry | `game.cfg` generated from the original `.singe` scripts inside `actionmax.zip`, cross-checked against the DAPHNE frame maps |
| 7 | HOME/exit softlocks | Exit verified on hardware: HOME -> Close Software returns to the Wii U Menu (see "The three landmines" below) |
| 8 | `sd:/` paths silently failed | `/vol/external01` + `/vol/content` resolution; boot-time media check hides broken games instead of crashing |
| 9 | Old Singe-era UI | Blue Klaxon identity (real wordmark asset), LED score box, card game select |

**Deliberately not carried over:** the Standard / Limited Ammo split and its
5-miss game over. That is a Singe emulator invention (`ammoCount = 5`); the real
Action Max has no ammo limit, so a tape now just plays to its end.

## The three landmines (all found on hardware, none visible in Cemu)

1. **Textures wider than 512 px.** A 700 px logo crashed the console on the
   splash; the 720 px video decoded, uploaded with `SDL_UpdateTexture == 0`, and
   never appeared. Textures are now capped at 512 px: the logo is 480 px, and
   video is downscaled to 360x240 before upload.
2. **`pl_mpeg` never writes the alpha byte.** `plm_frame_to_rgba` fills R, G and
   B only, and the Wii U pixel shader multiplies the sample by alpha - so frames
   rendered as a patchwork of opaque and transparent pixels. Alpha is now forced
   to 255 while staging.
3. **Exit.** `SYSLaunchMenu()` blocks when the OS already requested the exit
   (HOME -> Close Software), and `SDL_Quit` alone leaves ProcUI live, so the
   console sat on the menu transition forever. Shutdown now quits video, releases
   ProcUI if SDL did not own it, and calls `_exit(0)` so no destructor can keep
   the process alive.

## Verification performed

- **Media gates:** every converted segment validated (mpeg1video + mp2,
  sequence-header rate code 4, frame count vs source vs authored length).
- **Hit logic on real data:** `tools/verify_hit_logic.py` decodes converted tape
  frames and runs a 1:1 transcription of the detector.
- **Build:** cmake (msys2) -> elf2rpl -> wuhbtool -> `Klaxon-ActionMax.wuhb`
  (~6.2 MB, assets + configs + manifest inside) plus the staged SD payload.
- **Console:** boots to the splash and title; all eight SFX decode before audio
  starts; tape video and audio play; shots register and score; HOME -> Close
  Software returns to the Wii U Menu.

## Known issues

- The LED score occasionally reads oddly on hardware (reported 10 -> 0 -> 11).
  Every score change is now logged (`score: a -> b (good=.. bad=..)`) so the
  counter and the drawing can be told apart; digits use fixed 36x52 cells.
- Video is 360x240 upscaled to the 4:3 window. That is the 512 px limit, not a
  decode problem.

## Legal path

No tapes, no `.singe` scripts and no extracted Singe assets are in this repo.
`tools/extract_assets.ps1` pulls the scripts and assets out of the user's own
`actionmax.zip`; `tools/prepare_media.ps1` verifies the archive.org download
against `media/source_hashes.sha256` before converting; the app verifies SD media
at boot. Third-party licences are collected in `THIRD_PARTY_NOTICES.md`.

## Notes for review

- `src/pl_mpeg.h` is vendored upstream (MIT) with one local change: three
  anonymous structs tagged for forward declarations (commented in place).
- Build quirk, documented in `tools/build_and_deploy.ps1`: devkitPro's WiiU cmake
  refuses the Windows cmake, and the msys2 cmake must run from `bash -lc`
  (Windows-style path arguments get mangled).
- On-console checklist: `HARDWARE_TEST.md`.
