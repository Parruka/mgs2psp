---
name: mgs2-me-picture
description: MGS2 picture on the Media Engine: the gl14mp probe's measured answers (floats, speed, sharing, copy rates) and the two-core estimate; the hand-off is in mgs2-me-handoff
metadata:
  type: project
  modified: 2026-11-02T18:08:09.703Z
---

- Study: mgs2/me_picture_study.md (task #130). Probe build gl14mp (task #132), notes mgs2/me_probe_notes.md; run 1 read 2026-11-02 18:05 (task #139), section "Run 1 on your PSP"; files prof/hw_gl14mp/run1; reader tools/dev/mep/mepwin.py <log> <gl14 log> <battery.txt> (adds Count wraps: Count is 32-bit at 331.343 MHz, wraps every 12.96 s).
- Run 1 [C]: exact (434/311 = gl14, 199 digests 4534b532 = emulator, 16 shots = gl13's PSP shots), 46.7 min, OSTENT, lowest headroom ~340 KB, ME no exception or stop (162 kicks).
- FLOATS: 17 FPU ops x 67,136 pairs bit-identical on ME and main under FCR31 0x01000001: converters can move as they are.
- SPEED: ME/main time 0.96-1.0 (ALU 0.996, loads 0.975, I-miss 0.976): s = 1.0.
- SHARING (main CPU loss per step, medians): ME picture-like stream (55 MB/s) +1.2-1.5% (inside +1.39 ms); ME copy (91 MB/s each way) +4.7-5.4%; DMA sceDmacMemcpy at only 22.7 MB/s +9-14% (inside +12.6, deck +17.2). DMA is OUT for the hand-off.
- COPY RATES 2 MB alone: main memcpy 76 MB/s, main claimed-line 191, ME plain 103, ME claimed-line 146, DMA 196 (24 us min per call).
- Two-core estimate [I] on gl14 numbers: deck 149.6 -> ~82-88 ms, inside 91.3 -> ~52, call 1 58.8 -> ~38 (main keeps logic + draw pipeline + IOP + own walk + 1.5 hand-off; ME the rest of walk + picture).
- HAND-OFF: design and build order step 1 (done 2026-11-03, exact on x86) are in [[mgs2-me-handoff]]. Related: [[mgs2-me-audio]], [[mgs2-stagec]].
