---
name: ff8-transition
description: FF8 battle transition (swirl) slow on the PSP (Ffm 2026-11-18 17:15): causes, fixes 1-4 done and exact (w23, w24, w25/w28, w26 rt_m.c); figures; all in p9 (PGO started 20:00)
metadata:
  type: project
  modified: 2026-11-18T20:00:41.223Z
---

Ffm 17:15 "the battle transition is still realy slow, is it normal ?", 17:18 "was the same in the others builds". Answered: yes for now, fixes in p9 (after [[ff8-p8]]). Notes: stage5_notes "17:15-", "18:30-19:50", "19:50-20:05 swirl fix 4" (md5 d197fdfb after it).

**Swirl [C]:** per frame a software-drawn feedback loop (70 15-bit 32x32 quads from last frame's copy, gouraud semi quads, 128 15-bit semi strips, VRAM copy 320x224; GE route rejected, the STP bit feeds back) while the game waits for the fight's disc data: library waits 0x80044360 / 0x80046304 / 0x8004b294 call VSync(-1) ~2,191 times a frame.

**Fixes, all exact** (route, p1, p3, fx1, p4 l/e + VSUM 49, Ffm's p7 session prints + card 4691e69b + interp 72,406):
1. w23 gpu25.c: tri_time takes 15-bit textures. -7% [C model].
2. w24 gpu26.c: px16/pxu spans. -17%.
3. w25/w28 src_w/gpu28.c 714bd1eb (env LZP25): whole triangles by own row walk; P25CK 0 differ. -13%.
4. w26 recomp/rt_m.c b8d28313 (rt_l.c 888753fa + dynamic code pages, env W26 default 1): the PS1 stack (0x801EF000-) lies on the menu modules' stale code pages, so every stack store took the slow code-write path (7,348 a frame). A fixed module marks its pages only while tracked. Swirl -10.2%, WM -0.3/-0.5%, fight after -1/-2.4% [C model]. Checks ALL SAME 19:58.
Chain p8 -> now, 4 swirl windows: 475 -> 334 ms (-30%) [C model, dev, no layout].

**Left in 78300-78350 (116 ms [C model]):** drawing ~37, runtime 33, SPU 7, timers 7. Next lever, NOT p9: exact skip of the library wait loops (counter 0x8009a704, root-counter reads: hand-written skips).

p9 = these + shape 6 ([[ff8-u1]]). PGO started 20:00: scratchpad pg9/gen.sh -> rec.sh (FF8PG p4 2300-26600, FF8PF fx1 65900-79000, FF8PS swirl from st78200.st 78250-78350, all "shape 6") -> psp/vp9; log pg9/gen_run.log. Dev builds' _end is ~1.1 MB above a release's: heap figures from the release only.
