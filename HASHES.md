# HASHES

SHA-256 of the original Singe Action Max files from
[archive.org/details/singev1-actionmax](https://archive.org/details/singev1-actionmax).
These are the reference hashes: run `tools/prepare_media.ps1` and it will
verify your downloads against these before converting anything. The missing
Sonic Fury audio hash (yes, it said "hash soon, have to fix audio" for months)
is finally here.

## Singe - IA tree (original Singe 1 media — use these)

### 38 Ambush Alley

VIDEO: `704ea89dd4080cc9df669f87bb2e5c7bcb62b9b9a128713e0256d001c46cfa3a`
AUDIO: `58ed77c7ec848c06850fb2fdf7e2613d59636975069f246782b820b285bad371`

### Blue Thunder

VIDEO: `556139d6d7a1eae0abb0134cc0845d78925778a4bb8492365ef34d31d8675156`
AUDIO: `8fe189a79f8fdd38fbbba457196c1655e5a8359384f1aab9a9563100d6bb4c90`

### Hydrosub: 2021

VIDEO: `a05025ac974f33b572c9ce11b99fb78fe8e5c9de6b7797a6a1187ac28c0960f6`
AUDIO: `f5222aa9f9d0d033d066e4fdce5e64ab301a8da247416208a5a44701699031fc`

### The Rescue of Pops Ghostly

VIDEO: `0551ff873bb45844ed44e55cee33e11c862dc1a4dd31cb08ed26d66485cb0bd9`
AUDIO: `769932d318c2b35ab70c8e8dfb85bfa13b4a4234f77144f477d12885c5ad7bae`

### Sonic Fury

VIDEO: `76ac9b5bf5766d9f1cbde73b89c1d35cc41979bb7755ddfbdc882d2a053e9bdf`
AUDIO: `08abc726c69f9b63b08ce6981eb7b594222be08072d47fbb2a7888368a51c788`

### Common

INTRO VIDEO: `74abfe70cd2943f24cc1243ba49281082b858acc937abdf1d510804c0f702acf`
INTRO AUDIO: `f06199ba8dc88bf198ee134a97ab838f5b64b7715c330ec46d95111c13cb8801`
MENU VIDEO:  `6d87ad4a22cd6a21d6e3081700decab39b19c292b22ba4610f6ff3198cddec0d`
MENU AUDIO:  `8663d09444546a93e3a3efe80f4560ab8a244a1f7af3551b7b14368d3cb1d975`

ASSETS (`actionmax.zip`): `68913c52abeba879b180910d00dd7d6c6c08b7207db05fb538166ddd70517fcc` — hash in
[media/source_hashes.sha256](media/source_hashes.sha256) (complete manifest of
every file in both trees, machine-readable).

## Remux - IA tree (re-encoded re-up; NOT recommended)

Those files are a re-encode with ±1–3 frame content drift vs the authored
Singe frame numbers — fine for watching, wrong for hit detection. Their hashes
live in `media/source_hashes.sha256` too, but the pipeline defaults to the
`Singe - IA` originals. Don't use the Remux tree for play.
