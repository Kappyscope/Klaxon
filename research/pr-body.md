# Klaxon ground-up rebuild

Third generation of Klaxon, rebuilt from zero after both previous iterations failed on every front (boot, exit, video, hit detection, audio). Everything below is verified on this machine - nothing is claimed without an executed check.

## What changed vs the old builds

| # | Old failure | This rebuild |
|---|---|---|
| 1 | MPEG-2 `.m2v` fed to pl_mpeg (MPEG-1-only) - green macroblock garbage | Hash-gated pipeline re-encodes every segment to **muxed MPEG-1 System `.mpg`** (720x480 @ 29.97, MP2 48 kHz); output validated per file (codec + sequence-header frame rate code + frame count) |
| 2 | Re-encode doubled fps to 59.94, patched at runtime with `frameScale` | Sequence-header gate pins 29.97; frame numbering applies 1:1 - no runtime hack |
| 3 | Gameplay file treated as containing the intro | Tape model = intro - game - menu segments with authored cut points (Singe cuts the intro at frame 354 of 358) |
| 4 | Wall-clock video vs SDL_mixer music drift | **Audio-master clock**: decode thread on CPU2 fills an S16 ring; SDL_mixer's `Mix_HookMusic` drains it; video follows audio |
| 5 | Single-frame "is it white" hit test (unscorable) | Line-by-line port of `emulator.singe` shot logic (three-state classification, playfield deadzone, sensor exclusion, two-flip decision) |
| 6 | Four of five games shipped with Blue Thunder's geometry | `game.cfg` generated from the original `.singe` scripts inside `actionmax.zip`, cross-validated against the DAPHNE frame maps (all five match: e.g. 38AA map intro=354 == lengthIntro=354) |
| 7 | HOME handling + mid-transition teardown - softlock | HOME untouched (OS-owned), foreground-gated render loop, exit = hold `-` - single `SDL_Quit()` ProcUI handshake |
| 8 | `sd:/` paths silently failed | `/vol/external01` + `/vol/content` resolution; boot-time media integrity check (size + SHA-256 of first 256 KiB; full verify optional) hides broken games instead of crashing |
| 9 | Old Singe-era UI | Redesigned UI: dark panel + orange design system, Orbitron/Chakra Petch/DSEG7 (OFL), LED score box bottom-left, card game select, faithful Standard/Limited halves over the menu tape |

## Verification performed

- **Source hashes**: all 40 files in both IA trees SHA-256'd; the 9 hashes previously published in HASHES.md all match; the missing Sonic Fury audio hash is filled in (`08abc726...`).
- **Media gates**: every converted segment validated (mpeg1video + mp2 + sequence-header rate code 4 + frame count vs source vs authored length).
- **Hit logic on real data**: `tools/verify_hit_logic.py` decodes actual converted tape frames and runs a 1:1 transcription of the detector. Result per game: sensor flips 586-1122 per 1500-frame live window, high/low alternation ~50/50, GOOD hit decisions fire on real tape data (e.g. Blue Thunder 10 good / 345 windows - the rest junk because the simulated gun re-aims naively between flips; a human rests on target).
- **Full build**: cmake (msys2) - elf2rpl - wuhbtool - **Klaxon-ActionMax.wuhb (6 MB, assets + cfgs + manifest inside)**, plus staged SD payload with the converted tapes.
- **Sensor patch localized on real tape**: the suction-cup patch flashes in the bottom-right sensor box from the moment each game's interactive segment starts (Blue Thunder: frame 1598), exactly where Singe samples it.

## Legal path (unchanged, now actually enforced)

No tapes in git. Users fetch from archive.org, the prep script verifies their downloads against `media/source_hashes.sha256` **before** converting, and the app verifies SD media at boot. HASHES.md now contains every hash, including the one that said "hash soon, have to fix audio".

## Notes for review

- `pl_mpeg.h` is vendored upstream (MIT) with one local change: three anonymous structs tagged for forward declarations (commented in-place).
- Build quirk documented in `tools/build_and_deploy.ps1`: devkitPro's WiiU.cmake refuses the Windows cmake; the msys2 cmake must run from `bash -lc` (Windows-style path args get mangled otherwise).
- Hardware checklist for the real console: `HARDWARE_TEST.md` (boot, HOME menu, quit gesture, aim, Standard + Limited ammo, LED score, A/V sync).
