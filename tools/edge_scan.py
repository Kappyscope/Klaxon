#!/usr/bin/env python3
"""edge_scan.py - measure the flashing strip near the frame's right edge
across all five original Action Max tapes (the suspected suction-cup patch).
"""

import subprocess
from pathlib import Path

W, H = 720, 480
FRAME = W * H * 3
SRC = Path(r"E:\ACTIONMAX RESOURCES\Singe - IA\actionmax")

GAMES = ["bluethunder", "38ambushalley", "popsghostly", "hydrosub2021", "sonicfury"]

for name in GAMES:
    tape = SRC / name / f"video_{name}.m2v"
    proc = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-i", str(tape), "-f", "rawvideo",
         "-pix_fmt", "rgb24", "-"],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    frames = []
    for _ in range(160):
        buf = proc.stdout.read(FRAME)
        if len(buf) < FRAME:
            break
        frames.append(buf)
    proc.kill()

    def luma(f, vx, vy):
        off = (vy * W + vx) * 3
        return (f[off] + f[off + 1] + f[off + 2]) // 3

    flashing = []
    for vx in range(690, 720, 2):
        for vy in range(180, 420, 2):
            vals = [luma(f, vx, vy) for f in frames]
            lo, hi = min(vals), max(vals)
            if hi - lo > 80:
                flashing.append((vx, vy, lo, hi))

    if not flashing:
        print(f"{name}: NO flashing pixels in right-edge region")
        continue

    xs = [a for a, _, _, _ in flashing]
    ys = [b for _, b, _, _ in flashing]
    cx, cy = sum(xs) // len(xs), sum(ys) // len(ys)
    series = [luma(f, cx, cy) for f in frames[:40]]
    print(f"{name}: {len(flashing)} flashing px, bbox x[{min(xs)}-{max(xs)}] "
          f"y[{min(ys)}-{max(ys)}], center ({cx},{cy})")
    print(f"   luma series: {series}")
