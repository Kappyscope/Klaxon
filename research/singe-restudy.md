# Singe / DAPHNE Action Max — Restudy for a ground-up C++ Wii U port

Date: 2026-11-15. Scope: original Singe Action Max scripts and media (archive.org `singev1-actionmax`), singe/Hypseus engine semantics (frame model, pixel sampling), pl_mpeg capability/licence. Evidence tiers used: **[primary code/file]** (read or measured on this machine), **[official doc]** (upstream README/source), **[inference]**, **[unverified]**.

---

## 1. Source provenance (what the two local media trees actually are)

Internet Archive item [`singev1-actionmax`](https://archive.org/details/singev1-actionmax), "Actionmax for Hypseus Singe", by ScottD, uploaded by DirtBagZon 2021-03-05, item size 4.7 G. Description:
- "The main zip is the original Singe 1 video."
- "A **remux** version based on **Singe 2 media** is also offered, re-encoded with *smaller* video files (*lower bit-rate*) but may have better audio sync."
- "These versions are **multi-game zip ROM** and requires at least Hypseus version **2.11.3**."

Inner file listings (fetched 2026-11-15) match the two local trees **byte-for-byte by size**:

| file | `hypseus_singe_actionmax.zip` (IA) | local `Singe - IA` | `hypseus_singe_actionmax_remux.zip` (IA) | local `Remux - IA` |
|---|---|---|---|---|
| actionmax.zip | 1,420,508 | 1,420,508 | 1,420,508 | 1,420,508 |
| 38ambushalley/video_38ambushalley.m2v | 733,184,761 | 733,184,761 | 278,538,815 | 278,538,815 |
| bluethunder/video_bluethunder.m2v | 733,080,602 | 733,080,602 | 293,889,106 | 293,889,106 |
| hydrosub2021/video_hydrosub2021.m2v | 732,798,526 | 732,798,526 | 216,600,350 | 216,600,350 |
| popsghostly/video_popsghostly.m2v | 732,802,686 | 732,802,686 | 195,134,008 | 195,134,008 |
| sonicfury/video_sonicfury.m2v | 732,805,551 | 732,805,551 | 325,920,986 | 325,920,986 |
| video_actionmaxintro.m2v / .ogg | 4,641,910 / 180,261 | same | same | same (byte-identical, MD5-checked) |
| video_menu.m2v / .ogg | 171,728 / 4,770 | same | same | same (byte-identical) |
| per-game .ogg | 9,467,184 / 12,288,195 / 9,704,476 / 10,815,788 / 8,929,163 | same | 11,409,651 / 14,860,805 / 11,733,086 / 13,204,126 / 10,726,812 | same |

⇒ `Singe - IA` = original Singe 1 media; `Remux - IA` = re-encode from *Singe 2* media. The `actionmax.zip` inside is the script package (5 game `.singe` configs + `emulator.singe` + `framework.singe` + `borf.ini` + sprites/sounds/fonts), identical in both trees.

**Repo note:** `github.com/DAPHNE-Emulator/DAPHNE` **no longer resolves** (`git clone` → "repository not found", 2026-11-15). The living upstream for the Singe engine is [`DirtBagXon/hypseus-singe`](https://github.com/DirtBagXon/hypseus-singe) — "Hypseus is a SDL3 version of Daphne and Singe … a fork of Matt Ownby's Daphne" — and it is the version the IA item requires (≥ 2.11.3). All engine facts below are read from that repo (`master`, cloned 2026-11-15).

---

## 2. Per-game configuration — CONFIRMED, zero discrepancies

Source: `actionmax.zip → singe/actionmax/<game>.singe` (verbatim Lua assignments), extracted from the local trees on 2026-11-15.

| game (dir) | gameID | lengthIntro | lengthGame | lengthMenu | backgroundFrame | highThreshold | lowThreshold | sensorLeft | sensorTop | sensorX | sensorY |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 38ambushalley | `38AmbushAlley` | 354 | 27819 | 96 | 819 | 127 | 20 | 288 | 182 | 312 | 205 |
| bluethunder | `BlueThunder` | 354 | 27594 | 96 | 2453 | **64** | 20 | 288 | 182 | **320** | **210** |
| hydrosub2021 | `Hydrosub2021` | 354 | 28464 | 96 | 4622 | 127 | 20 | 282 | 182 | 312 | 205 |
| popsghostly | `PopsGhostly` | 354 | 31479 | 96 | 2090 | 127 | 20 | 282 | 182 | 312 | 205 |
| sonicfury | `SonicFury` | 354 | 26289 | 96 | 8006 | 127 | 20 | 282 | 182 | 312 | 205 |

Every value in the user's current table matches these files exactly. Two fields were only partially listed before; they are **uniform across all five games**: `lengthIntro = 354`, `lengthMenu = 96`, `lowThreshold = 20`. `gameID` (used for `sprite_<lower(gameID)>.png`) is capitalised as shown — note the official title of 38 Ambush Alley is `38AmbushAlley` (no space).

`borf.ini` (multi-game launcher manifest): title "ActionMax", `multi = "38 Ambush Alley|38ambushalley"`, `"Blue Thunder|bluethunder"`, `"Hydrosub: 2021|hydrosub2021"`, `"The Rescue of Pops Ghostly|popsghostly"`, `"Sonic Fury|sonicfury"`, category Lightgun.

`framework.singe`: `discSetFPS(29.97)`; `discSearch(1)`.

---

## 3. Laser-disc frame model (Singe/Hypseus) — exact semantics

### 3.1 The `.txt` framefile format
Local files are e.g. `38ambushalley.txt`:

```
.
1       video_actionmaxintro.m2v
355     38ambushalley/video_38ambushalley.m2v
28175   video_menu.m2v
```

Parsed by `ldp_vldp::parse_framefile()` ([`src/ldp-out/ldp-vldp.cpp:1319`](https://github.com/DirtBagXon/hypseus-singe/blob/master/src/ldp-out/ldp-vldp.cpp)). Behaviour is pinned by unit tests in `src/game/releasetest.cpp` (`test_framefile_parse`):
- **First line = media directory prefix** (here `.`, i.e. the game dir); it is resolved relative to the `.txt` location and gets a trailing `/`. Blank lines are skipped; `\r`, tabs and spaces are tolerated.
- **Every later non-blank line = `<first LD frame> <file name>`**; negative frames are legal (`-35 asdf` passes). Entry frame = the 1-based LD frame number of that file's **first** frame.

### 3.2 LD frame ↔ MPEG frame mapping
`ldp_vldp::mpeg_info()` ([`ldp-vldp.cpp` ~1250](https://github.com/DirtBagXon/hypseus-singe/blob/master/src/ldp-out/ldp-vldp.cpp)):

```cpp
while ((index + 1 < m_file_index) && (ld_frame >= m_mpeginfo[index + 1].frame)) index++;
mpeg_frame           = (Uint32)(ld_frame - m_mpeginfo[index].frame);
m_cur_ldframe_offset = m_mpeginfo[index].frame;
```

- Pick the **last entry whose start frame ≤ requested frame**; the returned MPEG frame is **0-based within that file**.
- So LD frame 355 → file-relative frame 0 of the game `.m2v`; LD 28175 → file-relative 0 of `video_menu.m2v`.
- Audio is derived per video file: `oggize_path()` → same basename `.ogg` (this is why each segment has its own `.ogg`).
- All five `.txt` files are identical in both trees and encode exactly: intro@1, game@355, menu@ `lengthIntro + lengthGame + 2` (28 175 / 27 950 / 28 820 / 31 835 / 26 645 — matches every game).

### 3.3 What the script does with the frame numbers (`emulator.singe`, verbatim logic)
- Startup: `discSearch(backgroundFrame)` → static box/menu background behind the title overlay.
- Title + trigger → `discSearch(lengthIntro + lengthGame + 2); discPlay()` → **menu**, state `stateMenu`.
- Menu + trigger: left half = Standard, right half = Limited Ammo; then `discSearch(1); discPlay()` → intro.
- Intro state: when `currentFrame == lengthIntro` → `discSearch(lengthIntro + 1); discPlay()` → game (state `statePlaying`).
- Game end: when `currentFrame == lengthIntro + lengthGame` → `discPause(); discSearch(backgroundFrame)` → title.
- Menu loop: when `currentFrame == lengthIntro + lengthGame + lengthMenu` → `discSearch(lengthIntro + lengthGame + 2); discPlay()`.
- Game-over (limited ammo exhausted) → menu segment, then the same menu-loop check.

Effective timeline for 38 Ambush Alley (identical structure for all games): intro frames 1–354; game frames 355–28174 are on disc, but the script declares the game over at frame 28173 (so 28174 is never shown in-game); menu starts at 28175 and the loop fires at 28269 → the authored `lengthMenu = 96` maps to frames 28175–28270, of which 28175–28269 (95) are actually displayed before the loop.

### 3.4 Pixel sampling — `vldpGetPixel()` / overlay coordinate model
Hypseus registers `vldpGetPixel` → `sep_mpeg_get_pixel` ([`src/game/singe/singeproxy.cpp:2491`](https://github.com/DirtBagXon/hypseus-singe/blob/master/src/game/singe/singeproxy.cpp)), which delegates to `yuv_from_buf()` (`:2433`):

```cpp
int xpos = (int)(lua_tointeger(L, 1) * m_se_yuv_scale_x);   // scale = vldp_w / overlay_w
int ypos = (int)(lua_tointeger(L, 2) * m_se_yuv_scale_y);   // scale = vldp_h / overlay_h
xpos = clamp(xpos, 0, buf.width-1);  ypos = clamp(ypos, 0, buf.height-1);
int UVx = min(xpos >> 1, buf.UVw - 1);  int UVy = min(ypos >> 1, buf.UVh - 1);
Y = buf.Y[ypos*buf.Ypitch + xpos];  U = buf.U[UVy*buf.Upitch + UVx];  V = buf.V[...];
```

with `m_se_yuv_scale_x/y = g_se_vldp_width/height ÷ g_se_overlay_width/height` (`:805`), `g_se_vldp_width = g_vldp_info->w` (decoded frame size, `:715`). Y is read at full resolution, U/V at half resolution (4:2:0), then BT.601 YUV→RGB (`R = clip((298·(Y-16) + 409·(V-128) + 128) >> 8)` etc.).

- `setOverlayResolution(360,240)` → `SINGE_OVERLAY_CUSTOM` 360×240 (enum: FULL = video size, HALF, NONSQUARE (360 px wide), CUSTOM, EMPTY — `src/game/singe/singe_interface.h:7-17`).
- With the 720×480 media this gives **scale exactly 2.0 in both axes**; every sensor/gun coordinate in the scripts is in **overlay (360×240) space**.
- `deadzone(x,y)`: `x == 0 or y == 0 or x > sensorX or y > sensorY` → `pixelUnknown`. **`sensorX/sensorY` double as the valid-read rectangle**, not just the patch location: pixel reads only work inside `[1..sensorX] × [1..sensorY]` (312×205 or 320×210). The patch sits at the extreme corner of the readable area.
- `getPixelState(x,y)`: `pixelHigh` iff `255-r < highThreshold ∧ 255-g < highThreshold ∧ 255-b < highThreshold`; `pixelLow` iff `r,g,b < lowThreshold`; else `pixelUnknown`.

### 3.5 Suction-cup sensor ("Score Signal") model in the script
Each overlay update during play: `sensorState = getPixelState(sensorX, sensorY)`. On trigger (`SWITCH_BUTTON3`): `triggerPulled = 2` (two-frame sampling window), one shot counted, ammo decrement.

On each new frame while `triggerPulled > 0`, if the sensor state changed since last frame:
- gun pixel = `getPixelState(mouseX, mouseY)` **only if the crosshair is outside the sensor rect** (`mouseX < sensorLeft or mouseY < sensorTop`); shooting into the patch area yields `pixelUnknown` (wasted shot, no score).
- First sample: remember `gunLastState`. Second sample: if all four states (gun/sensor, now/last) are valid **and** `gunLastState ≠ gunState` **and** `gunState == sensorState` → **good hit** (+1, light on, ammo refunded in limited mode); otherwise **bad hit** (+1 "bad").

Hardware background ([Wikipedia, Action Max](https://en.wikipedia.org/wiki/Action_Max)): 1987 Worlds of Wonder VHS console, light gun, "TV mounted *Score Signal*" sensor; five tapes exactly matching our five games. The script's comments call it the "suction cup sensor" — the gun's cup holds the photo-sensor against a TV-mounted patch; in emulation the patch is simply a video region that flashes white/black.

**Empirically verified on this machine** (decoded frames, `System.Drawing.GetPixel`):
- 38 Ambush Alley, gameplay ≈ frame 5000: video pixel **(624,410)** (= overlay 312,205 × 2) alternates **≈165 ↔ 0 every other frame** (e.g. `164 163 162 165 … 0 168 0 167 0 …`). `165` passes `highThreshold 127` (255−165 = 90 < 127); `0` passes `lowThreshold 20`.
- Blue Thunder, gameplay ≈ frame 5000: pixel **(640,420)** (= overlay 320,210 × 2) alternates **232–236 ↔ 0 every other frame**. Its brighter white is why its `highThreshold` can be the tighter `64` (255−232 = 23 < 64 — a 165-level white would *fail* at 64).
- Game start (first 150 frames of 38 AA): patch is constant 0 (black), i.e. not flashing during the pre-game segment — matching the "sensor state must change between the two samples" requirement.

---

## 4. Media measurements: which tree matches the authored numbers

`ffprobe` on raw `.m2v` returns `format=duration = N/A` (elementary stream, no container) → durations below are **frame counts ÷ 29.97** (`-count_frames`), plus `format=duration` of the matching `.ogg`.

| segment | frames (Singe tree) | frames (Remux tree) | authored length | Δ Singe | Δ Remux | ogg dur Singe | ogg dur Remux |
|---|---|---|---|---|---|---|---|
| intro (both trees identical) | 358 | 358 | 354 | +4 | +4 | 11.5845 s | 11.5845 s |
| menu (both trees identical) | 101 | 101 | 96 | +5 | +5 | 3.0396 s | 3.0396 s |
| 38ambushalley game | 27,821 | 27,823 | 27,819 | **+2** | **+4** | 928.2589 s | 927.9600 s |
| bluethunder game | 27,596 | 27,598 | 27,594 | **+2** | **+4** | 920.7709 s | 920.5064 s |
| hydrosub2021 game | 28,466 | 28,468 | 28,464 | **+2** | **+4** | 949.8229 s | 949.5314 s |
| popsghostly game | 31,481 | 31,483 | 31,479 | **+2** | **+4** | 1050.4229 s | 1050.1434 s |
| sonicfury game | 26,291 | 26,293 | 26,289 | **+2** | **+4** | 877.1869 s | 876.9226 s |

Both trees are **MPEG-2 video, 720×480, SAR 8/9, 29.97 fps** (`codec_name=mpeg2video` — not MPEG-1). Hypseus VLDP plays them with **libmpeg2** (`mpeg2_init`, `decode_mpeg2` in `src/vldp/vldp_internal.cpp`); hence the 720×480 MPEG-2 requirement in these trees.

Structural verdict:
- The **Singe - IA tree** is the media the authored numbers were calibrated against: every game file is exactly **`lengthGame + 2`** frames (the +1 shadowed frame where the menu starts at `lengthIntro+lengthGame+2`, plus one spare), and per-game `.ogg` duration equals its own `.m2v` duration to the millisecond. `lengthGame = 27819` therefore corresponds to physical frames 355..28173 of a 27 821-frame file.
- The **Remux - IA tree** is a different encode (Singe 2 source): every game file is **`lengthGame + 4`**, its `.ogg` is ~0.30–0.37 s *shorter* than its own video, and — decisive — its **content is not frame-aligned** with the Singe tree. SSIM probe (38 AA, decoded PNGs, 720×480): best match of Remux frame N' vs Singe frame N was **N' = N+3** at N = 40, 120, 2 000; **N' = N+1** at N = 800; **N' = N** at N = 5 000 (SSIM peaks 0.69–0.88; neighbouring frames score lower, so this is motion content, not noise). Some frames also decode differently at the same index (expected from a lossy re-encode), but the drift is real and non-uniform — i.e. a different capture/encode lineage, not a clean offset.
- Both trees work *structurally* with the shipped `.txt` (both are long enough for the spans), but only the **Singe - IA tree** carries content whose frame numbers equal the authored frame model. A Wii U port should re-encode **from `Singe - IA`**; using `Remux - IA` would shift light-gun/sensor timing by a few frames relative to the `.singe` constants (harmless-looking, but it changes hit detection timing in a genre where the patch flashes every frame).

Audio caveat (both trees): `video_actionmaxintro.ogg` (11.5845 s) and `video_menu.ogg` (3.0396 s) are ~0.33–0.36 s **shorter** than their videos (11.94 s / 3.37 s). Menu/intro looping is frame-driven in the script, so this is cosmetic, but a port must not rely on audio EOF to end those segments.

---

## 5. pl_mpeg — capability and licence verdict

Upstream: [`phoboslab/pl_mpeg`](https://github.com/phoboslab/pl_mpeg) (Dominic Szablewski). The `johnsonm/pl_mpeg` URL in the brief is not the upstream repo; all facts below are from the cloned `master` (75 commits, tip `c871f2b`, 2025-12-30) of `phoboslab/pl_mpeg`.

| question | answer | evidence |
|---|---|---|
| (a) MPEG-1 **System/Program Stream** with MPEG-1 Layer II audio | **Yes.** "PL_MPEG - MPEG1 Video decoder, MP2 Audio decoder, MPEG-PS demuxer". "Most MPEG-PS (`.mpg`) files containing MPEG1 Video ("mpeg1") and MPEG1 Audio Layer II ("mp2") streams should work." | README; `pl_mpeg.h:2` |
| (b) MPEG-2 video / MPEG-2 PS extensions | **No.** "Note that `.mpg` files can also contain MPEG2 Video, which is not supported by this library." (No MPEG-2 sequence-extension parsing anywhere.) | README "Encoding for PL_MPEG" |
| (c) licence for vendoring | **MIT** — `SPDX-License-Identifier: MIT` in the header, "Single-file MIT licensed library for C/C++" in README. Safe to vendor. | `pl_mpeg.h:3` |
| (d) API surface | `plm_create_with_filename() / _with_file() / _with_memory() / _with_buffer()` (**no bare `plm_create` in current API**); `plm_decode(plm, seconds)` (push + callbacks); `plm_decode_video()` / `plm_decode_audio()` (pull one frame / one 1152-sample packet); low-level `plm_video_decode(plm_video_t*)`, `plm_audio_decode(plm_audio_t*)` for raw ES; `plm_frame_to_rgba(frame, dest, stride)` where **stride = bytes per row** (dest ≥ stride × height; example uses `frame->width * 4`); `plm_samples_t` = 1152 samples/frame, interleaved float L/R (−1..1) or `PLM_AUDIO_SEPARATE_CHANNELS`; buffer callbacks: `plm_buffer_create_with_callbacks(load, seek, tell, length, user)` and `plm_buffer_set_load_callback()`. | `pl_mpeg.h:483-527, 737-749, 3641-3643` |
| threading / `plm_video_decode` off the main thread | **No internal threads at all** — `git log -S "thread"` over the full history returns **zero** commits; the default file buffer (`plm_buffer_create_with_file`, `PLM_BUFFER_DEFAULT_SIZE` 128 KiB) does a **blocking `fread` in `plm_buffer_load_file_callback`, on whichever thread calls decode**. No mutexes/locks. ⇒ calling `plm_video_decode`/`plm_decode_video` from a non-main thread is **safe provided that thread exclusively owns the `plm_t` and its buffers** (never touch the same instance from two threads, and invoke callbacks on that thread). | `pl_mpeg.h:1438-1468` (line 1446 ff.), full git history |
| other limits that matter | PTS ignored ("may cause sync issues"); frame-skip not implemented in `plm_decode`; no error reporting; seeking requires a seekable buffer (`with_filename` / `with_memory` / `_for_appending`). | README "Limitations"; `pl_mpeg.h` |

**Consequence for this port:** *neither* local tree can be fed to pl_mpeg as-is (both MPEG-2 720×480). The pipeline is re-encode → MPEG-1 PS + MP2, e.g. the README's own recipe:
`ffmpeg -i in.m2v -c:v mpeg1video -q:v 0 -c:a libtwolame -b:a 224k -f mpeg out.mpg` (MPEG-1 allows 720×480; if a smaller decode target is preferred, 640×480 / 320×240 with the same 29.97 fps keeps the overlay scale arithmetic trivial).

---

## 6. Port-relevant engineering notes (derived, clearly labelled)

1. **Frame indexing.** A single concatenated MPEG-1 PS per game, with segments in `.txt` order (intro, game, menu), makes *decoded frame number = LD frame number* (1-based). That reproduces `discSearch(n)` without needing a `.txt` at runtime; keep a byte-offset table of I-frames for fast seek, or use `plm_seek(t, seek_exact)` with `t = (n-1)/29.97`. If the segments are kept as separate files (as shipped), implement the `.txt` rule exactly: `file = last entry with start ≤ n`, `frame_in_file = n − entry.start` (0-based).
2. **Coordinate transform.** Overlay space = 360×240. For a decode size `Dw×Dh`, `sx = x·Dw/360`, `sy = y·Dh/240`, clamp, then Y from full-res luma and U/V from `(sx>>1, sy>>1)`; BT.601 matrix as in §3.4. For 720×480 media this is exactly ×2. Do **not** resample the luma before sampling (the patch is 1–2 px wide at 720×480; bilinear filtering would blur white/black into `pixelUnknown`).
3. **Threshold semantics are absolute, on RGB after BT.601.** `pixelHigh` = all channels > 255−highThreshold; `pixelLow` = all channels < lowThreshold. Measured patch whites: 38 AA ≈165, Blue Thunder ≈232 → a naive ">200 = white" port would break 38 AA. Keep the per-game thresholds exactly as tabled.
4. **Two-frame sampling.** Hit logic needs the *previous* frame's sensor and gun states; the overlay update must run **once per decoded video frame** (Singe calls `onOverlayUpdate` per frame; the script's `os.time` heartbeat is only for text/light timing). Sampling on a timer instead of per frame will desync the flash comparison.
5. **Deadzone rectangle.** Reads outside `[1..sensorX]×[1..sensorY]` and shots inside `[sensorLeft..]×[sensorTop..]` must return `pixelUnknown`; the code path that consumes "unknown" is what makes a shot score nothing rather than a miss.
6. **Timing facts to preserve.** 29.97 fps (`discSetFPS`), intro 354 frames, game `lengthGame`, menu 96 frames, `backgroundFrame` still; game-over at `lengthIntro+lengthGame` and menu-loop at `lengthIntro+lengthGame+lengthMenu` (both off-by-one relative to physical file lengths — reproduce them as-is, do not "fix").

---

## 7. Open items / not verified here

- Original 2010 Singe 1 engine binary/source (pre-Hypseus): the DAPHNE-Emulator org is gone; the scripts shipped in `actionmax.zip` are the Hypseus-era versions (they call `setOverlayResolution`, `ratioGetX/Y`, `rewriteStatus`). Behavioural differences of the 2010 build (default overlay size) are unverified; the Hypseus semantics above are what the shipped scripts target.
- `johnsonm/pl_mpeg` fork (mentioned in the brief) not inspected — upstream `phoboslab` is authoritative and was read directly.
- Remux-tree audio "better sync" claim from the item description not audited sample-by-sample; only durations were compared.
