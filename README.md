<img width="1895" height="830" alt="Futuristic Blue KLAXON Wordmark" src="https://github.com/user-attachments/assets/11ad8e30-9f19-4466-ad2b-41b7bb1bf964" />

Stupid VHS game emulator(-ish) for the Wii U homebrew environment, built with SDL2 and WUT + devkitPPC (Action Max only for now!! Will eventually hopefully expand into other VHS systems.)

This is the third time I've built this thing. Attempts one and two are in a ditch somewhere. This one actually works, and more importantly, it actually *stops* when you ask it to.

# KLAXON DOESN'T EMULATE ROMS!
This is NOT a full on ROM emulator. There is no ROMFS or .wup to run. What this is emulating (Action Max) is just a FMV emulator like application that plays Action Max tape video files, aligns it with the timeline of the boundary circles to shoot, adds a counter, and just maps the Wiimote coordinates to your screen to emulate the actual game.

The actual mechanic, for the three people who care: the tape flashes a white circle in the bottom right corner of the screen (that's what the real gun's suction cup stuck to your TV was watching). Every time that circle flips black/white, the console samples the pixel under your crosshair. Same state as the circle = hit. Opposite state = you shot a friend, which deducts points, because apparently the World Peace Coalition frowns on friendly fire. This is a 1:1 port of the original Singe scripts, because I'm not smart enough to invent my own hit detection.

# What, How, and Why?
Read: https://en.wikipedia.org/wiki/Action_Max
TL;DR: Action Max is a VHS console game where you literally just shoot targets and it registers. Did I mention this runs on glorious VHS? Oh, and you can't really lose at this game. Which makes it funny. Oh, you could also aim it at a light too and it'd register. Fun fact, the Teddy Ruxpin people made this system too. That gave me nightmares, remember that yellow caterpillar thing too? I believe his name was Grubby. He was cool though. Did I mention he had a cable? Kind of reminds me of Human Centipede, but without the human centipede. Bear centipede. Thanks, I hate it.

This is just a port of the Singe/Daphne data, but just compiled into an Aroma WUHB for your own pleasure.

# SETUP (the part you actually came here for)

## What you need
- A Wii U with the Aroma environment. If you're here, you probably have this. If you don't, that's a whole other wiki page, my guy.
- A Wii Remote and a sensor bar. The IR camera is the gun's sights. No sensor bar, no aim, just vibes.
- A PC with [devkitPro](https://devkitpro.org/) installed (devkitPPC, wut, and the Wii U SDL2 / SDL2_image / SDL2_ttf / SDL2_mixer portlibs), plus ffmpeg and ffprobe somewhere in your PATH.
- About 6 GB of hard drive space for the converted tapes, and a FAT32 SD card with actual room on it.
- The ability to run a PowerShell script and wait. That's the whole skill requirement.

## Step 1: the legal dance (get the tapes yourself)
There are no tapes in this repo. Nothing copyrighted, look at me following the law for once.

Go to https://archive.org/details/singev1-actionmax and download the **Singe - IA** tree. Yes, THAT one specifically. There's a "Remux - IA" tree sitting right next to it that's smaller and tempting, and I need you to not. The Remux files are a re-upload with frame drift, and frame drift in this game means the flashing circle doesn't line up with what the scripts expect, which means your shots eat glass. The original Singe 1 rips match the script frame numbers exactly. I checked. Twice. Don't make me check a third time.

## Step 2: feed the tapes to the machine
Drop the tree where the scripts expect it (default is `E:\ACTIONMAX RESOURCES`, or pass `-SourceRoot` to the script like a person with a different drive letter):

```powershell
tools\extract_assets.ps1
tools\prepare_media.ps1
```

What just happened: the first script unpacked the emulator assets (fonts, sprites, sounds) out of `actionmax.zip`, and the second one verified every file's SHA-256 against `media/source_hashes.sha256`, then converted each tape into a single muxed MPEG-1 `.mpg` (720x480, exactly 29.97 fps, MP2 audio) that the Wii U can decode in real time, then validated the output so you don't get green macroblock soup on your TV.

If a hash check screams at you, your download is bad. Re-download it. Don't argue with the hash. The hash is always right.

## Step 3: build the thing
```powershell
tools\build_and_deploy.ps1
```

That compiles the whole thing with the devkitPro msys2 toolchain and spits out `dist\Klaxon-ActionMax\` with the `.wuhb` and all the media laid out correctly. Fair warning: devkitPro's Wii U CMake toolchain only cooperates when launched through the msys2 bash, so the script handles that nonsense for you. You're welcome.

## Step 4: SD card
Copy the ENTIRE `dist\Klaxon-ActionMax\` folder to `<SD>:\wiiu\apps\`. The whole folder. It should look like this when you're done:

```
<SD>:\wiiu\apps\Klaxon-ActionMax\Klaxon-ActionMax.wuhb
<SD>:\wiiu\apps\Klaxon-ActionMax\data\media\common\*.mpg
<SD>:\wiiu\apps\Klaxon-ActionMax\data\media\38ambushalley\tape.mpg
<SD>:\wiiu\apps\Klaxon-ActionMax\data\media\bluethunder\tape.mpg
<SD>:\wiiu\apps\Klaxon-ActionMax\data\media\hydrosub2021\tape.mpg
<SD>:\wiiu\apps\Klaxon-ActionMax\data\media\popsghostly\tape.mpg
<SD>:\wiiu\apps\Klaxon-ActionMax\data\media\sonicfury\tape.mpg
```

## Step 5: shoot stuff
Boot Aroma, hit the Klaxon tile (with the sweet custom icon), and:

| Input | Does |
|---|---|
| Point the Wiimote | That's your crosshair |
| **B** | That's your trigger |
| **A** | Also a trigger, for people with weird grips |
| Hold **-** (~1.2 s) | Quit back to the Wii U Menu, cleanly, no softlock |
| HOME | System's job, not mine |
| GamePad | It's a PadCon. It mirrors the TV. It is NOT the gun. Stop asking. |

The app checks the media on your SD card at boot (hash + size). If a game's card says MEDIA MISSING, the tape isn't there or it's wrong — it's not a crash, it's the app being polite about your file situation.

# When it inevitably breaks
There's a `klaxon.log` written right next to the `.wuhb` on your SD card every boot. Boot stages, per-game media checks, decode timings, audio underruns, all of it. If something's haunted, that file is the exorcist. Send it to me with a description of what you saw and I'll have a look.

Aiming feels upside down? Make a `config.txt` next to the `.wuhb` with `aim_flip_y=1` in it and reboot. Feeling brave? `full_media_verify=1` makes the app hash entire multi-hundred-MB files at boot, which it will do patiently while you contemplate your choices.

# Why Wii U out of all things?
Actually, this is a great question. Do you know who wakes up on a crisp 7 AM on a Saturday and thinks to themselves, "Gee, I could really go for Action Max games on the Wii U!" No one normal, of course. Mainly I'm doing this because:
1. The homebrew scene is cool
2. I just want to make something cool for it and see what I can yield
3. Did I mention it's cool?
4. I'm just bored man.

# If you're doing it on Wii U, will you port it to other niche systems?
Yeah, I guess. But not for Windows, please see the lovely folks at [Kangaroo Punch](https://kangaroopunch.com/view/ShowSoftware?id=5) for more information on Action Max emulation. I'll try to get it on the Wii standalone eventually, maybe in the future if I can get my hands on a blaster, I could just do a hackjob redneck engineering method thing. I don't know. But Wii is planned.

# Credits & legal
- Action Max hardware & tapes © Worlds of Wonder. Game scripts and emulator assets (`actionmax.zip`) by **ScottD** and the Singe/DAPHNE folks — this whole thing is a fan port of their data. No tapes are distributed here, you fetch them yourself like a law-abiding VHS enjoyer.
- [pl_mpeg](https://github.com/phoboslab/pl_mpeg) by Dominic Szablewski (MIT) does the actual MPEG-1 lifting.
- UI fonts: Orbitron, Chakra Petch, DSEG7 — all SIL OFL, licenses included in the app assets.
- License: [WTFPL](LICENSE.md), obviously. And the [AI usage disclosure](AI_USAGE_DISCLOSURE.md) is a thing that exists, for transparency.
- Third-party licences (pl_mpeg MIT, SDL zlib, the font OFLs) are collected in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
