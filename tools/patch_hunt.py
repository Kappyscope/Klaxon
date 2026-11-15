#!/usr/bin/env python3
"""patch_hunt.py - find pixels that alternate bright<->dark CONTINUOUSLY
(the Action Max suction-cup patch flashes every other frame for the whole
tape; target circles only flash when they appear)."""

import subprocess
import sys
from pathlib import Path

W, H = 720, 480
FRAME = W * H * 3

def analyze(tape, nframes=200):
    proc = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-i", str(tape), "-f", "rawvideo",
         "-pix_fmt", "rgb24", "-"],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    frames = []
    for _ in range(nframes):
        buf = proc.stdout.read(FRAME)
        if len(buf) < FRAME:
            break
        frames.append(buf)
    proc.kill()
    n = len(frames)
    print(f"{Path(tape).name}: {n} frames")

    results = []
    for vy in range(0, H, 4):
        for vx in range(0, W, 4):
            off = (vy * W + vx) * 3
            bright = dark = 0
            for f in frames:
                v = (f[off] + f[off + 1] + f[off + 2]) // 3
                if v > 100:
                    bright += 1
                elif v < 20:
                    dark += 1
            if bright and dark:
                persistence = min(bright, dark) / n
                if persistence > 0.15:
                    results.append((persistence, vx, vy, bright, dark))
    results.sort(reverse=True)
    for p, vx, vy, b, d in results[:15]:
        print(f"  ({vx:4d},{vy:3d})  alt={p:4.2f}  bright={b:3d}/{n} dark={d:3d}/{n}")

if __name__ == "__main__":
    src = Path(r"E:\ACTIONMAX RESOURCES\Singe - IA\actionmax")
    for game in sys.argv[1:] or ["bluethunder", "sonicfury"]:
        print(f"=== {game} (original) ===")
        analyze(src / game / f"video_{game}.m2v")
