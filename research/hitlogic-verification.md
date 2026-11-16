# Hit-detection verification on real tape frames

Method: ffmpeg decodes N gameplay frames of each converted tape;
a 1:1 Python transcription of src/hit_detector.cpp (== emulator.singe)
runs over them. The simulated gun re-aims at a plausible target on
every sensor flip (sparse grid scan) - this is NOT the on-screen
experience, it verifies the *logic* on real sensor data.

| game | frames | flips | high/low | fired | good | bad | junk |
|---|---|---|---|---|---|---|---|
| 38ambushalley | 1500 | 1122 | 561/561 | 561 | 8 | 0 | 552 |
| bluethunder | 1500 | 689 | 344/345 | 345 | 10 | 0 | 334 |
| hydrosub2021 | 1500 | 695 | 347/348 | 348 | 0 | 0 | 347 |
| popsghostly | 1500 | 953 | 476/477 | 477 | 0 | 0 | 476 |
| sonicfury | 1500 | 586 | 292/294 | 294 | 7 | 0 | 286 |

VERDICT: PASS - sensor flips detected and the two-flip decision fires on real data.