#!/usr/bin/env python3
"""sensor_probe.py - find where the Action Max sensor patch actually flashes.

Decodes N gameplay frames of a converted tape and:
  1. prints min/max RGB at candidate sensor points (proportional mappings)
  2. scans an 8px grid over the whole frame, ranks pixels by brightness
     variance across frames, prints the top flashing locations
"""

import sys
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
W, H = 720, 480
FRAME = W * H * 3

game = sys.argv[1] if len(sys.argv) > 1 else "bluethunder"
nframes = int(sys.argv[2]) if len(sys.argv) > 2 else 240

tape = Path(game) if game.lower().endswith(('.mpg', '.m2v')) else \
    ROOT / "sd_data" / "media" / game / "tape.mpg"
proc = subprocess.Popen(
    ["ffmpeg", "-v", "error", "-i", str(tape), "-f", "rawvideo",
     "-pix_fmt", "rgb24", "-"],
    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

frames = []
for i in range(nframes):
    buf = proc.stdout.read(FRAME)
    if len(buf) < FRAME:
        break
    frames.append(buf)
proc.kill()
print(f"read {len(frames)} frames of {game}")

def px(buf, vx, vy):
    off = (vy * W + vx) * 3
    return buf[off], buf[off + 1], buf[off + 2]

print("\ncandidate sensor points (min..max across frames):")
cands = {
    "prop 720x480 (624,410)": (624, 410),
    "prop 720x480 (640,420)": (640, 420),
    "640-mapped    (554,410)": (554, 410),
    "640-mapped    (568,420)": (568, 420),
}
for name, (vx, vy) in cands.items():
    vals = [px(f, vx, vy) for f in frames]
    mins = tuple(min(v[c] for v in vals) for c in range(3))
    maxs = tuple(max(v[c] for v in vals) for c in range(3))
    print(f"  {name}: min={mins} max={maxs}")

print("\ntop flashing pixels (variance across frames, 8px grid):")
best = []
for vy in range(0, H, 8):
    for vx in range(0, W, 8):
        vals = [px(f, vx, vy)[0] for f in frames]  # red channel enough
        mean = sum(vals) / len(vals)
        var = sum((v - mean) ** 2 for v in vals) / len(vals)
        best.append((var, vx, vy, min(vals), max(vals)))
best.sort(reverse=True)
for var, vx, vy, lo, hi in best[:12]:
    print(f"  ({vx:4d},{vy:3d})  var={var:9.1f}  min={lo:3d} max={hi:3d}")
