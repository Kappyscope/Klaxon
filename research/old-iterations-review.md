# Old Iterations Review — Klaxon / Action Max Wii U (Cline & Deepseek)

**Date:** 2026-11-15
**Scope:** Read of every source file in `E:\Projects\Klaxon-ActionMax-WiiU` ("Cline iteration") and `E:\Projects\Klaxon-ActionMax-WiiU - Deepseek` ("Deepseek iteration"): `src/*.cpp`, `src/*.hpp`, `CMakeLists.txt`, all `.ps1` scripts, plus romfs/sd_data/dist layouts and build artifacts. There are no `.md` notes files in either project (verified by recursive glob).
**Baseline for comparison:** the user-provided description of the Finalbuild iteration (pl_mpeg.h MPEG-1 decode, KPAD/VPAD input, SDL2/SDL2_ttf/SDL2_image/SDL2_mixer, ProcUI-aware run loop, game.cfg files, fixed two-frame sensor-flip hit detection). Finalbuild code was **not** re-read for this review; items marked "given" rest on that description.

**Evidence tiers used:**
- **T1 (verified, strongest):** direct code/build-artifact observation in this repo (file:line cited).
- **T2 (documented intent):** comments/scripts claiming hardware behavior (e.g., "crashes the console") — author statements, not reproduced experiments.
- **T3 (inference):** my conclusions drawn from T1/T2; always labeled.

---

## 1. Relationship and timeline

- Both iterations use the **same third-party decoder**: `src/pl_mpeg.h` is byte-identical in both trees (SHA256 `3A8CB30C83C2A1147719C30FE0C8B93DA2987AA43140077C575B39AAA75FC2C9`, 141,424 bytes). pl_mpeg is *not* a Finalbuild invention — it was already the video engine in both older iterations (T1).
- Both were configured with the devkitPro `WiiU.cmake` toolchain and built on the **same evening** (T1, file timestamps):
  - Cline: build objects and CMake files dated **2026-10-03 22:38**.
  - Deepseek: CMakeCache 2026-10-03 23:55; `.elf` / `.rpx` / `.wuhb` all dated **2026-10-04 00:38**.
  - Read as inference (T3): Cline was tried first; Deepseek is the same-day rework after its shortcomings.
- Deepseek keeps Cline's overall class structure (`ActionMaxEmulator` / `VideoPlayer` / `InputManager` / `GameConfig`), file names, SDL2 stack, and the Singe `isPixelHigh` formula — it is an evolution of the same design, not a rewrite (T1).
- Both projects' `romfs/games/*/game.cfg` files are **byte-identical between the two trees** (verified by `Compare-Object`, empty diff) (T1).

---

## 2. Iteration 1 — "Cline" (`E:\Projects\Klaxon-ActionMax-WiiU`)

### Architecture (4 bullets)
1. **Single SDL2 render loop, no Cafe OS awareness.** `main.cpp` is a 16-line stub (`src/main.cpp:4-16`): construct `ActionMaxEmulator`, `init()`, `run()`. The run loop is a bare `while (running_) { SDL_PollEvent; update; render; }` (`src/emulator.cpp:87-101`) — **no ProcUI init/process/shutdown anywhere** (T1).
2. **All assets from RomFS.** Fonts/sounds/sprites/video are opened via `romfs:/...` paths baked into the code (`src/emulator.cpp:52-60`, `src/emulator.cpp:66-67`), with the full ~1.4 GB of game media packed into romfs (see romfs listing below) (T1).
3. **Hardcoded single-game session.** `loadGame()` ignores the on-disk `game.cfg` and assigns constants directly (`src/emulator.cpp:68-77`: thresholds 64/20, sensor 320,210, guard 288,182), then hardcodes `loadGame("bluethunder")` at init (`src/emulator.cpp:64`). `TITLE`/`MENU` enum states exist but are unreachable — init jumps straight to `PLAYING` (T1).
4. **Single-frame hit detection + Wiimote-only input.** One trigger → one same-frame pixel comparison of gun pixel vs. sensor pixel (`src/emulator.cpp:130-147`); no two-frame latch is ever executed. Input polls VPAD HOME + KPAD channel 0 only (`src/input_wiimote.cpp:22-62`) (T1).

### File-by-file notes

**src/main.cpp** (16 lines)
- Trivial entry point; no logging, no SD fallback, no lifetime scoping (T1).

**src/emulator.hpp / emulator.cpp**
- SDL init order: `SDL_Init(VIDEO|AUDIO)` → `TTF_Init` → `IMG_Init(PNG)` → `Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)` (`emulator.cpp:41-45`).
- Window 1280x720 at (0,0); renderer `SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC` (`emulator.cpp:47-53`). vsync is the only frame throttle (T1).
- **Config dead code:** `GameConfig` is populated from literals in `loadGame()` (`emulator.cpp:71-76`); the generated `romfs/games/*/game.cfg` (keys `gameID`, `lengthIntro`, `lengthGame`, `lengthMenu`, `backgroundFrame`, `highThreshold`, `lowThreshold`, `sensorTop`, `sensorLeft`, `sensorX`, `sensorY`) is never opened by any code (T1; grep for cfg parsing = 0 hits in both iterations).
- **Hit detection, `triggerShot()` (`emulator.cpp:130-147`):**
  - sensor pixel sampled at `(sensorX, sensorY)`; gun pixel at aim position; both in *video pixel space* — but the constants are Singe **overlay-space** (360x240) values (see Deepseek §3), so on a 720x480 video the sensor coordinate reads half the intended position (T1 + T3, coordinate math).
  - Suction-cup guard is two-sided only: `if (aimX < sensorLeft || aimY < sensorTop) gunHigh=false` (`emulator.cpp:139`) — ignores anything right/bottom of the sensor pixel inside the cup area (T1).
  - Same-frame comparison: `gunHigh && sensorHigh` → good hit (+1); `gunHigh && !sensorHigh` → bad hit (−1, floor 0) (`emulator.cpp:143-147`). Comment says "Hardware logic comparison:" but it is a single-frame approximation of the real two-frame sensor/gun latch (T1 comment, T3 interpretation).
- **Vestigial two-frame design:** header declares `triggerPulledCountdown_`, `lastSensorState_`, `lastGunState_` and a `PIXEL_UNKNOWN/LOW/HIGH` scheme (`emulator.hpp:71-72`; defines at `emulator.cpp:5-7`) — **none are ever read/written in the .cpp** (grep-verified). The two-frame latch was designed on paper and never implemented here (T1).
- Per-frame TTF cost: score text is re-rendered and a new texture created *every frame* (`emulator.cpp:181-192`) (T1).
- `EmulatorState` has `TITLE/MENU/PLAYING/GAMEOVER`, but only `PLAYING`/`GAMEOVER` are ever entered (T1).

**src/video_player.hpp / video_player.cpp**
- Uses the **high-level pl_mpeg API** (`plm_t`, `plm_create_with_filename`, audio disabled via `plm_set_audio_enabled(plm_, FALSE)`) (`video_player.cpp:27-31`) (T1).
- Build workaround comment: *"Explicitly pull in standard C headers so pl_mpeg knows what size_t and FILE are in GCC 16"* (`video_player.cpp:1-8`) — include-order fix for the header-only library on the current devkitPPC GCC (T2; plausible and harmless, but it's an author claim about the toolchain).
- Decode pacing: `targetFrameDuration_ = 1.0f/29.97f` hardcoded (`video_player.cpp:16`), accumulator loop decodes **up to unbounded frames per update** (`video_player.cpp:94-104`) — after a stall it will attempt a full catch-up burst (T1).
- **No A/V synchronization at all:** video is wall-clock paced; audio is an OGG played once via `Mix_PlayMusic` (`video_player.cpp:55-62`). No drift correction, no loop-length coupling (T1).
- Render: RGB24 (`SDL_PIXELFORMAT_RGB24`) streaming texture, full `SDL_UpdateTexture` of the decoded frame every render (`video_player.cpp:127-145`) (T1).
- `isPixelHigh()` (`video_player.cpp:132-144`) is the Singe formula verbatim: `((255 - r < threshold) && (255 - g < threshold) && (255 - b < threshold))` — "near-white within threshold on all three channels" (T1).
- Comment on load says "Load MPEG-2 Video Stream via pl_mpeg" (`video_player.cpp:25`) — misleading: pl_mpeg is an **MPEG-1** decoder (pl_mpeg.h:2, pl_mpeg.h:60, pl_mpeg.h:673). See §6 media-format finding (T1).

**src/input_wiimote.hpp / input_wiimote.cpp**
- **Bug (hardware-visible):** `KPADInit()` is called but **`KPADEnableDPD()` is never called** (`input_wiimote.cpp:17-19`). Deepseek's comment at `input_wiimote.cpp:25-26` ("Enable the IR camera (DPD) on all channels. Without this, pos.x/y stay at 0") documents that this leaves IR at 0 → aim is permanently screen-center (T1 code + T2 comment; T3 conclusion about Cline's runtime behavior).
- **Bug (crash risk):** destructor calls `KPADShutdown()` (`input_wiimote.cpp:13`). Deepseek's comment (`input_wiimote.cpp:18-19`) says KPADShutdown during Aroma teardown crashes the console (T2).
- IR mapping **inverts Y**: `normY = (1.0f - kpad.pos.y) * 0.5f` with comment "Invert Y for top-down display" (`input_wiimote.cpp:48`) — contradicted by Deepseek (`input_wiimote.cpp:61`, see §3). The two iterations disagree on KPAD's Y polarity; at most one is correct on hardware (T1, conflict documented).
- Edge/held mixing: `triggerHeld` from `kpad.hold`, but `buttonA` and wiimote HOME read from `kpad.trigger` (edge-only) (`input_wiimote.cpp:37-40, 59`) — minor inconsistency, A is effectively "pressed this frame" (T1).
- No input-loss handling: no lost-frame tolerance, no finite-value checks (T1).

**CMakeLists.txt (1,086 B)**
- Manual include/link dirs for `$DEVKITPRO/wut` and `portlibs/wiiu`; C++17; links SDL2 stack with full transitive dep chain (`modplug mpg123 vorbisfile vorbis ogg webp png jpeg z freetype harfbuzz wut m`) (T1).
- **No packaging step:** no elf2rpl/wuhbtool anywhere. The build produced 4 `.o` objects (verified: `build/CMakeFiles/Klaxon-ActionMax.dir/`), and a link recipe (`build/CMakeFiles/Klaxon-ActionMax.dir/link.txt` → `Klaxon-ActionMax.elf` with `wut.specs`), but **no `.elf`/`.rpx`/`.wuhb` exists anywhere in the tree** (recursive search, T1). So compilation succeeded; there is no evidence a runnable artifact was ever produced or kept (T3: link either never completed or the artifact was removed).

**setup_romfs.ps1 (5,294 B)**
- Asset provenance: extracts `actionmax.zip` from `Singe - IA\actionmax\` or `Remux - IA\actionmax\` (Singe Internet Archive package) and distributes `.ttf/.wav/.png` into `romfs/assets` (T1).
- **Generates** the five `romfs/games/*/game.cfg` files with per-game `lengthGame` (27000/27594/26850/25920/27120) and shared sync values (High=64, Low=20, sTop=182, sLeft=288, sX=320, sY=210) — keys copied from the Singe Action Max scripts' frame-length scheme (T1; script comment "Write hardware sync configuration").
- Also stages `games/common/video_actionmaxintro.*` and `video_menu.*` — which **no C++ in either iteration ever references** (grep for `actionmaxintro|video_menu|games/common` = 0 hits in Deepseek; same in Cline) (T1).
- Colorful error messages ("UNCLE'S RESCUE MISSION", "STOP CRYING AND CHECK THE FOLDER!") (T1).

**romfs/ (contents)**
- Complete media set: 5 game videos `.m2v` (195–326 MB each) + `.ogg` (10.7–14.9 MB), fonts (`font_led_real.ttf`, `font_bluestone.ttf`, `font_chemrea.ttf`), sounds (gunshot, goodhit, badhit, gameover, "get ready", "a steady aim is critical", actionmax jingle), sprites (crosshair, light on/off, 5 box arts, bullet, actionmax logo). Empty leftover dirs `romfs/actionmax/` and `romfs/games/common/` (T1).

---

## 3. Iteration 2 — "Deepseek" (`E:\Projects\Klaxon-ActionMax-WiiU - Deepseek`)

### Architecture (5 bullets)
1. **ProcUI-aware run loop with SD logging.** `ProcUIInit(nullptr)` before SDL touches the display (`src/emulator.cpp:147-150`), `ProcUIProcessMessages(TRUE)` every frame with EXITING / RELEASE_FOREGROUND (`ProcUIDrawDoneRelease`) / IN_BACKGROUND (delay 20 ms) handling (`emulator.cpp:369-387`), `ProcUIShutdown()` on exit (`emulator.cpp:~423`). A `KLOG` macro writes every stage to `klaxon.log` on the SD card (`emulator.cpp:7-13`; `main.cpp:9-15`) (T1).
2. **Two-tier content split: small assets in WUHB romfs, big media on SD.** `BASE_PATH = "/vol/external01/wiiu/apps/Klaxon-ActionMax/data/"` for game videos (`emulator.cpp:31, 338`); small assets load through `OpenAssetRW()`, which tries `sd:/...`, `/vol/external01/...`, `fs:/vol/external01/...` in order (`emulator.cpp:36-56`). `split_assets.ps1` moves every `.m2v/.ogg` out of romfs *"so the .wuhb fits in the Wii U's ~1GB app memory budget"* (`split_assets.ps1:1-9`) (T1).
3. **Full game-flow state machine: TITLE → MENU (5 box arts, IR-select) → INTRO → PLAYING → GAMEOVER → MENU.** Game configs hardcoded as a vector of 5 `GameConfig`s with **overlay-space** sensor coords and Singe frame lengths (`emulator.cpp:253-263`); INTRO ends at frame `lengthIntro`, playback ends at `lengthIntro + lengthGame` (`emulator.cpp:310-319, 515-533`); GAMEOVER shows stats, auto-returns to MENU after 5 s or on trigger (`emulator.cpp:535-542, 466-469`) (T1).
4. **Still single-frame hit detection**, but with corrected coordinate space: sensor coords converted from 360x240 overlay space to actual video pixels (`overlayToVideoX/Y`, `emulator.cpp:292-305, 483-489`) and a full 4-sided sensor-box guard `inSensorBox = (aimX >= sLeft && aimY >= sTop && aimX <= sX && aimY <= sY)` (`emulator.cpp:490-492`). The `lastSensorState_/lastGunState_` latch fields are still declared and still never driven (T1).
5. **Hardware-resilience patterns everywhere:** renderer fallback ACCELERATED→software (`emulator.cpp:122-131`), dark-red "first present" boot marker (`emulator.cpp:135-139`), audio init deferred after first present and fail-soft (`emulator.cpp:141-157`), dt clamp 0.1 s in both run loop and decoder (`emulator.cpp:395-397`, `video_player.cpp:117-119`), decode burst cap (T1).

### File-by-file notes

**src/main.cpp (45 lines)**
- Opens `klaxon.log` on SD (tries `/vol/external01/...` then `sd:/...`), logs `main() entered`, `init() OK -- entering run()`, `run() returned -- destructor about to run`, `clean exit`, with `fflush` after every write (`main.cpp:9-45`). Emulator lifetime scoped in a block so destructor logging is visible (T1).
- This is a purpose-built harness for chasing an on-hardware hang/crash: every lifecycle stage leaves a marker on the SD card (T1 structure, T3 purpose).

**src/emulator.hpp / emulator.cpp**
- `KLOG(...)` macro: fprintf + newline + fflush to `g_KlaxonLog` (`emulator.cpp:7-13`) (T1).
- Constants: `OVERLAY_W 360 / OVERLAY_H 240` ("Singe overlay resolution", `emulator.cpp:22-25`), `SCREEN_W/H 1280x720` (T1).
- `OpenAssetRW` prefix-fallback loader + thin wrappers `LoadFont`/`LoadWAV`/`LoadTex` using the `*_RW` SDL variants (`emulator.cpp:36-74`) (T1).
- **init() order documents a hardware lesson:** *"ProcUI: register with Cafe OS before SDL touches the display. Without this, pressing HOME on the GamePad produces an unhandled OS message and locks the whole console."* (`emulator.cpp:145-149`) (T2 — author's claim of an on-hardware freeze, consistent with Cline's missing ProcUI).
- Renderer: try `SDL_RENDERER_ACCELERATED`, fall back to flags `0` (`emulator.cpp:122-131`); first frame presented is a dark-red clear as a visible boot marker (`emulator.cpp:135-139`) (T1).
- Audio is initialized *after* the first present via `SDL_InitSubSystem(SDL_INIT_AUDIO)`, and failure only logs — the app "continuing silent" (`emulator.cpp:141-157`) (T1).
- Games vector hardcodes all five games with titles, box-art sprite names, thresholds 64/20, sensor overlay coords (320,210 / left 288, top 182), `lengthIntro=354`, per-game `lengthGame` (`emulator.cpp:253-263`) — same numbers as the generated cfg files; **`game.cfg` is still never parsed** (grep: no `ifstream`/cfg parsing anywhere) (T1).
- Cached text textures baked once at init: "Pull Trigger to Start!", "Get Ready!", "GAME OVER" (`emulator.cpp:275-283`); score LED texture rebuilt only when `score_ != lastScoreRendered_` (`emulator.cpp:734-750`) — fixes Cline's per-frame TTF cost (T1).
- `overlayToVideoX/Y` scale overlay coords by actual decoded video size with clamping (`emulator.cpp:292-305`) (T1).
- `currentGameplayFrame()` = decode frame − `lengthIntro`; `currentGameEndFrame()` = `lengthIntro + lengthGame` (`emulator.cpp:310-319`) — Singe's frame-count gating ported to a state machine (T1).
- **run() ProcUI loop (`emulator.cpp:360-423`):** `ProcUIProcessMessages(TRUE)` each frame; `EXITING` → break; `RELEASE_FOREGROUND` → `ProcUIDrawDoneRelease()`; `IN_BACKGROUND` → `SDL_Delay(20); continue;` (T1). SDL `SDL_QUIT` still polled. dt clamped to 0.1 s. Performance probes log frames 1/60/300 with elapsed ms (`emulator.cpp:413-417`) (T1).
- Input/state coupling: in MENU, IR `posX * games_.size()` picks the selected tile (`emulator.cpp:438-443`) — a quick hack mapping the whole screen width to 5 games, not the actual tile rects (T1). Trigger advances TITLE→MENU→INTRO; during PLAYING fires shots; during GAMEOVER sets `gameOverTimer_ = 999` to skip the 5 s wait (`emulator.cpp:466-469`) (T1).
- `triggerShot()` (`emulator.cpp:478-505`): converts sensor coords overlay→video, samples sensor pixel, computes `inSensorBox`, skips gun sampling inside the box; good/bad scoring identical to Cline (single-frame) with KLOG on each hit (T1).
- `update()` (`emulator.cpp:508-543`): INTRO→PLAYING at `frame >= lengthIntro`; PLAYING→GAMEOVER at `isFinished() || frame >= endFrame` + `sndGameOver_`; GAMEOVER→MENU after 5 s (T1).
- Renders: TITLE (logo, pulsing prompt, credits "An emulation of the 1987 Worlds of Wonder Action Max" / "Games & scripts by ScottD | Klaxon by homebrew", `emulator.cpp:545-600`); MENU (5 box-art tiles with glow, `emulator.cpp:601-663`); GAMEOVER (dimmed last frame + shots/good/bad/accuracy %, `emulator.cpp:665-723`); HUD (score LED box, hit-light sprite, crosshair, `emulator.cpp:725-780`) (T1).

**src/video_player.hpp / video_player.cpp**
- Switches to the **low-level pl_mpeg API**: `plm_buffer_create_with_filename` + `plm_video_create_with_buffer(buffer_, TRUE)` (decoder owns buffer), forcing `plm_video_has_header()` at load, logging decoded size + `plm_video_get_framerate()`, `plm_video_set_time(0)` (`video_player.cpp:29-59`) (T1). Benefit vs Cline: header parse errors surface at load instead of mid-decode (T3).
- **RGBA instead of RGB24:** buffer pre-filled `0xFF` alpha (`video_player.cpp:63-64`), texture `SDL_PIXELFORMAT_RGBA8888` + `SDL_BLENDMODE_NONE` (`video_player.cpp:150-152`). Same `isPixelHigh` Singe formula on RGB bytes (`video_player.cpp:170-177`) (T1). Motivation unstated in code; plausible causes are Wii U SDL2 RGB24 texture upload problems or endianness (T3, unverified).
- Decode pacing hardened: dt clamp 0.1 s; **max 3 frames decoded per update**; accumulator discarded if it exceeds 3 frames' worth — "so a hiccup doesn't snowball" (`video_player.cpp:116-137`) (T1).
- A/V sync **still absent**: OGG via `Mix_PlayMusic` once, independent of decode (`video_player.cpp:73-82`) (T1).

**src/input_wiimote.hpp / input_wiimote.cpp**
- `IR_HOLD_FRAMES = 15` ("roughly 250ms at 60 fps"): invalid/lost IR samples hold the last aim position for 15 frames, then hide the crosshair (`input_wiimote.cpp:9-15, 75-84`; `lostFrames` field in header) (T1).
- **`KPADEnableDPD((KPADChan)i)` for all 4 channels** with the comment *"Enable the IR camera (DPD) on all channels. Without this, pos.x/y stay at 0."* (`input_wiimote.cpp:24-27`) — the fix for Cline's dead IR (T1 + T2).
- **Destructor intentionally empty:** *"Intentionally empty. KPADShutdown() during Aroma teardown crashes the console; Aroma reclaims input subsystems itself."* (`input_wiimote.cpp:17-20`) — fix for Cline's crash-risk shutdown (T1 + T2).
- **Y no longer inverted:** `normY = (kpad.pos.y + 1.0f) * 0.5f` with comment *"Wii U reports Y with +1 at the top, which matches screen space (0=top), so no flip needed."* (`input_wiimote.cpp:58-62`) — direct contradiction of Cline's inversion; one of the two is wrong on hardware (T1 conflict).
- Sample validation: `std::isfinite` on both axes, tolerance window `[-0.1, 1.1]`, then clamp to [0,1] (`input_wiimote.cpp:64-76`) (T1).
- `kError == KPAD_ERROR_OK` explicit enum (vs Cline's `== 0`) (`input_wiimote.cpp:53`) (T1).
- Same `kpad.trigger`-vs-`kpad.hold` inconsistency for buttonA remains (`input_wiimote.cpp:47`) (T1).

**CMakeLists.txt (2,887 B)**
- Same include/link dir scheme, C++17; extended dependency order for SDL2_image: `brotlidec brotlicommon bz2` added, with comment *"Leaf deps (must come last, in dependency order)"* — evidence of linker-order battles (`CMakeLists.txt:36-47`) (T1).
- `set_target_properties(... SUFFIX ".elf")` and a **manual packaging pipeline** with `find_program` for `elf2rpl` and `wuhbtool` (fatal errors if missing): POST_BUILD step 1 `elf2rpl ELF → RPX`; step 2 `wuhbtool RPX + romfs → WUHB` with `--content=romfs --name="Klaxon Action Max" --short-name="ActionMax" --author="ScottD / Homebrew" --icon=sprite_actionmax.png` (`CMakeLists.txt:50-96`, comment "no devkitPro macros involved") (T1).
- Result verified on disk: `build/Klaxon-ActionMax.elf` (13.4 MB), `.rpx` (2.96 MB), `.wuhb` (4.68 MB), all 2026-10-04 00:38 (T1). The 4.7 MB WUHB confirms the small-assets-only romfs (media moved to SD).

**split_assets.ps1 (3,054 B)**
- Header comment documents the constraint: moves all `.m2v/.ogg` from `romfs/` to `sd_data/` *"so the .wuhb fits in the Wii U's ~1GB app memory budget"*; what stays in romfs: "fonts, sprites, small .wav sounds, all .cfg files" (`split_assets.ps1:1-9`); target size guidance "`.wuhb` should now be < 20 MB" (`split_assets.ps1:~87`) (T1/T2).
- `sd_data/` verified populated with the 10 game media files + 4 common files (~1.1 GB) (T1).

**build_and_deploy.ps1 (11,759 B)**
- 6-step idempotent pipeline: rebuild `dist/` → stage assets from romfs → stage media from romfs OR `sd_data` OR previous `dist` (dedup by name+size) → configure with `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/WiiU.cmake` and `E:/devkitPro/msys2/usr/bin/make.exe` → build → copy `.wuhb` into `dist/Klaxon-ActionMax/` and validate (T1).
- Sanity checks with hardware-informed thresholds: media count ≥ 12 expected in `dist/data/games/`; assets ≥ 100 KB; **"`.wuhb` is over 100 MB. Aroma will likely fail to load it."** (T1/T2).
- Final layout documented: `<SD>:\wiiu\apps\Klaxon-ActionMax\{Klaxon-ActionMax.wuhb, data\assets\..., data\games\...}` and *"A fresh klaxon.log will land at: <SD>:\wiiu\apps\Klaxon-ActionMax\klaxon.log"* (T1).
- `dist/Klaxon-ActionMax/` exists with `data/games/*/game.cfg` staged (cfg files deployed to SD **and** packed in romfs, read by neither) (T1).

---

## 4. Differences vs the Finalbuild approach (given feature list)

**What Finalbuild changed (per the given description, mapped to what the old iterations lacked):**
1. **game.cfg files actually drive config** — in both old iterations the cfg files are generated/staged but parsed by nothing; all values are hardcoded (Cline `emulator.cpp:68-77`; Deepseek `emulator.cpp:253-263`). Finalbuild's cfg-driven loader removes the code/data duplication (given vs T1).
2. **Two-frame sensor-flip hit detection** — both old iterations compare gun pixel vs sensor pixel in the *same* decoded frame (`emulator.cpp:143-147` Cline; `emulator.cpp:490-500` Deepseek). The two-frame latch fields exist in Cline's header (`emulator.hpp:71-72`) but were never implemented; Deepseek carried them as dead code too. Finalbuild implemented the latch (given).
3. **ProcUI run loop** — absent in Cline (`emulator.cpp:87-101`), introduced in Deepseek (`emulator.cpp:360-423`). Finalbuild's "ProcUI-aware run loop" is inherited from Deepseek's shape (given vs T1).
4. **MPEG-1 media** — Finalbuild's "pl_mpeg.h for MPEG-1 video decode" matches what both old iterations *intended*; the shipped media, however, is MPEG-2-syntax (§6). Finalbuild presumably pairs the decoder with genuinely MPEG-1 streams (given; not verifiable here).
5. **Video decode API:** both old iterations already used pl_mpeg (high-level in Cline, low-level in Deepseek); Finalbuild inherited this engine choice rather than introducing it (given vs T1).

**What was inherited from these two iterations into Finalbuild (visible lineage):**
- The class decomposition (`ActionMaxEmulator` / `VideoPlayer` / `InputManager` / `GameConfig`), SDL2+TTF+IMG+Mixer stack, wut/devkitPPC CMake shape, and the Singe `isPixelHigh` near-white formula (T1, same code in all iterations).
- Sensor constants (64/20 thresholds; 320,210 sensor; 288,182 guard) identical across Cline, Deepseek, both cfg generators, and Deepseek's hardcoded vector (T1) — Finalbuild's game.cfg values trace back to the same Singe-script numbers.
- SD-card `data/games/<id>/` media layout with romfs/WUHB small assets (Deepseek's split_assets/deploy design; Finalbuild's game.cfg-on-data layout matches this tree) (T1 + given).
- KPAD/VPAD input model (HOME exit, B-trigger edge detect) with Deepseek's hardware fixes presumably retained (given; Finalbuild specifics unverified here).

---

## 5. Bugs, hacks, and techniques NOT in the Finalbuild

### 5a. Worth carrying into a ground-up rewrite (present here, not in the given Finalbuild feature list)
1. **SD-card file logger** (`main.cpp:9-45` + `KLOG` macro `emulator.cpp:7-13`): every lifecycle stage and key event flushed to `klaxon.log`. The cheapest possible on-hardware diagnosis for a console where you can't see stdout. Highest-value carry-over. (T1)
2. **Multi-prefix asset loader** (`OpenAssetRW`, `emulator.cpp:36-56`): tries `sd:/`, `/vol/external01/`, `fs:/vol/external01/` transparently for every small asset. Makes the app indifferent to how Aroma mounts the SD. (T1)
3. **Decode burst cap + accumulator drop + dt clamp** (`video_player.cpp:116-137`; also run loop `emulator.cpp:395-397`): prevents a single hitch from triggering an unbounded decode catch-up burst (Cline's version `video_player.cpp:94-104` lacks exactly this and is the cautionary counter-example). (T1)
4. **Cached text textures** — bake static strings once (`emulator.cpp:275-283`), re-render the LED score only on change (`emulator.cpp:734-750`). Cheap CPU win on PPC; Cline's every-frame `TTF_RenderText_Solid` + texture create/destroy (`emulator.cpp:181-192`) is the anti-pattern. (T1)
5. **Renderer fallback ACCELERATED→software + fail-soft deferred audio init + visible first-present boot marker** (`emulator.cpp:122-157`): the app boots to *something* even when GPU/audio init misbehaves, and you can tell from the screen how far init got. (T1)
6. **IR robustness trio** (`input_wiimote.cpp:9-84`): `KPADEnableDPD` on all channels, `isfinite` + tolerance window + clamp, and hold-last-aim-for-15-frames before hiding the crosshair. (T1)
7. **Manual elf2rpl→wuhbtool packaging pipeline** (`CMakeLists.txt:50-96`) with icon/metadata, plus the **split_assets/build_and_deploy** scripts' validated dist output and WUHB-size warnings (>20 MB hint, >100 MB "Aroma will likely fail"). A complete, re-runnable deployment story. (T1)
8. **Singe frame-length gating** (`lengthIntro`/`lengthGame` → INTRO→PLAYING→GAMEOVER transitions, `emulator.cpp:310-319, 515-542`): ports the Singe scripts' frame-count semantics to a real state machine; game-over stats (shots/good/bad/accuracy %) too. Useful UX glue regardless of the hit-detection rewrite. (T1)
9. **Low-level pl_mpeg load path with forced header check** (`video_player.cpp:29-59`): surfaces "not an MPEG stream?" at load time instead of silently ending playback on the first decode miss. (T1)

### 5b. Bugs/hacks to AVOID (fixed only later, or never fixed in these iterations)
1. **Same-frame hit comparison.** Both iterations score a shot against the sensor state of the *current* frame only. The real hardware compares the gun's video pixel against the suction-cup sensor across a two-frame latch (hence Finalbuild's "fixed two-frame sensor-flip port"). The un-driven latch fields in both headers (`emulator.hpp:71-72`) show the design existed on paper twice and was implemented zero times here. (T1 + given)
2. **Config duplication.** `game.cfg` generated twice (script) + hardcoded twice (C++) + identical values in dist — four copies of the same numbers, none read from disk at runtime. (T1)
3. **Cline's two input bugs**: no `KPADEnableDPD` (IR dead at center per Deepseek's comment) and `KPADShutdown()` in the destructor (crash during Aroma teardown per Deepseek's comment). (T1 code + T2 comments)
4. **KPAD Y-polarity disagreement.** Cline inverts Y (`input_wiimote.cpp:48`); Deepseek does not (`input_wiimote.cpp:58-62`). Whichever is right, do not repeat the trial-and-error: pin this down once in the rewrite. (T1 conflict)
5. **No A/V synchronization in either iteration.** One `Mix_PlayMusic` at start; decode paced by wall clock with hardcoded 29.97 fps regardless of the stream's real framerate. Long-play drift is unbounded. Nothing in the given Finalbuild feature list mentions A/V sync either — check it. (T1; Finalbuild status unknown)
6. **MPEG-2-syntax media fed to an MPEG-1 decoder** (§6): the shipped `.m2v` files carry MPEG-2 extension start codes; pl_mpeg implements MPEG-1 only. (T1 bytes + T1 decoder self-description)
7. **Aspect distortion:** 720x480 frames copied into a full 1280x720 rect with no letterbox (`emulator.cpp:159-161` Cline; `emulator.cpp:725-727` Deepseek HUD path). (T1)
8. **Dead weight:** common intro/menu media staged but never referenced; Cline's unreachable TITLE/MENU states; Deepseek's menu selection mapping the whole screen width to 5 tiles; `buttonA` read from the edge mask instead of the hold mask (both iterations). (T1)
9. **GCC-16 include workaround** (`video_player.cpp:1-8`, both iterations): pulling stddef/stdio/stdlib/string/stdint before pl_mpeg.h. Harmless but a smell — if the rewrite compiles clean without it, drop it. (T1)

---

## 6. Media format finding (verified at byte level)

- First 16 bytes of `sd_data/games/bluethunder/video.m2v` (Deepseek; Cline's romfs copy is the same size):
  `00 00 01 B3 2D 01 E0 24 FF FF E0 18 00 00 01 B5` (T1, read via PowerShell byte dump).
  - `00 00 01 B3` = MPEG sequence header code.
  - `2D 01` → horizontal size 0x2D0 = **720**; `E0 ...` → vertical size 0x1E0 = **480** → **720x480**.
  - Frame-rate nibble 0x4 → **29.97 fps** (matches the hardcoded `1.0f/29.97f` decode target).
  - `00 00 01 B5` = **extension start code** — an MPEG-2 syntax element (MPEG-1 streams never carry B5).
- pl_mpeg's own header self-describes as "MPEG1 Video decoder" (`pl_mpeg.h:2`, `:60`, `:673`) and only skips B5 codes (`PLM_START_EXTENSION = 0xB5`, `pl_mpeg.h:2296`; "skip extension and user data", `pl_mpeg.h:3084`) without implementing MPEG-2 semantics.
- **Implication (T3, flagged not proven):** these two iterations shipped 720x480 MPEG-2-syntax video to an MPEG-1-only decoder. Whether playback worked at all depends on encoder details (frame-coded vs field-coded, quant matrices); nothing in the repo records a successful playback. This is a plausible root cause for whatever the Deepseek klaxon.log debugging was chasing, and a plausible reason Finalbuild is described as decoding *MPEG-1*. Recommend confirming what the Finalbuild build encodes its media as.
- Coordinate corollary: 720x480 is exactly 2x the 360x240 Singe overlay space — which is why Deepseek's `overlayToVideoX/Y` scaling was necessary, and why Cline's un-scaled `(320,210)` sensor read sampled the wrong pixel.

---

## 7. On-hardware testing evidence

**Cline iteration — indirect and weak.**
- Compiled: 4 `.o` objects exist; CMake link recipe targets `Klaxon-ActionMax.elf`; **no ELF/RPX/WUHB artifact anywhere in the tree** (§2 CMake notes). So it got at least as far as compilation on 2026-10-03 (T1).
- No logging infrastructure, no deployment layout, no packaging. The only hardware-adjacent comment is "clean exit back to Aroma" (`input_wiimote.cpp:22`).
- It contains two hardware-critical omissions (no ProcUI → HOME would freeze the console per Deepseek's comment; no KPADEnableDPD → IR dead per Deepseek's comment) that Deepseek explicitly documents as fixes — reading these together (T3): **Cline likely never ran correctly on hardware, or its testing never got past launch.** Confidence: medium; absence of evidence is not proof, but the successor's fix-comments point exactly at Cline's gaps.

**Deepseek iteration — strong, multi-source.**
- Full build+package artifacts on disk: `.elf` (13,397,476 B), `.rpx` (2,963,081 B), `.wuhb` (4,683,124 B), 2026-10-04 00:38 (T1).
- A complete, validated deployment folder (`dist/Klaxon-ActionMax/` with wuhb + `data/` tree) built by `build_and_deploy.ps1`, which also *expects* "a fresh klaxon.log will land at <SD>:..." — i.e., the author planned to read back hardware logs (T1).
- Purpose-built hardware-debug apparatus: SD log of every lifecycle stage (`main.cpp`), per-stage init logs with pointer values (`emulator.cpp:214-246`), frame/elapsed performance probes at frames 1/60/300 (`emulator.cpp:413-417`), dark-red first-present marker, software-renderer fallback, fail-soft audio (T1).
- Comments record three hardware-learned facts as fixes: HOME without ProcUI "locks the whole console" (`emulator.cpp:145-149`); `KPADShutdown()` "crashes the console" during Aroma teardown (`input_wiimote.cpp:18-19`); DPD must be enabled or "pos.x/y stay at 0" (`input_wiimote.cpp:24-27`). These read as post-mortems of real failures (T2).
- The renderer fallback, WUHB size warnings, and link-order fixes each encode a solved practical problem (T1/T2).
- **Not present:** the actual `klaxon.log` contents (they live on the SD card, not in the repo), so specific runtime outcomes (did a full game play through? did hits register correctly?) are unrecorded here. Confidence: high that Deepseek was run on hardware through at least the init/loop-debugging phase; low on any claim about gameplay-correctness results. (T3)

---

## 8. Open items / unverified

1. **What did klaxon.log actually say?** The log file is not in either tree. If an SD card or backup of `<SD>:\wiiu\apps\Klaxon-ActionMax\klaxon.log` exists, it would settle what the Deepseek run-through achieved. (Suggested check: search user's SD backups / any `*.log` in E:\Projects.)
2. **Did the MPEG-2-syntax media actually decode?** Byte evidence says the streams are MPEG-2 syntax; pl_mpeg claims MPEG-1 only. Encode one 720x480 B5-bearing file with the same toolchain and run it through pl_mpeg on PC to confirm/deny playback (or diff Finalbuild's media headers — if Finalbuild's .m2v lacks B5, the re-encode theory is proven).
3. **KPAD Y polarity ground truth.** Cline vs Deepseek disagree (`input_wiimote.cpp:48` vs `:58-62`). One quick hardware test with the crosshair drawn at reported IR position settles it; check what Finalbuild does first.
4. **RGB24 → RGBA8888 texture switch motivation** (`video_player.cpp:150`): no comment records why; if Finalbuild uses a different pixel format path, the Wii U SDL2 RGB24-upload theory is testable in isolation.
5. **Finalbuild verification of carried fixes:** whether Finalbuild actually retained `KPADEnableDPD`, the no-KPADShutdown destructor, the SD logger, and the decode burst cap — the given feature list doesn't mention them. A grep of the Finalbuild src for `EnableDPD`, `KLOG`, `decodedThisTick` would confirm the inheritance claims in §4/§5a.

---

### One-line summary
Cline = minimal SDL-only prototype (romfs-packed media, hardcoded config, single-frame hits, broken IR, no ProcUI, never packaged); Deepseek = the same skeleton hardened by on-hardware debugging (ProcUI loop, SD logging, WUHB/SD split, deploy pipeline, input fixes, state machine) that still skipped game.cfg parsing, A/V sync, and the two-frame hit latch — those three, plus genuinely MPEG-1 media, are exactly the gaps the Finalbuild closed.
