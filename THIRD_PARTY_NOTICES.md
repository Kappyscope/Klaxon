# Third-party notices

Klaxon's own code is released under the WTFPL (see `LICENSE.md`). It links
against, vendors, or ships the following third-party work. Their licences are
reproduced or pointed to here.

---

## pl_mpeg — vendored source (`src/pl_mpeg.h`)

MIT License

Copyright (c) 2017 Dominic Szablewski — https://phoboslab.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

One local modification is documented in the file itself: three anonymous
structs are tagged so they can be forward-declared.

---

## SDL2, SDL2_image, SDL2_ttf (devkitPro Wii U portlibs)

Distributed under the zlib licence:

Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Full texts ship with devkitPro (`$DEVKITPRO/portlibs/wiiu`). SDL2_image
additionally bundles libpng (zlib/libpng licence) and libjpeg (IJG licence);
SDL2_ttf bundles FreeType (FreeType licence, FTL).

---

## Fonts (shipped inside the built app, extracted by `tools/extract_assets.ps1`)

- **Orbitron** — SIL Open Font License 1.1
- **Chakra Petch** / **Saira** — SIL Open Font License 1.1
- **DSEG7** — SIL Open Font License 1.1

The OFL texts are packaged alongside the fonts in `data/assets/fonts/`
(`OFL-orbitron.txt`, `OFL-saira.txt` and friends) and are present in every
built `.wuhb`.

---

## Action Max material — NOT distributed here

- **Action Max** hardware and tapes © Worlds of Wonder (1987).
- **Game scripts, emulator assets and framework** (`actionmax.zip`, including
  `emulator.singe`, `framework.singe`, the per-game `.singe`/`.txt` scripts and
  `borf.ini`) by **ScottD** and the Singe/DAPHNE contributors.

None of that is redistributed in this repository. `tools/extract_assets.ps1`
copies those files out of the user's own `actionmax.zip` into `romfs/assets/`
and `romfs/singe/`, both of which are git-ignored. Tape video/audio is fetched
by the user from archive.org and converted locally by `tools/prepare_media.ps1`.

The hit-detection logic in `src/hit_detector.cpp` is a re-implementation of the
`emulator.singe` shot rules, with the original quoted in comments for
provenance.
