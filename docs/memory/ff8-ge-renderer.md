---
name: ff8-ge-renderer
description: FF8 GE renderer (stage 4): design of psp/ge_ff8.c, harness switches and tools, emulator and PSP findings, cost on the PSP and the speed plan
metadata:
  type: project
---

Part of [[ff8-stage4]]. Notes: /mnt/project-files/ff8/stage4_notes.md §7 (7.1 design, 7.2 emulator findings, 7.4 g1, 7.5 g1 on the PSP, 7.6 g2 probe). Sources in shared ff8/ref and ff8/psp (b3's versions kept as *_b3*).

- Design: ref/gpu_host.h + read-only hooks in ref/gpu.c (x86 check with hooks: hashes, per-frame, modules SAME, s4/gehk); psp/ge_ff8.c: mirror of PS1 VRAM in eDRAM 0x088000 1024x512 5551, texture pages at 0x188000 15x32 KB, palettes 8888 alpha 0/0x60/0xFF (64 slots), triangles +7/16 px and UV +0.5, rects whole pixels with UV u0 (u0+1 flipped), fitted blend factors 0:(80,7D) 1:(FF,FB) 2:revsub(FF,FF) 3:(40,F9). Since 14:30: a flat colour is multiplied into the palette on the CPU ((c5 x m)>>4, then REPLACE), GE modulate only for gouraud textures; dither matrix 2 - PS1's for mode 2 (B-F).
- GE harness: main_ff8.c cfg "ge 1", "gecheck N", "shot F", "gedump A B" (per-primitive check). Dev build psp/g1; emulator folder /home/claude/msf/PSP/GAME/FF8G (G=... sh psp/emu_run.sh ABSOLUTE_OUT 3600; ~6 min for 40,000 frames); tools psp/shot_png.py, ge_whohit.py, ge_primsum.py [-t]; runs psp/emu_g1*, dumps s4/geshots2-6.
- GE FINDINGS 2026-11-11: BUG FIXED: at a new list the cached blend state -1 meant "off", so Disable(BLEND) was never sent (all the big differences). Left, understood: single pixels where u+0.5 or v+0.5 is a whole number (PS1 12-bit fixed-point slopes vs GE floats), and gouraud see-through shadows one step off (the PS1 dithers before it subtracts). Game state exact with the renderer on in every run.
- FULL CHECK after the fixes (psp/emu_g1g, 17,316 pictures): 655,936 px differ in all, worst picture 336 of 71,680; steps 1: 617,807, 2-3: 27,421, 4-7: 7,956, 8+: 2,752; BIOS/title/movies/menus 0; quiet field 64 (shadows). Notes stage4_notes.md §7; sources in shared ff8/ref and ff8/psp (b3's versions kept as *_b3*).
- PSP cost of the renderer (g1 - b3, ms a frame): BIOS/title +23-25 (CPU mirror copies of uploads), movies +3.5, quiet field +16.3, heavy field +39-53 (22-35 page conversions a frame: pages overlapping the draw buffer marked dirty by every draw); GE waits only 0-0.3. Plan: lazy mirror (show upload-only pictures from the exact VRAM, copy only before a GE draw), page dirty by used texel area, cheaper per-primitive path; a state-file start on the PSP would cut runs from 75 to ~25 min.
- Perf notes for later: present waits for all GE work (pic_wait), every CPU mirror copy waits for the GE (2.33 G px in the run, movies), lists not double-buffered; 97,497 page conversions: a texture page that overlaps the draw buffer (e.g. tp 448,0 8-bit over x 512+) is marked dirty by every draw (track the used texel area instead).
