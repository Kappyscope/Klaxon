# Klaxon — the Action Max for Wii U

**Klaxon** is a ground-up Wii U homebrew emulator for the 1987 **Worlds of Wonder
Action Max** — the VHS-console where the "console" was a suction-cup sensor stuck
to your TV and the games were tapes you shot at. It plays the Singe/DAPHNE
Action Max tape data natively on a modded Wii U (Aroma), with the **Wii Remote
as the Action Max pistol** (IR aim + B trigger).

This is the third generation of Klaxon. The first two (built with Cline and
Deepseek) never fully worked — see the bottom of this file for what was wrong
and how this rebuild fixes it, piece by piece.

```
TITLE ─▶ GAME SELECT ─▶ GAME TYPE ─▶ INTRO ─▶ PLAYING ─▶ GAME OVER ─▶ TITLE
                              (Standard / Limited Ammo)
```

---

## How the real Action Max worked (and how Klaxon replicates it)

The Action Max never fired bullets. Every tape flashes a small white/black
**suction-cup patch** on the TV at a fixed spot. The pistol's suction cup
senses that patch. On every *flip* of the patch (black→white or white→black),
the console samples the light state under your aim point:

* aim pixel classified the same as the patch on the flip → **good hit**
* confident opposite classification → **bad hit** (you shot a friend)
* anything ambiguous → ignored

Klaxon ports this logic 1:1 from ScottD's original Singe scripts
(`emulator.singe`): the three-state pixel classification
(`high`/`low`/`unknown` with per-game thresholds), the playfield deadzone, the
sensor-exclusion box, and the two-flip trigger window.

The "tape" is three video segments played on one continuous 29.97 fps timeline,
exactly like the Singe/DAPHNE laser-disc model:

| segment | source file | authored length |
|---|---|---|
| intro (Action Max sting) | `video_actionmaxintro.*` | 354 frames |
| gameplay (per game)      | `video_<game>.*`          | `lengthGame` frames |
| menu loop                | `video_menu.*`            | 96 frames |

The per-game parameters (`lengthIntro/lengthGame/lengthMenu`,
`backgroundFrame`, `sensorX/sensorY/sensorLeft/sensorTop`, `highThreshold`,
`lowThreshold`) come from the original Singe `.txt` scripts and ship as
`game.cfg` inside the `.wuhb`. The intro is cut at frame 354 even though the
file carries 358 — just like Singe does it.

---

## The media (legal-ish) part — KLAXON DOES NOT SHIP TAPES

This repository contains **no copyrighted video or audio**. To play, you
convert the tape data yourself:

1. Download the Action Max Singe data from
   [archive.org/details/singev1-actionmax](https://archive.org/details/singev1-actionmax)
   (the `Singe - IA` tree — those are the original Singe 1 rips whose frame
   counts match the authored script numbers exactly).
2. Run the pipeline: `tools/prepare_media.ps1`
   * verifies every source file against `media/source_hashes.sha256`
     (SHA-256, also published in [HASHES.md](HASHES.md))
   * re-encodes each segment to one **muxed MPEG-1 System `.mpg`**
     (720×480 @ exactly 29.97 fps, MP2 48 kHz audio) — a format the Wii U can
     software-decode in real time
   * validates every output (codec, frame rate, frame count) and fails loudly
     on anything wrong
3. `tools/build_and_deploy.ps1` builds the `.wuhb` and stages
   `dist/Klaxon-ActionMax/` — drag that whole folder to `<SD>:/wiiu/apps/`.

At boot, the app **hash-checks the media on your SD card** (size + SHA-256 of
the first 256 KiB; full-file verification optional via `config.txt`) and hides
any game whose media is missing or wrong instead of crashing.

## What you need

* A Wii U with the **Aroma** environment
* [devkitPro](https://devkitpro.org/) with devkitPPC, wut, and the Wii U SDL2
  ports (SDL2, SDL2_image, SDL2_ttf, SDL2_mixer) — the same machine can build
  on Windows; see the notes in `tools/build_and_deploy.ps1`
* ffmpeg + ffprobe in PATH for the media pipeline
* a Wii Remote (sensor bar required for IR aiming)

## Build & run

```powershell
# one-time: fetch assets + convert media (from E:\ACTIONMAX RESOURCES)
tools\extract_assets.ps1 -Force
tools\prepare_media.ps1 -Force

# build + stage SD payload
tools\build_and_deploy.ps1
# copy dist\Klaxon-ActionMax\  ->  <SD>:\wiiu\apps\
```

In-game: **B** fires, IR points, hold **`-`** ~1.2 s to quit. The GamePad is a
passive mirror ("PadCon") — it is not the gun. HOME is owned by the OS, always.

First launch writes `<SD>:/wiiu/apps/Klaxon-ActionMax/klaxon.log` — if anything
behaves oddly on hardware, that file is the first thing to check (it logs boot
stages, media verification per game, decode timings, audio underruns).

---

## Why the ground-up rebuild (post-mortem of iterations 1 & 2)

* **Feed format.** Both old builds fed MPEG-2 `.m2v` files to `pl_mpeg`, which
  only decodes MPEG-1 — the picture was green macroblock garbage. The new
  pipeline re-encodes to MPEG-1 and *proves* each output (codec + fps + frame
  count gates).
* **Frame rate.** The old re-encode doubled everything to 59.94 fps and papered
  over it with a runtime `frameScale` hack. The new pipeline pins 29.97 and the
  output gate fails on any deviation, so the Singe frame numbering applies
  1:1.
* **Tape model.** The old builds treated the gameplay file as if it contained
  the intro, so "Get Ready!" showed gameplay footage and every boundary was
  offset. The new tape player models intro→game→menu as separate segments with
  authored cut points.
* **A/V sync.** The old builds raced SDL_mixer music against a wall-clock
  frame counter. The new player uses an **audio-master clock**: the decode
  thread (own core, CPU2) fills an audio ring that the SDL device drains; the
  video follows the audio, the way the tape head led the picture.
* **Hit detection.** The first port collapsed the Singe two-flip sensor
  comparison into a single-frame "is it white" test — unscorable. This rebuild
  is a line-by-line port (three-state classification, deadzone, sensor box,
  two-flip decision), verified against real decoded frames from the tapes.
* **Wrong game data.** The old build shipped four of five games with Blue
  Thunder's geometry and thresholds. The configs are now generated from the
  Singe scripts themselves.
* **Exit softlocks.** Handling HOME directly and tearing down SDL mid-transition
  softlocked the console. This rebuild never touches HOME, gates rendering on
  the foreground state, and exits through SDL's single ProcUI handshake; the
  in-app quit is hold-`-`.
* **Paths.** `sd:/` is not a Wii U path (it's `/vol/external01`); assets are
  also packed into the `.wuhb` so the app boots without an SD at all (it will
  just tell you the media is missing, per game).

## Credits & legal

* Action Max hardware & tapes © Worlds of Wonder; game scripts and emulator
  assets (`actionmax.zip`) by **ScottD** and the Singe/DAPHNE teams — this is a
  fan port of their data, no tapes are distributed here.
* [pl_mpeg](https://github.com/phoboslab/pl_mpeg) by Dominic Szablewski (MIT).
* Redesigned UI fonts: Orbitron, Chakra Petch (OFL), DSEG7 by keshikan (OFL).
* License: [WTFPL](LICENSE.md) (same as the previous Klaxon), and yes — the
  [AI usage disclosure](AI_USAGE_DISCLOSURE.md) still applies.
