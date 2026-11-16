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
`c380aa3` — "In-memory linear resample for 44.1k SFX - no SDL_mixer decode on
hardware at all". Built, staged, copied to SD (F:) and Cemu at 19:54, pushed.
**Never booted** — no log exists from this build on either target
(the Cemu log on disk is 19:09, from the previous build).

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
   fell back into the mixer-lock decoder. Current fix: in-memory linear
   resample 44.1 → device rate at boot.

## Next action
Boot `c380aa3` on hardware, then read `F:\wiiu\apps\Klaxon-ActionMax\klaxon.log`.
Success signature, in order:
```
decodeSounds: begin (device 48000 Hz)
res: quickload ok ... 48000 Hz      (x8)
sounds decoded: gun=.. good=.. bad=.. over=.. ready=.. jingle=.. splash=..   (all non-zero)
splash jingle playing
run: begin (state=0)
-> TITLE
alive: frame N state=X underruns=.. dropped=..    (every 5 s)
```
If it dies, the log ends at a named line — that name is the next bug.

## Other artifacts
- `research/singe-restudy.md` — config table + media tree analysis (Singe-IA).
- `research/hitlogic-verification.md` — hit-detection harness results.
- `HARDWARE_TEST.md` — the 20-item on-console acceptance list.
- `HASHES.md` — media hashes; `dist/`, `sd_data/`, `romfs/assets` gitignored.
