# Klaxon — prebuilt release

This is the ready-to-run `.wuhb`. You do **not** need devkitPro, cmake or a
compiler: the file already contains every font, sprite, sound and game config.

**No tapes are included.** Those are copyrighted VHS rips, so you convert your
own — that part is on you, and it is the only fiddly step.

---

## Option A — just play (recommended)

1. **Download** `Klaxon-ActionMax.wuhb` from the assets below.
2. **Get the tapes.** Follow [Step 1 and 2 of the README](../../blob/main/README.md):
   download the **Singe - IA** tree from archive.org, then run
   ```powershell
   tools\extract_assets.ps1
   tools\prepare_media.ps1
   ```
   You need PowerShell and `ffmpeg`/`ffprobe` on your PATH for this. The scripts
   convert the tapes into the MPEG-1 files the console can decode and verify
   every hash on the way.
3. **Lay out the SD card:**
   ```
   <SD>:/wiiu/apps/Klaxon-ActionMax/Klaxon-ActionMax.wuhb
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/common/intro.mpg
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/common/menu.mpg
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/38ambushalley/tape.mpg
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/bluethunder/tape.mpg
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/hydrosub2021/tape.mpg
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/popsghostly/tape.mpg
   <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/sonicfury/tape.mpg
   ```
   (`prepare_media.ps1` leaves that tree in `sd_data\media`.)
4. Boot Aroma → Wii U Menu → **Klaxon**.

## Option B — build it yourself

Clone the repo and run `tools\build_and_deploy.ps1`; it produces the same
`.wuhb` plus the staged folder. Needs devkitPro + msys2. See the README.

---

## Verify your download

```
Klaxon-ActionMax.wuhb
6,499,724 bytes
SHA-256 2216D12DA1D1E1874130CFC6861768861D1E5B2B27F8F20FA11CBBDB6669A0B7
```

## Controls

| Input | Action |
|---|---|
| Point the Wiimote | aim |
| **B** / **A** | fire |
| Tap **`-`** | leave the current tape, back to the title |
| Hold **`-`** ~1.2 s | quit the app |
| **HOME** → Close Software | return to the Wii U Menu |

## Known issues

- The LED score occasionally reads oddly (reported 10 → 0 → 11). Every score
  change is logged to `klaxon.log`, so a report with that log is welcome.
- Video is decoded at 720x480 but uploaded at 360x240 and stretched (the Wii U
  renderer here only handles textures up to 512 px wide). Expect a soft picture,
  not a broken one.
- Only the five Action Max tapes are supported.

## Credits

Klaxon itself is WTFPL. Bundled third-party work — pl_mpeg (MIT), SDL2
(zlib), Orbitron / Chakra Petch / DSEG7 (SIL OFL) — is listed in
[THIRD_PARTY_NOTICES.md](../../blob/main/THIRD_PARTY_NOTICES.md). Action Max
hardware and tapes are © Worlds of Wonder; the Singe game scripts and emulator
assets are by ScottD and the Singe/DAPHNE contributors, and are **not**
redistributed here.
