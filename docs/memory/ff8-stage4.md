---
name: ff8-stage4
description: FF8 stage 4 (2026-11-11): what FF8 draws (probe), fast exact drawing (done), b2/b3 PSP builds, L2a parked, GE renderer status (g1, g2, g3 read; stage 4 at the original look done 2026-11-11 20:08), Ffm's 2-3% picture margin
metadata:
  type: project
---

Part of [[ff8-port]], after [[ff8-stage3]]. Notes: /mnt/project-files/ff8/stage4_notes.md (probe, findings, order, §6 the fast drawing).

- Probe (s4/probe, env GPROBE): FF8 reads back DRAWN pixels made up to 20,254 frames earlier, so the exact software drawing always runs; the GE picture sits beside it. FF8 draws: no render-to-texture, no mask bits, blend modes 1 and 2, 4/8-bit textures, no lines, texture windows in menus, interlace skip only in the BIOS logo.
- FAST DRAWING DONE 2026-11-11 (checks 0 differ). b2 PSP 108.25 ms a frame; model refit (prof_ms.py, fit_model.py, psp/predict.py).
- b3 (EBOOT d255a964) on the PSP, READ 2026-11-11 14:00 (FF8\LOGS\b3; container psp/psp_b3): exact (80/80, out.txt = b2), 91.063 ms a frame avg (10.98 fps); field 62.014 (predicted 61.9), field+movie 77.656, movie 107.089, title 104.076, BIOS logo 48.062; picture draw 1.658 ms; model A 1.535 B 56.2 C 86 holds, no refit. Notes §6.3. No PSP build is out now.
- L2a (leaner generated code) PARKED in /home/claude/ff8/parked/l2a/ (README.txt); working tree = b3.
- DECIDED by Ffm 2026-11-11 12:44 (card): "GE renderer next". Shots to Ffm before the picture changes on the PSP.
- GE renderer: design, tools, findings and PSP cost in [[ff8-ge-renderer]].
- G1 ON THE PSP, READ 2026-11-11 16:40 (FF8\LOGS\g1; container psp/psp_g1; notes §7.5): exact (80/80), GE never stopped, 107.278 ms a frame (b3 91.063). Rect-only pictures (BIOS, title, menus) 0 px off on the real GE; field 1,000-1,300 of 71,680 (PPSSPP 190-280). CAUSE: the real GE samples at the pixel centre 8/16, PPSSPP's software GE at 7/16 (a +1/16 shift explains 1,539 of 1,599 listed pixels).
- G2 ON THE PSP, READ 2026-11-11 18:50 (FF8\LOGS\g2; container psp/psp_g2; notes §7.8): exact (80/80); the chip's probe: L8 T8 R9 B9 -> 8/16 as expected, blends 0-2 and dither = the PS1's (same answers as PPSSPP), blend 3 8 of 1,024 one step off. Picture: 212,501 px over 4,336 pictures, worst 481 of 71,680 (0.67%); shots 0-424 px (g1 137-1,303). Inside the margin. Time 103.666 ms a frame; g2 FASTER than g1 everywhere with the same renderer work (movies -2.1, quiet field 73.0 vs 78.4, heavy -7): cache placement luck on the real PSP [I], see [[ff8-g3]]. g3 delivered right after: [[ff8-g3]]. Pictures go into /mnt/project-files or the thread only on Ffm's word.
- G3 ON THE PSP, READ 2026-11-11 20:08 (notes §7.9, [[ff8-g3]]): exact, picture = g2's (worst 0.67%), 99.6 ms a frame; renderer over b3: movies +0.9, quiet field +6.2 (check off), busy field +10-14, title +14-20. STAGE 4 AT THE ORIGINAL LOOK IS DONE. A quiet field frame is 68 ms = 24% of the PS1's speed (generated code + helpers + runtime ~37 ms [I], exact drawing 11, renderer 5-6). NEXT (Ffm's card, "Both, speed build first"): speed builds (leaner generated code) + U1 480x272 shots for Ffm's choice; see MEMORY.md.
- PICTURE MARGIN (Ffm 2026-11-11 17:02: "dont need to be pixel exact, we can have a 2-3% margin"): the shown GE picture may differ from the exact one by up to 2-3% of a picture's pixels (my reading [I], told to Ffm 17:03: picture only, game logic and hashes stay exact). g1 on the PSP was inside it (worst 1,421 of 71,680 = 2.0%). Ffm 17:03: "if the change is not big like the shots you showed me, its ok, its small" (the 2 comparison shots of 14:45: differences that small are fine). So: no more chasing single pixels (texel ties, fl21 shadows); renderer work goes to speed; report each build's worst picture in % against the margin.
