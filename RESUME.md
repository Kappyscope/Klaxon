# Klaxon — resume state (updated 2026-11-15 ~20:05 local)

## What this is
Wii U homebrew: an Action Max **tape player / game engine** (not a real emulator).
MPEG-1 video decode (pl_mpeg) + SDL2 + SDL_mixer + ProcUI, built with devkitPro.
Five games driven by original Singe scripts: 38AmbushAlley, BlueThunder,
Hydrosub2021, PopsGhostly, SonicFury.

## Repo
- GitHub: https://github.com/Kappyscope/Klaxon.git (public)
- Local `master` tracks `origin/ground-up-rebuild` — that is the working branch.
  `origin/main` is the old stable line (92bf210).
- The remote was missing from `.git/config` after the session break; re-added
  2026-11-15 20:0x. `gh` CLI lives at `C:\Program Files\GitHub CLI\gh.exe` but is
  **not authenticated**, so PR browsing/creation needs a fresh `gh auth login`.

## Build / deploy
```powershell
# configure once
E:\Projects\NEW KLAXON ACTIONMAX WII U\tools\build_and_deploy.ps1 -ConfigureOnly

# build (must go through msys2 bash; E:\klaxon-src is a space-free junction to this repo)
& 'E:\devkitPro\msys2\usr\bin\bash.exe' -lc 'cd /e/klaxon-src && cmake --build /e/klaxon-build 2>&1 | tail -20'

# stage dist + deploy
E:\Projects\NEW KLAXON ACTIONMAX WII U\tools\build_and_deploy.ps1
Copy-Item 'dist\Klaxon-ActionMax\Klaxon-ActionMax.wuhb' 'F:\wiiu\apps\Klaxon-ActionMax\Klaxon-ActionMax.wuhb' -Force
Copy-Item 'dist\Klaxon-ActionMax\Klaxon-ActionMax.wuhb' 'C:\Users\mrsco\AppData\Roaming\Cemu\sdcard\wiiu\apps\Klaxon-ActionMax\Klaxon-ActionMax.wuhb' -Force
Remove-Item 'F:\wiiu\apps\Klaxon-ActionMax\klaxon.log' -ErrorAction SilentlyContinue
```
`E:\klaxon-build` is the existing build tree (do not reconfigure casually).

## Current HEAD
`2bbd30e` — "Own audio mixer: remove SDL_mixer entirely (device lock deadlocks
on hardware)". Built, deployed to F: + Cemu 20:09, pushed. **Awaiting first boot.**

Previous `c380aa3` booted and logged up to `res: resampled ... 44100 -> 48000 Hz`
then stopped — i.e. it died on the very next call, `Mix_QuickLoad_RAW`, the first
SDL_mixer call made after the audio callback thread was live. Every `Mix_*` entry
point takes the audio-device lock; taking it once the callback is running
deadlocks on this stack. Same root cause as the total silence.

## Hardware debug chain (root causes, in the order found)
1. freetype hangs streaming from `/vol/content` → all bundled assets read into
   memory-backed RWops first.
2. Any file read after audio/threads start can wedge → all asset I/O before
   audio, SD-first resolution, sequential chunked reads.
3. `Mix_LoadWAV` before the device opens → split sound loading: read pre-audio,
   decode post-audio.
4. **Mutex inside the SDL mixer callback** wedges the audio thread on real
   hardware → replaced with a lock-free SPSC ring (atomics only).
5. SDL_mixer WAV decode takes the mixer lock → parse RIFF + `Mix_QuickLoad_RAW`.
6. QuickLoad only accepted 48 kHz but Singe SFX are 44.1 kHz → the first effect
   fell back into the mixer-lock decoder.
7. **Every SDL_mixer call deadlocks once the audio thread is live** (device
   lock). Fix in `2bbd30e`: SDL_mixer removed entirely — plain SDL audio device,
   own callback, tape ring + 8 SFX voices mixed there, lock-free trigger queue,
   SFX decoded to raw S16 PCM. Device opens paused, unpaused only after all
   buffers exist; shutdown pauses instead of closing.

## Next action
Boot `2bbd30e`, then read `F:\wiiu\apps\Klaxon-ActionMax\klaxon.log`.
Success signature, in order:
```
decodeSounds: begin (device 48000 Hz)
res: pcm ready ... (N frames, 44100 -> 48000 Hz)      (x8)
sounds ready: gun=0 good=1 bad=2 over=3 ready=4 jingle=5 steady=6 splash=7
audio: device running (own mixer, no SDL_mixer)
splash jingle playing
run: begin (state=0)
-> TITLE
alive: frame N state=X underruns=.. dropped=.. audioPeriods=P voices=V
```
`audioPeriods` must keep climbing (≈47/s at 1024 samples / 48 kHz). If it stays
at 1, the wut SDL audio thread itself is dead → next step is native AX audio.
If it climbs but there is no sound, the mixer is fine and the problem is output
routing/volume.

## Other artifacts
- `research/singe-restudy.md` — config table + media tree analysis (Singe-IA).
- `research/hitlogic-verification.md` — hit-detection harness results.
- `HARDWARE_TEST.md` — the 20-item on-console acceptance list.
- `HASHES.md` — media hashes; `dist/`, `sd_data/`, `romfs/assets` gitignored.
