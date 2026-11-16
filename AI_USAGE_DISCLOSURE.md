# AI USAGE DISCLOSURE

In order to be transparent as much as possible, this project was made with the
assistance of artificial intelligence large-language model technologies —
hosted locally, and partially outsourced to API keys or tools. Generative AI
tools were used for the following actions:

- Generating graphical branding assets (i.e., the logo font which cannot be found)
- Writing parts of code
- Debugging parts of code
- Reworking important functions of code
- Checking parts or segments of code
- Testing specific features contained in the code
- Assistance in porting systems to allow functions to work

For the ground-up rebuild (November 2026), specifically:

- The C++ runtime (tape player, hit detector, HUD, app loop) was written by an
  AI agent (AutoClaw) from a design reviewed against the original Singe
  scripts; every gameplay-critical function is a documented 1:1 port of
  ScottD's `emulator.singe`.
- The media pipeline (PowerShell + ffmpeg) was AI-written and gates itself on
  hash and frame-count verification before it will produce media.
- All source-file hashes in HASHES.md were computed locally by script and
  cross-checked against this repo's previously published values (9/9 matched).
- Build tooling quirks (msys2 cmake requirements, path conversion traps) were
  diagnosed empirically on this machine and documented in the build script.
